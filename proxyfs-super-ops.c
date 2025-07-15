// File		:proxyfs-super-ops.c
// Author	:Victor Kovalevich
// Created	:Tue Jul 15 03:00:42 2025
#include "proxyfs.h"
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/quota.h>
#include <linux/writeback.h>

// Get super block of underlying FS
static struct super_block *proxyfs_lower_sb(const struct super_block *sb) {
    return ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb;
}

// alloc_inode()
static struct inode *proxyfs_alloc_inode(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    if (sb == NULL) {
        return NULL;
    }
    struct proxyfs_inode_info *info;
    if ((info = kmalloc(sizeof(struct proxyfs_inode_info), GFP_KERNEL)) == NULL) {
        return NULL;
    }
    info->lower_inode = NULL;
    return &info->vfs_inode;
}

// destroy_inode()
static void proxyfs_destroy_inode(struct inode *inode) {
    PROXYFS_DEBUG("\n");
    if (inode == NULL) {
        return;
    }
    struct proxyfs_inode_info *info = container_of(inode, struct proxyfs_inode_info, vfs_inode);
    if (info->lower_inode) {
        // Decrement of refcount of underlying FS's inode (or even release it at all)
        iput(info->lower_inode);
        info->lower_inode = NULL;
    }
    kfree(info);
}

// free_inode()
static void proxyfs_free_inode(struct inode *inode) {
    PROXYFS_DEBUG("\n");
    if (inode == NULL) {
        return;
    }
    struct proxyfs_inode_info *info = container_of(inode, struct proxyfs_inode_info, vfs_inode);
    if (info->lower_inode) {
        // TBD: ???
        // // Decrement of refcount of underlying FS's inode (or even release it at all)
        // iput(info->lower_inode);
        // info->lower_inode = NULL;
    }
    kfree(info);
}

// dirty_inode()
static void proxyfs_dirty_inode(struct inode *inode,
                                int flags) {
    PROXYFS_DEBUG("flags=%d\n", flags);
    struct super_block *lower_sb = proxyfs_lower_sb(inode->i_sb);
    if (lower_sb->s_op && lower_sb->s_op->dirty_inode) {
        lower_sb->s_op->dirty_inode(inode, flags);
    }
}

// write_inode()
static int proxyfs_write_inode(struct inode *inode,
                               struct writeback_control *wbc) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(inode->i_sb);
    if (lower_sb->s_op && lower_sb->s_op->write_inode) {
        return lower_sb->s_op->write_inode(inode, wbc);
    }
    return 0;
}

// drop_inode()
static int proxyfs_drop_inode(struct inode *inode) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(inode->i_sb);
    if (lower_sb->s_op && lower_sb->s_op->drop_inode) {
        return lower_sb->s_op->drop_inode(inode);
    }
    return 0;
}

// evict_inode()
static void proxyfs_evict_inode(struct inode *inode) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(inode->i_sb);
    if (lower_sb->s_op && lower_sb->s_op->evict_inode) {
        lower_sb->s_op->evict_inode(inode);
    }
}

// put_super()
static void proxyfs_put_super(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->put_super) {
        lower_sb->s_op->put_super(lower_sb);
    }
}

// sync_fs()
static int proxyfs_sync_fs(struct super_block *sb,
                           int wait) {
    PROXYFS_DEBUG("wait=%d\n", wait);
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->sync_fs) {
        return lower_sb->s_op->sync_fs(lower_sb, wait);
    }
    return 0;
}

// freeze_super()
static int proxyfs_freeze_super(struct super_block *sb,
                                enum freeze_holder who) {
    PROXYFS_DEBUG("who=%d\n", who);
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->freeze_super) {
        return lower_sb->s_op->freeze_super(lower_sb, who);
    }
    return 0;
}

// freeze_fs()
static int proxyfs_freeze_fs(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->freeze_fs) {
        return lower_sb->s_op->freeze_fs(lower_sb);
    }
    return 0;
}

// thaw_super()
static int proxyfs_thaw_super(struct super_block *sb,
                              enum freeze_holder who) {
    PROXYFS_DEBUG("who=%d\n", who);
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->thaw_super) {
        return lower_sb->s_op->thaw_super(lower_sb, who);
    }
    return 0;
}

// unfreeze_fs()
static int proxyfs_unfreeze_fs(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->unfreeze_fs) {
        return lower_sb->s_op->unfreeze_fs(lower_sb);
    }
    return 0;
}

// statfs()
static int proxyfs_statfs(struct dentry *dentry,
                          struct kstatfs *buf) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(dentry->d_sb);
    if (lower_sb->s_op && lower_sb->s_op->statfs) {
        return lower_sb->s_op->statfs(dentry, buf);
    }
    return -EOPNOTSUPP;
}

// remount_fs()
static int proxyfs_remount_fs(struct super_block *sb,
                              int *flags,
                              char *data) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->remount_fs) {
        return lower_sb->s_op->remount_fs(lower_sb, flags, data);
    }
    return -EOPNOTSUPP;
}

// umount_begin()
static void proxyfs_umount_begin(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->umount_begin) {
        lower_sb->s_op->umount_begin(lower_sb);
    }
}

#ifdef CONFIG_QUOTA
// quota_read()
static ssize_t proxyfs_quota_read(struct super_block *sb,
                                  int type,
                                  char *data,
                                  size_t len,
                                  loff_t off) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->quota_read) {
        return lower_sb->s_op->quota_read(lower_sb, type, data, len, off);
    }
    return -EOPNOTSUPP;
}

