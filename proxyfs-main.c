// File		:proxyfs-main.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:13:30 2025
#include "proxyfs.h"

struct proxyfs_inode_info {
    struct inode vfs_inode;
    struct inode *lower_inode;
};

struct proxyfs_file_info {
    struct file *lower_file;
};

struct proxyfs_sb_info {
    struct super_block *lower_sb;
};

// Get lower inode from proxy inode
static struct inode *proxyfs_lower_inode(const struct inode *inode) {
    return ((struct proxyfs_inode_info *)inode)->lower_inode;
}

// Get lower file from proxy file
static struct file *proxyfs_lower_file(const struct file *file) {
    return ((struct proxyfs_file_info *)file->private_data)->lower_file;
}

// open hook
static int proxyfs_open(struct inode *inode,
                        struct file *file) {
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    struct file *lower_file;

    pr_info("%s: %s: inode %lu, name = %s\n",
            MODULE_NAME,
            __FUNCTION__,
            inode->i_ino,
            file->f_path.dentry->d_name.name);
    //
    // TBD: inform user space running monitor application `open`
    //      routine has been called

    lower_file = dentry_open(lower_inode->i_sb->s_root, O_RDONLY, current_cred());
    if (IS_ERR(lower_file)) {
        return PTR_ERR(lower_file);
    }

    file->private_data = kmalloc(sizeof(struct proxyfs_file_info), GFP_KERNEL);
    ((struct proxyfs_file_info *)file->private_data)->lower_file = lower_file;

    return 0;
}

// release (close) hook
static int proxyfs_release(struct inode *inode,
                           struct file *file) {
    pr_info("%s: %s: inode = %lu, name = %s\n",
            MODULE_NAME,
            __FUNCTION__,
            inode->i_ino,
            file->f_path.dentry->d_name.name);

    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file) {
        fput(lower_file);
    }
    kfree(file->private_data);
    return 0;
}

// read hook
static ssize_t proxyfs_read(struct file *file,
                            char __user *buf,
                            size_t count,
                            loff_t *ppos) {
    ssize_t res;
    struct file *lower_file = proxyfs_lower_file(file);

    pr_info("%s: %s: name = %s, count = %zu\n",
            MODULE_NAME,
            __FUNCTION__,
            file->f_path.dentry->d_name.name,
            count);

    res = kernel_read(lower_file, buf, count, ppos);
    return res;
}

// write hook
static ssize_t proxyfs_write(struct file *file,
                             const char __user *buf,
                             size_t count,
                             loff_t *ppos) {
    ssize_t res;
    struct file *lower_file = proxyfs_lower_file(file);

    pr_info("%s: %s: name = %s, count = %zu\n",
            MODULE_NAME,
            __FUNCTION__,
            file->f_path.dentry->d_name.name,
            count);

    res = kernel_write(lower_file, buf, count, ppos);
    return res;
}

// create hook
static int proxyfs_create(struct inode *dir,
                          struct dentry *dentry,
                          umode_t mode,
                          bool excl) {
    pr_info("%s: %s: %s\n",
            MODULE_NAME,
            __FUNCTION__,
            dentry->d_name.name);
    return vfs_create(proxyfs_lower_inode(dir),
                      dentry,
                      mode,
                      excl);
}

// unlink hook
static int proxyfs_unlink(struct inode *dir,
                          struct dentry *dentry) {
    pr_info("%s: %s: %s\n",
            MODULE_NAME,
            __FUNCTION__,
            dentry->d_name.name);
    return vfs_unlink(proxyfs_lower_inode(dir),
                      dentry,
                      NULL);
}

// mkdir hook
static int proxyfs_mkdir(struct inode *dir,
                         struct dentry *dentry,
                         umode_t mode) {
    pr_info("%s: %s: %s\n",
            MODULE_NAME,
            __FUNCTION__,
            dentry->d_name.name);
    return vfs_mkdir(proxyfs_lower_inode(dir),
                     dentry,
                     mode);
}

