// File		:proxyfs-file-ops.c
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
#include <linux/io_uring/cmd.h>

// llseek()
static loff_t proxyfs_llseek(struct file *file,
                             loff_t offset,
                             int whence)
{
    PROXYFS_DEBUG("name=%s, offset=%lld, whence=%d\n", file->f_path.dentry->d_name.name, offset, whence);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->llseek) {
        return lower_file->f_op->llseek(lower_file, offset, whence);
    }
    return -ENOSYS;
}

// read()
static ssize_t proxyfs_read(struct file *file,
                            char __user *buf,
                            size_t count,
                            loff_t *ppos)
{
    PROXYFS_DEBUG("name=%s, count=%zu\n", file->f_path.dentry->d_name.name, count);
    return kernel_read(proxyfs_lower_file(file), buf, count, ppos);
}

// write()
static ssize_t proxyfs_write(struct file *file,
                             const char __user *buf,
                             size_t count,
                             loff_t *ppos)
{
    PROXYFS_DEBUG("name=%s, count=%zu\n", file->f_path.dentry->d_name.name, count);
    return kernel_write(proxyfs_lower_file(file), buf, count, ppos);
}

// read_iter
static ssize_t proxyfs_read_iter(struct kiocb *iocb,
                                 struct iov_iter *to)
{
    struct file *file = iocb->ki_filp;
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->read_iter) {
        struct kiocb lower_iocb = *iocb;
        lower_iocb.ki_filp = lower_file;
        return lower_file->f_op->read_iter(&lower_iocb, to);
    }
    return -ENOSYS;
}

// write_iter
static ssize_t proxyfs_write_iter(struct kiocb *iocb,
                                  struct iov_iter *from)
{
    struct file *file = iocb->ki_filp;
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->write_iter) {
        struct kiocb lower_iocb = *iocb;
        lower_iocb.ki_filp = lower_file;
        return lower_file->f_op->write_iter(&lower_iocb, from);
    }
    return -ENOSYS;
}

// iopoll
static int proxyfs_iopoll(struct kiocb *kiocb,
                          struct io_comp_batch *batch,
                          unsigned int flags)
{
    struct file *file = kiocb->ki_filp;
    PROXYFS_DEBUG("name=%s, flags=0x%x\n", file->f_path.dentry->d_name.name, flags);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->iopoll) {
        struct kiocb lower_iocb = *kiocb;
        lower_iocb.ki_filp = lower_file;
        return lower_file->f_op->iopoll(&lower_iocb, batch, flags);
    }
    return -ENOSYS;
}

// iterate_shared()
static int proxyfs_iterate_shared(struct file *file,
                                  struct dir_context *ctx)
{
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->iterate_shared) {
        return lower_file->f_op->iterate_shared(lower_file, ctx);
    }
    return -ENOSYS;
}

// poll
static __poll_t proxyfs_poll(struct file *file,
                             struct poll_table_struct *pts)
{
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->poll) {
        return lower_file->f_op->poll(lower_file, pts);
    }
    return 0;
}

// unlocked_ioctl()
static long proxyfs_unlocked_ioctl(struct file *file,
                                   unsigned int cmd,
                                   unsigned long arg)
{
    PROXYFS_DEBUG("name=%s, cmd=0x%x\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->unlocked_ioctl) {
        return lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);
    }
    return -ENOTTY;
}

// compat_ioctl()
static long proxyfs_compat_ioctl(struct file *file,
                                 unsigned int cmd,
                                 unsigned long arg)
{
    PROXYFS_DEBUG("name=%s, cmd=0x%x\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->compat_ioctl) {
        return lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
    }
    return -ENOTTY;
}

// mmap()
static int proxyfs_mmap(struct file *file,
                        struct vm_area_struct *vma)
{
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->mmap) {
        return lower_file->f_op->mmap(lower_file, vma);
    }
    return -ENOSYS;
}

// open()
static int proxyfs_open(struct inode *inode,
                        struct file *file)
{
    ///// struct inode *lower_inode = proxyfs_lower_inode(inode);
    struct file *lower_file;

