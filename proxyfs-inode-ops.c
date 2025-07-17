// File		:proxyfs-inode-ops.c
// Author	:Victor Kovalevich
// Created	:Tue Jul 15 00:39:51 2025
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
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/fiemap.h>
#include <linux/fileattr.h>

// lookup()
static struct dentry *proxyfs_lookup(struct inode *dir,
                                     struct dentry *dentry,
                                     unsigned int flags)
{
    PROXYFS_DEBUG("dir=" INODE_FMT ", dentry=%pd, flags=%ul\n",
                  INODE_ARG(dir),
                  dentry,
                  flags);
    struct dentry *ret = NULL;
    struct dentry *new_lower_dentry = NULL;
    do {
        if (dir == NULL || dentry == NULL) {
            ret = ERR_PTR(-EINVAL);
            break;
        }
        struct dentry *lower_parent = proxyfs_lower_dentry(dentry->d_parent);
        struct dentry *lower_dentry = d_lookup(lower_parent, &dentry->d_name);
        if (!lower_dentry) {
            if ((lower_dentry = d_alloc(lower_parent, &dentry->d_name)) == NULL) {
                ret = ERR_PTR(-ENOMEM);
                break;
            }
            new_lower_dentry = lower_dentry;
        }
        struct inode *lower_dir = proxyfs_lower_inode(dir);
        if (lower_dir->i_op && lower_dir->i_op->lookup) {
            struct dentry *res = lower_dir->i_op->lookup(lower_dir, lower_dentry, flags);
            if (IS_ERR(res)) {
                ret = res;
                break;
            }
        }
        struct inode *lower_inode = lower_dentry->d_inode;
        struct inode *inode = NULL;
        if (lower_inode) {
            inode = new_inode(dentry->d_sb);
            // Fill proxyfs inode with the attributes from lower_inode
            inode->i_ino = lower_inode->i_ino;
            ((struct proxyfs_inode *)inode)->lower_inode = lower_inode;
        }

        d_add(dentry, inode);
        proxyfs_init_dentry_ops(dentry);

        ret = NULL;
    } while (false);
    if (IS_ERR(ret)) {
        if (new_lower_dentry) {
            dput(new_lower_dentry);
        }
    }
    return ret;
}

// get_link()
static const char *proxyfs_get_link(struct dentry *dentry,
                                    struct inode *inode,
                                    struct delayed_call *done)
{
    PROXYFS_DEBUG("dentry=%pd, inode=" INODE_FMT ", done=%p\n",
                  dentry,
                  INODE_ARG(inode),
                  done);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->get_link) {
        return lower_inode->i_op->get_link(lower_dentry, lower_inode, done);
    }
    return ERR_PTR(-ENOSYS);
}

// permission()
static int proxyfs_permission(struct mnt_idmap *idmap,
                              struct inode *inode,
                              int mask)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", mask=0x%x\n",
                  INODE_ARG(inode),
                  mask);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->permission) {
        return lower_inode->i_op->permission(idmap, lower_inode, mask);
    }
    return -ENOSYS;
}

// get_inode_acl()
static struct posix_acl *proxyfs_get_inode_acl(struct inode *inode,
                                               int type,
                                               bool rcu)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", type=%d, rcu=%d\n",
                  INODE_ARG(inode),
                  type,
                  rcu);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->get_inode_acl) {
        return lower_inode->i_op->get_inode_acl(lower_inode, type, rcu);
    }
    return ERR_PTR(-ENOSYS);
}

// readlink()
static int proxyfs_readlink(struct dentry *dentry,
                            char __user *buffer,
                            int buffer_len)
{
    PROXYFS_DEBUG("dentry=%pd, buffer=%p, buffer_len=%d\n",
                  dentry,
                  buffer,
                  buffer_len);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->readlink) {
        return lower_inode->i_op->readlink(dentry, buffer, buffer_len);
    }
    return -ENOSYS;
}

// create()
static int proxyfs_create(struct mnt_idmap *idmap,
                          struct inode *dir,
                          struct dentry *dentry,
                          umode_t mode,
                          bool excl)
{
    PROXYFS_DEBUG("idmap=%p, dir=" INODE_FMT ", dentry=%pd, mode=0%o, excl=%s\n",
                  idmap,
                  INODE_ARG(dir),
                  dentry,
                  mode,
                  (excl ? "true" : "false"));
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->create) {
        return lower_inode->i_op->create(idmap, lower_inode, dentry, mode, excl);
    }
    return -ENOSYS;
}