// rmdir hook
static int proxyfs_rmdir(struct inode *dir,
                         struct dentry *dentry) {
    pr_info("%s: %s: %s\n",
            MODULE_NAME,
            __FUNCTION__,
            dentry->d_name.name);
    return vfs_rmdir(proxyfs_lower_inode(dir),
                     dentry);
}

// rename hook
static int proxyfs_rename(struct inode *old_dir,
                          struct dentry *old_dentry,
                          struct inode *new_dir,
                          struct dentry *new_dentry,
                          unsigned int flags) {
    pr_info("%s: %s: %s -> %s\n",
            MODULE_NAME,
            __FUNCTION__,
            old_dentry->d_name.name,
            new_dentry->d_name.name);
    return vfs_rename(proxyfs_lower_inode(old_dir),
                      old_dentry,
                      proxyfs_lower_inode(new_dir),
                      new_dentry,
                      flags);
}

// file_operations
static const struct file_operations proxyfs_file_ops = {
    .open = proxyfs_open,
    .release = proxyfs_release,
    .read = proxyfs_read,
    .write = proxyfs_write,
    //
    // TBD: llseek, mmap, etc.
};

// inode_operations
static const struct inode_operations proxyfs_dir_inode_ops = {
    .create = proxyfs_create,
    .unlink = proxyfs_unlink,
    .mkdir = proxyfs_mkdir,
    .rmdir = proxyfs_rmdir,
    .rename = proxyfs_rename,
    //
    // TBD: lookup, symlink, etc.
};

static const struct super_operations proxyfs_super_ops = {
    .statfs     = simple_statfs,
    .drop_inode = generic_delete_inode,
};

// proxyfs is mounted over ext4
static int proxyfs_fill_super(struct super_block *sb,
                              void *data,
                              int silent) {
    struct super_block *lower_sb;
    struct inode *inode, *lower_inode;
    char *lower_path = (char *)data;
    struct path lower_root;

    // Looking for root node of underlying FS
    if (kern_path(lower_path, LOOKUP_FOLLOW, &lower_root)) {
        pr_err("%s: %s: cannot find lowerdir %s\n",
               MODULE_NAME,
               __FUNCTION__,
               lower_path);
        return -ENOENT;
    }
    lower_sb = lower_root.dentry->d_sb;

    // Safe lower super block
    sb->s_fs_info = kzalloc(sizeof(struct proxyfs_sb_info), GFP_KERNEL);
    ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb = lower_sb;
    sb->s_magic = PROXYFS_MAGIC;
    sb->s_op = &proxyfs_super_ops;

    // Create root inode
    lower_inode = lower_root.dentry->d_inode;
    inode = new_inode(sb);
    if (!inode) {
        return -ENOMEM;
    }
    inode->i_ino = lower_inode->i_ino;
    inode->i_op = &proxyfs_dir_inode_ops;
    inode->i_fop = &proxyfs_file_ops;
    ((struct proxyfs_inode_info *)inode)->lower_inode = lower_inode;
    sb->s_root = d_make_root(inode);

    return sb->s_root ? 0 : -ENOMEM;
}

// mount routine
static struct dentry *proxyfs_mount(struct file_system_type *fs_type,
                                    int flags,
                                    const char *dev_name,
                                    void *data)
{
    pr_info("%s: %s: mount lowerdir = %s\n",
            MODULE_NAME,
            __FUNCTION__,
            (char *)data);
    return mount_nodev(fs_type, flags, data, proxyfs_fill_super);
}


// FS description
static struct file_system_type proxyfs_type = {
    .owner      = THIS_MODULE,
    .name       = MODULE_NAME,
    .mount      = proxyfs_mount,
    .kill_sb    = kill_anon_super,
};

static int __init proxyfs_init(void) {
    pr_info("%s: %s: init\n",
            MODULE_NAME,
            __FUNCTION__);
    return register_filesystem(&proxyfs_type);
}

static void __exit proxyfs_exit(void) {
    pr_info("%s: %s: exit\n",
            MODULE_NAME,
            __FUNCTION__);
    unregister_filesystem(&proxyfs_type);
}

module_init(proxyfs_init);
module_exit(proxyfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Kovalevich");
MODULE_DESCRIPTION("ProxyFS over ext4 basic prototype");
