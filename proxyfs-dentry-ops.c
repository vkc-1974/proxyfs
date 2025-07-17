// File		:proxyfs-dentry-ops.c
// Author	:Victor Kovalevich
// Created	:Wed Jul 16 01:32:40 2025
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include "proxyfs.h"

// d_revalidate()
static int proxyfs_revalidate(struct inode *inode,
                              const struct qstr *name,
                              struct dentry *dentry,
                              unsigned int flags)
{
    // 1 = valid, 0 = invalid dentry, <0 - error
    int ret = -ENOSYS;
    PROXYFS_DEBUG("inode=" INODE_FMT ", name=" QSTR_FMT ", dentry=%pd, flags=0x%x\n",
                  INODE_ARG(inode),
                  QSTR_ARG(name),
                  dentry,
                  flags);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_revalidate) {
        ret = lower_ops->d_revalidate(lower_inode, name, lower_dentry, flags);
    }
    return ret;
}

// d_weak_revalidate()
static int proxyfs_weak_revalidate(struct dentry *dentry,
                                   unsigned int flags)
{
    // 1 = valid, 0 = invalid dentry, <0 - error
    int ret = -ENOSYS;
    PROXYFS_DEBUG("dentry=%pd, flags=0x%x\n",
                  dentry,
                  flags);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_weak_revalidate) {
        ret = lower_ops->d_weak_revalidate(lower_dentry, flags);
    }
    return ret;
}

// d_hash()
static int proxyfs_hash(const struct dentry *dentry,
                        struct qstr *name)
{
    int ret = -ENOSYS;
    PROXYFS_DEBUG("dentry=%pd, name=" QSTR_FMT "\n",
                  dentry,
                  QSTR_ARG(name));
    const struct dentry *lower_dentry = proxyfs_lower_dentry((struct dentry *)dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_hash) {
        ret = lower_ops->d_hash(lower_dentry, name);
    }
    return ret;
}

// d_compare()
static int proxyfs_compare(const struct dentry *dentry,
                           unsigned int flags,
                           const char *str,
                           const struct qstr *qstr)
{
    int ret = -ENOSYS;
    PROXYFS_DEBUG("dentry=%pd, flags=0x%x, str=%s, qstr=" QSTR_FMT "\n",
                  dentry,
                  flags,
                  str,
                  QSTR_ARG(qstr));
    const struct dentry *lower_dentry = proxyfs_lower_dentry((struct dentry *)dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_compare) {
        ret = lower_ops->d_compare(lower_dentry, flags, str, qstr);
    }
    return ret;
}

// d_delete()
static int proxyfs_delete(const struct dentry *dentry)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct dentry *lower_dentry = proxyfs_lower_dentry((struct dentry *)dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_delete) {
        return lower_ops->d_delete(lower_dentry);
    }
    return 1;
}

// d_init()
static int proxyfs_init(struct dentry *dentry)
{
    struct proxyfs_dentry_info *info;
    struct proxyfs_dentry_info *parent_info;
    struct dentry *lower_parent = NULL;
    struct dentry *lower_dentry = NULL;
    struct vfsmount *lower_mnt = NULL;
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    if (dentry->d_parent && dentry->d_parent->d_fsdata) {
        parent_info = (struct proxyfs_dentry_info *)dentry->d_parent->d_fsdata;
        lower_parent = parent_info->lower_dentry;
    }
    if (lower_parent) {
        lower_dentry = lookup_one_len(dentry->d_name.name, lower_parent, dentry->d_name.len);
        if (IS_ERR(lower_dentry)) {
            lower_dentry = NULL;
        }
    }
    if ((info = kmalloc(sizeof(*info), GFP_KERNEL)) == NULL) {
        return -ENOMEM;
    }
    info->lower_dentry = lower_dentry;
    // increment of `refcount`
    info->lower_mnt = lower_mnt ? mntget(lower_mnt) : NULL;
    dentry->d_fsdata = info;
    return 0;
}

// d_release()
static void proxyfs_release(struct dentry *dentry)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct proxyfs_dentry_info *info = (struct proxyfs_dentry_info *)dentry->d_fsdata;
    if (info) {
        if (info->lower_dentry != NULL) {
            dput(info->lower_dentry);
        }
        if (info->lower_mnt != NULL) {
            mntput(info->lower_mnt);
        }
        kfree(info);
        dentry->d_fsdata = NULL;
    }
    //
    // Note: `d_release` is intended to release private resources of
    //       `dentry` instance; thus we don't need to invoke
    //       `d_release` method of underlying FS (unsafe behavior)
}

// d_prune()
static void proxyfs_prune(struct dentry *dentry)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct proxyfs_dentry_info *info = (struct proxyfs_dentry_info *)dentry->d_fsdata;
    if (info != NULL) {
        if (info->lower_dentry != NULL) {
            // Just decrease `refcount` of `lower_dentry`
            dput(info->lower_dentry);
        }
        kfree(info);
        dentry->d_fsdata = NULL;
    }
}

// d_iput()
static void proxyfs_iput(struct dentry *dentry, struct inode *inode)
{
    //
    // Note: this routine is intended to decrease reference counter just
    //       of inode instance involved
    PROXYFS_DEBUG("dentry=%pd, inode=" INODE_FMT "\n",
                  dentry,
                  INODE_ARG(inode));
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode) {
        iput(lower_inode);
    }
}