    PROXYFS_DEBUG("inode=%lu, name=%s\n", inode->i_ino, file->f_path.dentry->d_name.name);

    //
    // TBD: inform user space running monitor application `open`
    //      routine has been called

    lower_file = dentry_open(&file->f_path, file->f_flags, current_cred());
    if (IS_ERR(lower_file)) {
        return PTR_ERR(lower_file);
    }

    file->private_data = kmalloc(sizeof(struct proxyfs_file_info), GFP_KERNEL);
    if (!file->private_data) {
        fput(lower_file);
        return -ENOMEM;
    }
    ((struct proxyfs_file_info *)file->private_data)->lower_file = lower_file;
    return 0;
}

// flush()
static int proxyfs_flush(struct file *file,
                         fl_owner_t id)
{
    PROXYFS_DEBUG("name=%s flush\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->flush) {
        return lower_file->f_op->flush(lower_file, id);
    }
    return 0;
}

// release() / close
static int proxyfs_release(struct inode *inode,
                           struct file *file)
{
    PROXYFS_DEBUG("inode=%lu, name=%s\n", inode->i_ino, file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file) {
        fput(lower_file);
    }
    kfree(file->private_data);
    return 0;
}

// fsync()
static int proxyfs_fsync(struct file *file,
                         loff_t start,
                         loff_t end,
                         int datasync)
{
    PROXYFS_DEBUG("name=%s, start=%lld, end=%lld, datasync=%d\n", file->f_path.dentry->d_name.name, start, end, datasync);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fsync) {
        return lower_file->f_op->fsync(lower_file, start, end, datasync);
    }
    return 0;
}

// fasync()
static int proxyfs_fasync(int fd,
                          struct file *file,
                          int on)
{
    PROXYFS_DEBUG("name=%s, fd=%d, on=%d\n", file->f_path.dentry->d_name.name, fd, on);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fasync) {
        return lower_file->f_op->fasync(fd, lower_file, on);
    }
    return -ENOSYS;
}

// lock
static int proxyfs_lock(struct file *file,
                        int cmd,
                        struct file_lock *fl)
{
    PROXYFS_DEBUG("name=%s, cmd=%d\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->lock) {
        return lower_file->f_op->lock(lower_file, cmd, fl);
    }
    return -ENOSYS;
}

// get_unmapped_area
static unsigned long proxyfs_get_unmapped_area(struct file *file,
                                               unsigned long uaddr,
                                               unsigned long len,
                                               unsigned long pgoff,
                                               unsigned long flags)
{
    PROXYFS_DEBUG("name=%s, len=%lu\n", file->f_path.dentry->d_name.name, len);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->get_unmapped_area) {
        return lower_file->f_op->get_unmapped_area(lower_file, uaddr, len, pgoff, flags);
    }
    return 0;
}

// check_flags()
static int proxyfs_check_flags(int flags)
{
    PROXYFS_DEBUG("check_flags flags=%x\n", flags);
    //
    // Usually it does nothing but can be delegated
    return 0;
}

// flock
static int proxyfs_flock(struct file *file,
                         int cmd,
                         struct file_lock *fl)
{
    PROXYFS_DEBUG("name=%s, cmd=%d\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->flock) {
        return lower_file->f_op->flock(lower_file, cmd, fl);
    }
    return -ENOSYS;
}

// splice_write()
static ssize_t proxyfs_splice_write(struct pipe_inode_info *pipe,
                                    struct file *file,
                                    loff_t *ppos,
                                    size_t len,
                                    unsigned int flags)
{
    PROXYFS_DEBUG("name=%s, len=%zu\n", file->f_path.dentry->d_name.name, len);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->splice_write) {
        return lower_file->f_op->splice_write(pipe, lower_file, ppos, len, flags);
    }
    return -ENOSYS;
}

// splice_read()
static ssize_t proxyfs_splice_read(struct file *file,
                                   loff_t *ppos,
                                   struct pipe_inode_info *pipe,
                                   size_t len,
                                   unsigned int flags)
{
    PROXYFS_DEBUG("name=%s, len=%zu\n", file->f_path.dentry->d_name.name, len);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->splice_read) {
        return lower_file->f_op->splice_read(lower_file, ppos, pipe, len, flags);
    }
    return -ENOSYS;
}