// link()
static int proxyfs_link(struct dentry *old_dentry,
                        struct inode *dir,
                        struct dentry *dentry)
{
    PROXYFS_DEBUG("old_dentry=%pd, dir=" INODE_FMT ", dentry=%pd\n",
                  old_dentry,
                  INODE_ARG(dir),
                  dentry);
    struct dentry *lower_old_dentry = proxyfs_lower_dentry(old_dentry);
    struct inode *lower_dir = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_dir->i_op && lower_dir->i_op->link) {
        return lower_dir->i_op->link(lower_old_dentry, lower_dir, lower_dentry);
    }
    return -ENOSYS;
}

// unlink()
static int proxyfs_unlink(struct inode *dir,
                          struct dentry *dentry)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", dentry=%pd\n",
                  INODE_ARG(dir),
                  dentry);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_inode->i_op && lower_inode->i_op->unlink) {
        return lower_inode->i_op->unlink(lower_inode, lower_dentry);
    }
    return -ENOSYS;
}

// symlink()
static int proxyfs_symlink(struct mnt_idmap *idmap,
                           struct inode *dir,
                           struct dentry *dentry,
                           const char *symname)
{
    PROXYFS_DEBUG("idmap=%p, dir=" INODE_FMT ", dentry=%pd, symname=%s\n",
                  idmap,
                  INODE_ARG(dir),
                  dentry,
                  symname);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_inode->i_op && lower_inode->i_op->symlink) {
        return lower_inode->i_op->symlink(idmap, lower_inode, lower_dentry, symname);
    }
    return -ENOSYS;
}

// mkdir()
static struct dentry *proxyfs_mkdir(struct mnt_idmap *idmap,
                                    struct inode *dir,
                                    struct dentry *dentry,
                                    umode_t mode)
{
    PROXYFS_DEBUG("idmap=%p, dir=" INODE_FMT ", dentry=%pd, mode=0%o\n",
                  idmap,
                  INODE_ARG(dir),
                  dentry,
                  mode);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_inode->i_op && lower_inode->i_op->mkdir) {
        return lower_inode->i_op->mkdir(idmap, lower_inode, lower_dentry, mode);
    }
    return ERR_PTR(-ENOSYS);
}

// rmdir()
static int proxyfs_rmdir(struct inode *dir,
                         struct dentry *dentry)
{
    PROXYFS_DEBUG("dir=" INODE_FMT ", dentry=%pd\n",
                  INODE_ARG(dir),
                  dentry);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_inode->i_op && lower_inode->i_op->rmdir) {
        return lower_inode->i_op->rmdir(lower_inode, lower_dentry);
    }
    return -ENOSYS;
}

// mknod()
static int proxyfs_mknod(struct mnt_idmap *idmap,
                         struct inode *dir,
                         struct dentry *dentry,
                         umode_t mode,
                         dev_t dev)
{
    PROXYFS_DEBUG("idmap=%p, dir=" INODE_FMT ", dentry=%pd, mode=0%o, dev=%ul\n",
                  idmap,
                  INODE_ARG(dir),
                  dentry,
                  mode,
                  dev);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    if (lower_inode->i_op && lower_inode->i_op->mknod) {
        return lower_inode->i_op->mknod(idmap, lower_inode, lower_dentry, mode, dev);
    }
    return -ENOSYS;
}

// rename()
static int proxyfs_rename(struct mnt_idmap *idmap,
                          struct inode *old_dir,
                          struct dentry *old_dentry,
                          struct inode *new_dir,
                          struct dentry *new_dentry,
                          unsigned int flags)
{
    PROXYFS_DEBUG("idmap=%p, old_dir=" INODE_FMT ", old_dentry=%pd, new_dir=" INODE_FMT ", new_dentry=%pd, flags=0x%x\n",
                  idmap,
                  INODE_ARG(old_dir),
                  old_dentry,
                  INODE_ARG(new_dir),
                  new_dentry,
                  flags);
    struct inode *lower_old_dir = proxyfs_lower_inode(old_dir);
    struct dentry *lower_old_dentry = proxyfs_lower_dentry(old_dentry);
    struct inode *lower_new_dir = proxyfs_lower_inode(new_dir);
    struct dentry *lower_new_dentry = proxyfs_lower_dentry(new_dentry);
    if (lower_old_dir->i_op && lower_old_dir->i_op->rename) {
        return lower_old_dir->i_op->rename(idmap, lower_old_dir, lower_old_dentry, lower_new_dir, lower_new_dentry, flags);
    }
    return -ENOSYS;
}

// setattr()
static int proxyfs_setattr(struct mnt_idmap *idmap,
                           struct dentry *dentry,
                           struct iattr *attr)
{
    PROXYFS_DEBUG("idmap=%p, dentry=%pd, attr=%p\n",
                  idmap,
                  dentry,
                  attr);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = d_inode(lower_dentry);
    if (lower_inode->i_op && lower_inode->i_op->setattr) {
        return lower_inode->i_op->setattr(idmap, lower_dentry, attr);
    }
    return -ENOSYS;
}

