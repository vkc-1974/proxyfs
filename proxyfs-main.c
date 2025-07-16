// File		:proxyfs-main.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:13:30 2025


// #include <linux/fs.h>
// #include <linux/namei.h>
// #include <linux/uaccess.h>
// #include <linux/pagemap.h>
// #include <linux/slab.h>
#include <linux/mount.h>
#include <linux/init.h>

//// #include <linux/cred.h>
//// #include <linux/kernel_read_file.h>

#include "proxyfs.h"

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
    return mount_nodev(fs_type, flags, data, proxyfs_fill_super_block);
}


// FS description
static struct file_system_type proxyfs_type = {
    .owner      = THIS_MODULE,
    .name       = MODULE_NAME,
    .mount      = proxyfs_mount,
    .kill_sb    = kill_anon_super,
};

static int __init proxyfs_init(void)
{
    pr_info("%s: %s: init\n",
            MODULE_NAME,
            __FUNCTION__);
    return register_filesystem(&proxyfs_type);
}

static void __exit proxyfs_exit(void)
{
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