// splice_eof
static void proxyfs_splice_eof(struct file *file)
{
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->splice_eof) {
        lower_file->f_op->splice_eof(lower_file);
    }
}

// setlease
static int proxyfs_setlease(struct file *file,
                            int arg,
                            struct file_lease **flp,
                            void **priv)
{
    PROXYFS_DEBUG("name=%s, arg=%d\n", file->f_path.dentry->d_name.name, arg);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->setlease) {
        return lower_file->f_op->setlease(lower_file, arg, flp, priv);
    }
    return -ENOSYS;
}

// fallocate
static long proxyfs_fallocate(struct file *file,
                              int mode,
                              loff_t offset,
                              loff_t len)
{
    PROXYFS_DEBUG("name=%s, mode=%d, offset=%lld, len=%lld\n", file->f_path.dentry->d_name.name, mode, offset, len);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fallocate) {
        return lower_file->f_op->fallocate(lower_file, mode, offset, len);
    }
    return -ENOSYS;
}

// show_fdinfo
static void proxyfs_show_fdinfo(struct seq_file *m,
                                struct file *f)
{
    PROXYFS_DEBUG("name=%s\n", f->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(f);
    if (lower_file->f_op && lower_file->f_op->show_fdinfo) {
        lower_file->f_op->show_fdinfo(m, lower_file);
    }
}

#ifndef CONFIG_MMU
// mmap_capabilities
static unsigned proxyfs_mmap_capabilities(struct file *file)
{
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->mmap_capabilities) {
        return lower_file->f_op->mmap_capabilities(lower_file);
    }
    return 0;
}
#endif

// ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);
// copy_file_range
static ssize_t proxyfs_copy_file_range(struct file *file_in,
                                       loff_t pos_in,
                                       struct file *file_out,
                                       loff_t pos_out,
                                       size_t len,
                                       unsigned int flags)
{
    PROXYFS_DEBUG("name_in=%s, name_out=%s, len=%zu\n",
                  file_in->f_path.dentry->d_name.name,
                  file_out->f_path.dentry->d_name.name,
                  len);
    struct file *lower_in = proxyfs_lower_file(file_in);
    struct file *lower_out = proxyfs_lower_file(file_out);
    if (lower_in->f_op && lower_in->f_op->copy_file_range) {
        return lower_in->f_op->copy_file_range(lower_in, pos_in, lower_out, pos_out, len, flags);
    }
    return -ENOSYS;
}

// remap_file_range
static loff_t proxyfs_remap_file_range(struct file *file_in,
                                       loff_t pos_in,
                                       struct file *file_out,
                                       loff_t pos_out,
                                       loff_t len,
                                       unsigned int remap_flags)
{
    PROXYFS_DEBUG("name_in=%s, name_out=%s, len=%lld\n",
                  file_in->f_path.dentry->d_name.name,
                  file_out->f_path.dentry->d_name.name,
                  len);
    struct file *lower_in = proxyfs_lower_file(file_in);
    struct file *lower_out = proxyfs_lower_file(file_out);
    if (lower_in->f_op && lower_in->f_op->remap_file_range) {
        return lower_in->f_op->remap_file_range(lower_in, pos_in, lower_out, pos_out, len, remap_flags);
    }
    return -ENOSYS;
}

// fadvise
static int proxyfs_fadvise(struct file *file,
                           loff_t offset,
                           loff_t len,
                           int advice)
{
    PROXYFS_DEBUG("name=%s, offset=%lld, len=%lld, advice=%d\n", file->f_path.dentry->d_name.name, offset, len, advice);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fadvise) {
        return lower_file->f_op->fadvise(lower_file, offset, len, advice);
    }
    return -ENOSYS;
}

// uring_cmd
static int proxyfs_uring_cmd(struct io_uring_cmd *ioucmd,
                             unsigned int issue_flags)
{
    struct file *file = ioucmd->file;
    PROXYFS_DEBUG("name=%s, flags=0x%x\n", file->f_path.dentry->d_name.name, issue_flags);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->uring_cmd) {
        struct io_uring_cmd lower_cmd = *ioucmd;
        lower_cmd.file = lower_file;
        return lower_file->f_op->uring_cmd(&lower_cmd, issue_flags);
    }
    return -ENOSYS;
}