// getattr()
static int proxyfs_getattr(struct mnt_idmap *idmap,
                           const struct path *path,
                           struct kstat *stat,
                           u32 request_mask,
                           unsigned int flags)
{
    PROXYFS_DEBUG("idmap=%p, path=%p, stat=%p, request_mask=0x%x, flags=0x%x\n",
                  idmap,
                  path,
                  stat,
                  request_mask,
                  flags);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(path->dentry));
    if (lower_inode->i_op && lower_inode->i_op->getattr) {
        return lower_inode->i_op->getattr(idmap, path, stat, request_mask, flags);
    }
    return -ENOSYS;
}

// listxattr()
static ssize_t proxyfs_listxattr(struct dentry *dentry,
                                 char *buffer,
                                 size_t buffer_len)
{
    PROXYFS_DEBUG("dentry=%pd, buffer=%p, size=%zu\n",
                  dentry,
                  buffer,
                  buffer_len);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->listxattr) {
        return lower_inode->i_op->listxattr(lower_dentry, buffer, buffer_len);
    }
    return -ENOSYS;
}

// fiemap()
static int proxyfs_fiemap(struct inode *inode,
                          struct fiemap_extent_info *fieinfo,
                          u64 start,
                          u64 len)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", fileinfo=%p, start=%llu, len=%llu\n",
                  INODE_ARG(inode),
                  fieinfo,
                  start,
                  len);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->fiemap) {
        return lower_inode->i_op->fiemap(lower_inode, fieinfo, start, len);
    }
    return -ENOSYS;
}

static int proxyfs_update_time(struct inode *inode, int flags)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", flags=0x%x\n",
                  INODE_ARG(inode),
                  flags);
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->update_time) {
        return lower_inode->i_op->update_time(lower_inode, flags);
    }
    return -ENOSYS;
}

// atomic_open()
static int proxyfs_atomic_open(struct inode *dir,
                               struct dentry *dentry,
                               struct file *file,
                               unsigned open_flag,
                               umode_t create_mode)
{
    PROXYFS_DEBUG("inode=" INODE_FMT ", dentry=%pd, file=%p, open_flag=0x%x, create_mode=0%o\n",
                  INODE_ARG(dir),
                  dentry,
                  file,
                  open_flag,
                  create_mode);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_inode->i_op && lower_inode->i_op->atomic_open) {
        return lower_inode->i_op->atomic_open(lower_inode, lower_dentry, lower_file, open_flag, create_mode);
    }
    return -ENOSYS;
}

// tmpfile()
static int proxyfs_tmpfile(struct mnt_idmap *idmap,
                           struct inode *dir,
                           struct file *file,
                           umode_t mode)
{
    PROXYFS_DEBUG("idmap=%p, inode=" INODE_FMT ", file=%p, mode=0%o\n",
                  idmap,
                  INODE_ARG(dir),
                  file,
                  mode);
    struct file *lower_file = proxyfs_lower_file(file);
    struct inode *lower_inode = proxyfs_lower_inode(dir);
    if (lower_inode->i_op && lower_inode->i_op->tmpfile) {
        return lower_inode->i_op->tmpfile(idmap, lower_inode, lower_file, mode);
    }
    return -ENOSYS;
}

// get_acl()
static struct posix_acl *proxyfs_get_acl(struct mnt_idmap *idmap,
                                         struct dentry *dentry,
                                         int type)
{
    PROXYFS_DEBUG("idmap=%p, dentry=%pd, type=%d\n",
                  idmap,
                  dentry,
                  type);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->get_acl) {
        return lower_inode->i_op->get_acl(idmap, lower_dentry, type);
    }
    return ERR_PTR(-ENOSYS);
}

// set_acl()
static int proxyfs_set_acl(struct mnt_idmap *idmap,
                           struct dentry *dentry,
                           struct posix_acl *acl,
                           int type)
{
    PROXYFS_DEBUG("idmap=%p, dentry=%pd, acl=%p, type=%d\n",
                  idmap,
                  dentry,
                  acl,
                  type);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->set_acl) {
        return lower_inode->i_op->set_acl(idmap, lower_dentry, acl, type);
    }
    return -ENOSYS;
}

// fileattr_set()
static int proxyfs_fileattr_set(struct mnt_idmap *idmap,
                                struct dentry *dentry,
                                struct fileattr *fa)
{
    PROXYFS_DEBUG("idmap=%p, dentry=%pd, fa=%p\n",
                  idmap,
                  dentry,
                  fa);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->fileattr_set) {
        return lower_inode->i_op->fileattr_set(idmap, lower_dentry, fa);
    }
    return -ENOSYS;
}