// d_dname()
static char *proxyfs_dname(struct dentry *dentry,
                           char *buffer,
                           int buffer_len)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
    if (lower_ops && lower_ops->d_dname) {
        return lower_ops->d_dname(lower_dentry, buffer, buffer_len);
    }
    snprintf(buffer, buffer_len, "%s:%s", MODULE_NAME, proxyfs_dentry_name(dentry));
    return buffer;
}

// d_automount()
static struct vfsmount *proxyfs_automount(struct path *path)
{
    struct vfsmount *lower_mnt = NULL;
    PROXYFS_DEBUG("path=%pd\n", (path ? path->dentry : NULL));
    if (path == NULL) {
        return lower_mnt;
    }
    struct dentry *dentry = path->dentry;
    if (dentry != NULL) {
        struct proxyfs_dentry_info *info = dentry->d_fsdata;
        if (info && info->lower_dentry && info->lower_mnt) {
            lower_mnt = info->lower_mnt;
            // Incremet reference count of `lower_mnt`
            mntget(lower_mnt);
            return lower_mnt;
        }
    }
    //
    // Note: auto mount for underlying FS is not supported yet
    return NULL;
}

// d_manage()
static int proxyfs_manage(const struct path *path, bool do_invalidate)
{
    PROXYFS_DEBUG("path=%pd, do_invalidate=%s\n",
                  (path ? path->dentry : NULL),
                  (do_invalidate ? "true" : "false"));
    if (path == NULL || path->dentry == NULL) {
        return -EINVAL;
    }

    struct dentry *dentry = path->dentry;
    struct inode *inode = dentry->d_inode;

    if (do_invalidate) {
        // Invalidate `dentry` if it is not used right now
        d_invalidate(dentry);
        // Invalidate `inode` if it is present
        if (inode != NULL) {
            invalidate_inode_pages2(inode->i_mapping);
        }
    }

    return 0;
}

// d_real()
static struct dentry *proxyfs_real(struct dentry *dentry,
                                   enum d_real_type type)
{
    PROXYFS_DEBUG("dentry=%pd, type=%d\n", dentry, type);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_dentry != NULL) {
        const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
        if (lower_ops && lower_ops->d_real) {
            return lower_ops->d_real(lower_dentry, type);
        }
    }
    return lower_dentry ? lower_dentry : dentry;
}

// d_unalias_trylock()
static bool proxyfs_unalias_trylock(const struct dentry *dentry)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct dentry *lower_dentry = proxyfs_lower_dentry((struct dentry *)dentry);
    if (lower_dentry != NULL) {
        const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
        if (lower_ops && lower_ops->d_unalias_trylock) {
            return lower_ops->d_unalias_trylock(lower_dentry);
        }
    }
    return true;
}

// d_unalias_unlock()
static void proxyfs_unalias_unlock(const struct dentry *dentry)
{
    PROXYFS_DEBUG("dentry=%pd\n", dentry);
    struct dentry *lower_dentry = proxyfs_lower_dentry((struct dentry *)dentry);
    if (lower_dentry != NULL) {
        const struct dentry_operations *lower_ops = lower_dentry ? lower_dentry->d_op : NULL;
        if (lower_ops && lower_ops->d_unalias_unlock) {
            return lower_ops->d_unalias_unlock(lower_dentry);
        }
    }
}

const struct dentry_operations proxyfs_dentry_ops = {
	// int (*d_revalidate)(struct inode *, const struct qstr *, struct dentry *, unsigned int);
    .d_revalidate = proxyfs_revalidate,
	// int (*d_weak_revalidate)(struct dentry *, unsigned int);
    .d_weak_revalidate = proxyfs_weak_revalidate,
	// int (*d_hash)(const struct dentry *, struct qstr *);
    .d_hash = proxyfs_hash,
	// int (*d_compare)(const struct dentry *, unsigned int, const char *, const struct qstr *);
    .d_compare = proxyfs_compare,
	// int (*d_delete)(const struct dentry *);
    .d_delete = proxyfs_delete,
	// int (*d_init)(struct dentry *);
    .d_init = proxyfs_init,
	// void (*d_release)(struct dentry *);
    .d_release = proxyfs_release,
	// void (*d_prune)(struct dentry *);
    .d_prune = proxyfs_prune,
	// void (*d_iput)(struct dentry *, struct inode *);
    .d_iput = proxyfs_iput,
	// char *(*d_dname)(struct dentry *, char *, int);
    .d_dname = proxyfs_dname,
	// struct vfsmount *(*d_automount)(struct path *);
    .d_automount = proxyfs_automount,
	// int (*d_manage)(const struct path *, bool);
    .d_manage = proxyfs_manage,
	// struct dentry *(*d_real)(struct dentry *, enum d_real_type type);
    .d_real = proxyfs_real,
	// bool (*d_unalias_trylock)(const struct dentry *);
    .d_unalias_trylock = proxyfs_unalias_trylock,
	// void (*d_unalias_unlock)(const struct dentry *);
    .d_unalias_unlock = proxyfs_unalias_unlock,
};