// uring_cmd_iopoll
static int proxyfs_uring_cmd_iopoll(struct io_uring_cmd *ioucmd,
                                    struct io_comp_batch *batch,
                                    unsigned int poll_flags)
{
    struct file *file = ioucmd->file;
    PROXYFS_DEBUG("name=%s, flags=0x%x\n", file->f_path.dentry->d_name.name, poll_flags);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->uring_cmd_iopoll) {
        struct io_uring_cmd lower_cmd = *ioucmd;
        lower_cmd.file = lower_file;
        return lower_file->f_op->uring_cmd_iopoll(&lower_cmd, batch, poll_flags);
    }
    return -ENOSYS;
}

// file_operations
const struct file_operations proxyfs_file_ops = {
	// struct module *owner;
    .owner = THIS_MODULE,
	// fop_flags_t fop_flags;
    .fop_flags = 0,
	// loff_t (*llseek) (struct file *, loff_t, int);
    .llseek = proxyfs_llseek,
    // size_t (*read) (struct file *, char __user *, size_t, loff_t *);
    .read = proxyfs_read,
	// ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    .write = proxyfs_write,
	// ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
    .read_iter = proxyfs_read_iter,
	// ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
    .write_iter = proxyfs_write_iter,
	// int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *, unsigned int flags);
    .iopoll = proxyfs_iopoll,
	// int (*iterate_shared) (struct file *, struct dir_context *);
    .iterate_shared = proxyfs_iterate_shared,
	// __poll_t (*poll) (struct file *, struct poll_table_struct *);
    .poll = proxyfs_poll,
	// long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    .unlocked_ioctl = proxyfs_unlocked_ioctl,
    // long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    .compat_ioctl = proxyfs_compat_ioctl,
    // int (*mmap) (struct file *, struct vm_area_struct *);
    .mmap = proxyfs_mmap,
	// int (*open) (struct inode *, struct file *);
    .open = proxyfs_open,
	// int (*flush) (struct file *, fl_owner_t id);
    .flush = proxyfs_flush,
	// int (*release) (struct inode *, struct file *);
    .release = proxyfs_release,
	// int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    .fsync = proxyfs_fsync,
	// int (*fasync) (int, struct file *, int);
    .fasync = proxyfs_fasync,
	// int (*lock) (struct file *, int, struct file_lock *);
    .lock = proxyfs_lock,
	// unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
    .get_unmapped_area = proxyfs_get_unmapped_area,
	// int (*check_flags)(int);
    .check_flags = proxyfs_check_flags,
	// int (*flock) (struct file *, int, struct file_lock *);
    .flock = proxyfs_flock,
	// ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    .splice_write = proxyfs_splice_write,
	// ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
    .splice_read = proxyfs_splice_read,
	// void (*splice_eof)(struct file *file);
    .splice_eof = proxyfs_splice_eof,
	// int (*setlease)(struct file *, int, struct file_lease **, void **);
    .setlease = proxyfs_setlease,
	// long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
    .fallocate = proxyfs_fallocate,
	// void (*show_fdinfo)(struct seq_file *m, struct file *f);
    .show_fdinfo = proxyfs_show_fdinfo,
#ifndef CONFIG_MMU
	// unsigned (*mmap_capabilities)(struct file *);
    .mmap_capabilities = proxyfs_mmap_capabilities,
#endif
	// ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);
    .copy_file_range = proxyfs_copy_file_range,
	// loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in, struct file *file_out, loff_t pos_out, loff_t len, unsigned int remap_flags);
    .remap_file_range = proxyfs_remap_file_range,
	// int (*fadvise)(struct file *, loff_t, loff_t, int);
    .fadvise = proxyfs_fadvise,
	// int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
    .uring_cmd = proxyfs_uring_cmd,
	// int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *, unsigned int poll_flags);
    .uring_cmd_iopoll = proxyfs_uring_cmd_iopoll,
};
