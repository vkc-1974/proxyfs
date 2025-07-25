// File		:proxyfs-super-block.c
// Author	:Victor Kovalevich
// Created	:Wed Jul 16 00:11:45 2025
#include <linux/namei.h>
#include "proxyfs.h"

int proxyfs_fill_super_block(struct super_block *sb,
                             void *data,
                             int silent)
{
    struct super_block *lower_sb;
    struct inode *inode;
    struct inode *lower_inode;
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
    sb->s_fs_info = kmalloc(sizeof(struct proxyfs_sb_info), GFP_KERNEL);
    ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb = lower_sb;
    sb->s_magic = PROXYFS_MAGIC;
    sb->s_op = &proxyfs_super_ops;
    sb->s_flags = lower_sb->s_flags;
    sb->s_maxbytes = lower_sb->s_maxbytes;
    sb->s_blocksize = lower_sb->s_blocksize;
    sb->s_blocksize_bits = lower_sb->s_blocksize_bits;
    // Create root inode
    lower_inode = lower_root.dentry->d_inode;
    // Note: `struct proxyfs_inode` is allocated by the call below
    inode = new_inode(sb);
    if (!inode) {
        return -ENOMEM;
    }
    inode->i_ino = lower_inode->i_ino;
    ((struct proxyfs_inode *)inode)->lower_inode = lower_inode;
    sb->s_root = d_make_root(inode);
    proxyfs_init_dentry_ops(sb->s_root);

    return sb->s_root ? 0 : -ENOMEM;
}