// fileattr_get()
static int proxyfs_fileattr_get(struct dentry *dentry,
                                struct fileattr *fa)
{
    PROXYFS_DEBUG("dentry=%pd, fa=%p\n",
                  dentry,
                  fa);
    struct dentry *lower_dentry = proxyfs_lower_dentry(dentry);
    struct inode *lower_inode = proxyfs_lower_inode(d_inode(dentry));
    if (lower_inode->i_op && lower_inode->i_op->fileattr_get) {
        return lower_inode->i_op->fileattr_get(lower_dentry, fa);
    }
    return -ENOSYS;
}

// get_offset_ctx()
static struct offset_ctx *proxyfs_get_offset_ctx(struct inode *inode)
{
    PROXYFS_DEBUG("inode=" INODE_FMT "\n",
                  INODE_ARG(inode));
    struct inode *lower_inode = proxyfs_lower_inode(inode);
    if (lower_inode->i_op && lower_inode->i_op->get_offset_ctx) {
        return lower_inode->i_op->get_offset_ctx(lower_inode);
    }
    return NULL;
}

// dir_inode_operations
const struct inode_operations proxyfs_inode_ops = {
    // struct dentry * (*lookup) (struct inode *,struct dentry *, unsigned int);
    .lookup = proxyfs_lookup,
    // const char * (*get_link) (struct dentry *, struct inode *, struct delayed_call *);
    .get_link = proxyfs_get_link,
    // int (*permission) (struct mnt_idmap *, struct inode *, int);
    .permission = proxyfs_permission,
    // struct posix_acl * (*get_inode_acl)(struct inode *, int, bool);
    .get_inode_acl = proxyfs_get_inode_acl,
    // int (*readlink) (struct dentry *, char __user *,int);
    .readlink = proxyfs_readlink,
    // int (*create) (struct mnt_idmap *, struct inode *,struct dentry *, umode_t, bool);
    .create = proxyfs_create,
    // int (*link) (struct dentry *,struct inode *,struct dentry *);
    .link = proxyfs_link,
    // int (*unlink) (struct inode *,struct dentry *);
    .unlink = proxyfs_unlink,
    // int (*symlink) (struct mnt_idmap *, struct inode *,struct dentry *, const char *);
    .symlink = proxyfs_symlink,
    // struct dentry *(*mkdir) (struct mnt_idmap *, struct inode *, struct dentry *, umode_t);
    .mkdir = proxyfs_mkdir,
    // int (*rmdir) (struct inode *,struct dentry *);
    .rmdir = proxyfs_rmdir,
    // int (*mknod) (struct mnt_idmap *, struct inode *,struct dentry *, umode_t,dev_t);
    .mknod = proxyfs_mknod,
    // int (*rename) (struct mnt_idmap *, struct inode *, struct dentry *, struct inode *, struct dentry *, unsigned int);
    .rename = proxyfs_rename,
    // int (*setattr) (struct mnt_idmap *, struct dentry *, struct iattr *);
    .setattr = proxyfs_setattr,
    // int (*getattr) (struct mnt_idmap *, const struct path *,	struct kstat *, u32, unsigned int);
    .getattr = proxyfs_getattr,
    // ssize_t (*listxattr) (struct dentry *, char *, size_t);
    .listxattr = proxyfs_listxattr,
    // int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start, u64 len);
    .fiemap = proxyfs_fiemap,
    // int (*update_time)(struct inode *, int);
    .update_time = proxyfs_update_time,
    // int (*atomic_open)(struct inode *, struct dentry *, struct file *, unsigned open_flag, umode_t create_mode);
    .atomic_open = proxyfs_atomic_open,
    // int (*tmpfile) (struct mnt_idmap *, struct inode *, struct file *, umode_t);
    .tmpfile = proxyfs_tmpfile,
    // struct posix_acl *(*get_acl)(struct mnt_idmap *, struct dentry *, int);
    .get_acl = proxyfs_get_acl,
    // int (*set_acl)(struct mnt_idmap *, struct dentry *, struct posix_acl *, int);
    .set_acl = proxyfs_set_acl,
    // int (*fileattr_set)(struct mnt_idmap *idmap, struct dentry *dentry, struct fileattr *fa);
    .fileattr_set = proxyfs_fileattr_set,
    // int (*fileattr_get)(struct dentry *dentry, struct fileattr *fa);
    .fileattr_get = proxyfs_fileattr_get,
    // struct offset_ctx *(*get_offset_ctx)(struct inode *inode);
    .get_offset_ctx = proxyfs_get_offset_ctx,
};
