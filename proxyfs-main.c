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

// Get lower super block from proxy file
static struct super_block *proxyfs_lower_sb(const struct super_block *sb) {
    return ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb;
}

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