// quota_write()
static ssize_t proxyfs_quota_write(struct super_block *sb,
                                   int type,
                                   const char *data,
                                   size_t len,
                                   loff_t off) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->quota_write) {
        return lower_sb->s_op->quota_write(lower_sb, type, data, len, off);
    }
    return -EOPNOTSUPP;
}

// get_dquots()
static struct dquot __rcu **proxyfs_get_dquots(struct inode *inode) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(inode->i_sb);
    if (lower_sb->s_op && lower_sb->s_op->get_dquots) {
        return lower_sb->s_op->get_dquots(inode);
    }
    return NULL;
}
#endif

// nr_cached_objects()
static long proxyfs_nr_cached_objects(struct super_block *sb,
                                      struct shrink_control *sc) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->nr_cached_objects) {
        return lower_sb->s_op->nr_cached_objects(lower_sb, sc);
    }
    return 0;
}

// free_cached_objects()
static long proxyfs_free_cached_objects(struct super_block *sb,
                                        struct shrink_control *sc) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->free_cached_objects) {
        return lower_sb->s_op->free_cached_objects(lower_sb, sc);
    }
    return 0;
}

// shutdown()
static void proxyfs_shutdown(struct super_block *sb) {
    PROXYFS_DEBUG("\n");
    struct super_block *lower_sb = proxyfs_lower_sb(sb);
    if (lower_sb->s_op && lower_sb->s_op->shutdown) {
        lower_sb->s_op->shutdown(lower_sb);
    }
}

// show_options()
static int proxyfs_show_options(struct seq_file *seq,
                                struct dentry *root) {
    PROXYFS_DEBUG("\n");
    seq_printf(seq, ",proxyfs=1");
    return 0;
}

// show_devname()
static int proxyfs_show_devname(struct seq_file *seq,
                                struct dentry *root) {
    PROXYFS_DEBUG("\n");
    seq_printf(seq, "proxyfs");
    return 0;
}

// show_path()
static int proxyfs_show_path(struct seq_file *seq,
                             struct dentry *root) {
    PROXYFS_DEBUG("\n");
    seq_printf(seq, "/ (via proxyfs)");
    return 0;
}

// show_stats()
static int proxyfs_show_stats(struct seq_file *seq, struct dentry *root) {
    PROXYFS_DEBUG("\n");
    seq_printf(seq, "ProxyFS statistics: (no real stats, proxy only)\n");
    return 0;
}

const struct super_operations proxyfs_super_ops = {
    // struct inode *(*alloc_inode)(struct super_block *sb);
    .alloc_inode        = proxyfs_alloc_inode,
    // void (*destroy_inode)(struct inode *);
    .destroy_inode      = proxyfs_destroy_inode,
    // void (*free_inode)(struct inode *);
    .free_inode         = proxyfs_free_inode,
    // void (*dirty_inode) (struct inode *, int flags);
    .dirty_inode        = proxyfs_dirty_inode,
    // int (*write_inode) (struct inode *, struct writeback_control *wbc);
    .write_inode        = proxyfs_write_inode,
    // int (*drop_inode) (struct inode *);
    .drop_inode         = proxyfs_drop_inode,
    // void (*evict_inode) (struct inode *);
    .evict_inode        = proxyfs_evict_inode,
    // void (*put_super) (struct super_block *);
    .put_super          = proxyfs_put_super,
    // int (*sync_fs)(struct super_block *sb, int wait);
    .sync_fs            = proxyfs_sync_fs,
    // int (*freeze_super) (struct super_block *, enum freeze_holder who);
    .freeze_super       = proxyfs_freeze_super,
    // int (*freeze_fs) (struct super_block *);
    .freeze_fs          = proxyfs_freeze_fs,
    // int (*thaw_super) (struct super_block *, enum freeze_holder who);
    .thaw_super         = proxyfs_thaw_super,
    // int (*unfreeze_fs) (struct super_block *);
    .unfreeze_fs        = proxyfs_unfreeze_fs,
    // int (*statfs) (struct dentry *, struct kstatfs *);
    .statfs             = proxyfs_statfs,
    // int (*remount_fs) (struct super_block *, int *, char *);
    .remount_fs         = proxyfs_remount_fs,
    // void (*umount_begin) (struct super_block *);
    .umount_begin       = proxyfs_umount_begin,
    // int (*show_options)(struct seq_file *, struct dentry *);
    .show_options       = proxyfs_show_options,
	// int (*show_devname)(struct seq_file *, struct dentry *);
    .show_devname       = proxyfs_show_devname,
	// int (*show_path)(struct seq_file *, struct dentry *);
    .show_path          = proxyfs_show_path,
	// int (*show_stats)(struct seq_file *, struct dentry *);
    .show_stats         = proxyfs_show_stats,
#ifdef CONFIG_QUOTA
    // ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);
    .quota_read         = proxyfs_quota_read,
	// ssize_t (*quota_write)(struct super_block *, int, const char *, size_t, loff_t);
    .quota_write        = proxyfs_quota_write,
	// struct dquot __rcu **(*get_dquots)(struct inode *);
    .get_dquots         = proxyfs_get_dquots,
#endif
	// long (*nr_cached_objects)(struct super_block *, struct shrink_control *);
    .nr_cached_objects  = proxyfs_nr_cached_objects,
	// long (*free_cached_objects)(struct super_block *, struct shrink_control *);
    .free_cached_objects = proxyfs_free_cached_objects,
	// void (*shutdown)(struct super_block *sb);
    .shutdown           = proxyfs_shutdown,
};
