// File		:proxyfs-main.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:13:30 2025
#include "proxyfs.h"

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/version.h>
#include <linux/cred.h>
#include <linux/kernel_read_file.h>

// Get lower inode from proxy inode
static struct inode *proxyfs_lower_inode(const struct inode *inode) {
    return ((struct proxyfs_inode_info *)inode)->lower_inode;
}

// Get lower super block from proxy file
static struct super_block *proxyfs_lower_sb(const struct super_block *sb) {
    return ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb;
}


// iterate hook
#if 0
static int proxyfs_iterate(struct file *file, struct dir_context *ctx) {
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->iterate) {
        return lower_file->f_op->iterate(lower_file, ctx);
    }
    return -ENOSYS;
}
#endif //  0

#if 0
static int proxyfs_setlease(struct file *file,
                            long arg,
                            struct file_lock **flp,
                            void **priv) {
    PROXYFS_DEBUG("name=%s setlease arg=%ld\n", file->f_path.dentry->d_name.name, arg);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->setlease) {
        return lower_file->f_op->setlease(lower_file, arg, flp, priv);
    }
    return -ENOSYS;
}
#endif //  0

// create hook
static int proxyfs_create(struct mnt_idmap *idmap,
                          struct inode *dir,
                          struct dentry *dentry,
                          umode_t mode, bool excl)
{
    PROXYFS_INODE_DEBUG(dentry, "create\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->create)
        return lower_inode->i_op->create(idmap, lower_inode, dentry, mode, excl);
    return -ENOSYS;
}

// lookup
static struct dentry *proxyfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    PROXYFS_INODE_DEBUG(dentry, "lookup\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->lookup) {
        return lower_inode->i_op->lookup(lower_inode, dentry, flags);
    }
    return ERR_PTR(-ENOSYS);
}

// link
static int proxyfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
    PROXYFS_INODE_DEBUG(dentry, "link\n");
    struct inode *lower_dir = proxyfs_lower_inode(dir);
    ///// struct inode *lower_inode = proxyfs_lower_inode(d_inode(old_dentry));
    if (lower_dir->i_op && lower_dir->i_op->link) {
        return lower_dir->i_op->link(old_dentry, lower_dir, dentry);
    }
    return -ENOSYS;
}

// unlink
static int proxyfs_unlink(struct inode *dir, struct dentry *dentry)
{
    PROXYFS_INODE_DEBUG(dentry, "unlink\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->unlink) {
        return lower_inode->i_op->unlink(lower_inode, dentry);
    }
    return -ENOSYS;
}

// symlink
static int proxyfs_symlink(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry,
                           const char *symname)
{
    PROXYFS_INODE_DEBUG(dentry, "symlink\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->symlink) {
        return lower_inode->i_op->symlink(idmap, lower_inode, dentry, symname);
    }
    return -ENOSYS;
}

// mkdir
static struct dentry *proxyfs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode)
{
    PROXYFS_INODE_DEBUG(dentry, "mkdir\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->mkdir) {
        return lower_inode->i_op->mkdir(idmap, lower_inode, dentry, mode);
    }
    return ERR_PTR(-ENOSYS);
}

// rmdir
static int proxyfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    PROXYFS_INODE_DEBUG(dentry, "rmdir\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->rmdir) {
        return lower_inode->i_op->rmdir(lower_inode, dentry);
    }
    return -ENOSYS;
}

// mknod
static int proxyfs_mknod(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry,
                         umode_t mode, dev_t dev)
{
    PROXYFS_INODE_DEBUG(dentry, "mknod\n");
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->mknod) {
        return lower_inode->i_op->mknod(idmap, lower_inode, dentry, mode, dev);
    }
    return -ENOSYS;
}

// rename
static int proxyfs_rename(struct mnt_idmap *idmap, struct inode *old_dir, struct dentry *old_dentry,
                          struct inode *new_dir, struct dentry *new_dentry, unsigned int flags)
{
    PROXYFS_INODE_DEBUG(old_dentry, "rename to %s\n", new_dentry->d_name.name);
    struct inode *lower_old = proxyfs_lower_inode(old_dir);
    struct inode *lower_new = proxyfs_lower_inode(new_dir);
    if (lower_old->i_op && lower_old->i_op->rename) {
        return lower_old->i_op->rename(idmap, lower_old, old_dentry, lower_new, new_dentry, flags);
    }
    return -ENOSYS;
}

// setattr
static int proxyfs_setattr(struct mnt_idmap *idmap, struct dentry *dentry, struct iattr *attr)
{
    PROXYFS_INODE_DEBUG(dentry, "setattr\n");
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->setattr) {
        return lower_inode->i_op->setattr(idmap, dentry, attr);
    }
    return -ENOSYS;
}

// getattr
static int proxyfs_getattr(struct mnt_idmap *idmap, const struct path *path, struct kstat *stat, u32 request_mask, unsigned int flags)
{
    PROXYFS_INODE_DEBUG(path->dentry, "getattr\n");
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(path->dentry));
    if (lower_inode->i_op && lower_inode->i_op->getattr) {
        return lower_inode->i_op->getattr(idmap, path, stat, request_mask, flags);
    }
    return -ENOSYS;
}

// permission
static int proxyfs_permission(struct mnt_idmap *idmap, struct inode *inode, int mask)
{
    ///// struct dentry *dentry = NULL; // Не всегда есть dentry, можно вывести только inode
    pr_info("%s: %s: inode=%lu mask=0x%x\n", MODULE_NAME, __func__, inode->i_ino, mask);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->permission) {
        return lower_inode->i_op->permission(idmap, lower_inode, mask);
    }
    return -ENOSYS;
}

static int proxyfs_update_time(struct inode *inode, int flags)
{
    pr_info("%s: %s: inode=%lu update_time\n", MODULE_NAME, __func__, inode->i_ino);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->update_time) {
        return lower_inode->i_op->update_time(lower_inode, flags);
    }
    return -ENOSYS;
}


// dir_inode_operations
static const struct inode_operations proxyfs_dir_inode_ops = {
    .create     = proxyfs_create,
    .lookup     = proxyfs_lookup,
    .link       = proxyfs_link,
    .unlink     = proxyfs_unlink,
    .symlink    = proxyfs_symlink,
    .mkdir      = proxyfs_mkdir,
    .rmdir      = proxyfs_rmdir,
    .mknod      = proxyfs_mknod,
    .rename     = proxyfs_rename,
    .setattr    = proxyfs_setattr,
    .getattr    = proxyfs_getattr,
    .permission = proxyfs_permission,
    .update_time = proxyfs_update_time,
    // etc.
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

static struct proxyfs_context_data proxyfs_context = {
    .nl_socket = NULL,
    .client_pid = {
        .counter = 0
    },
    .proc_dir = NULL,
    .buffer_pool = {
        .buffers = NULL,
        .bitmap = NULL,
        .size = 0,
        .count = 0
    },
    .running_state = {
        .counter = 0
    },
    .handler_counter = {
        .counter = 0
    }
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

struct sock* proxyfs_context_get_nl_socket(void) {
    return proxyfs_context.nl_socket;
}

int proxyfs_context_set_client_pid(const int new_client_pid) {
    int res = atomic_read(&proxyfs_context.client_pid);
    atomic_set(&proxyfs_context.client_pid, new_client_pid);
    return res;
}

int proxyfs_context_get_client_pid(void) {
    return atomic_read(&proxyfs_context.client_pid);
}

module_init(proxyfs_init);
module_exit(proxyfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Kovalevich");
MODULE_DESCRIPTION("ProxyFS over ext4 basic prototype");
