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


// Get lower file from proxy file
static struct file *proxyfs_lower_file(const struct file *file) {
    return ((struct proxyfs_file_info *)file->private_data)->lower_file;
}

// llseek()
static loff_t proxyfs_llseek(struct file *file, loff_t offset, int whence) {
    PROXYFS_DEBUG("name=%s, offset=%lld, whence=%d\n", file->f_path.dentry->d_name.name, offset, whence);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->llseek) {
        return lower_file->f_op->llseek(lower_file, offset, whence);
    }
    return -ENOSYS;
}

// read()
static ssize_t proxyfs_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    PROXYFS_DEBUG("name=%s, count=%zu\n", file->f_path.dentry->d_name.name, count);
    return kernel_read(proxyfs_lower_file(file), buf, count, ppos);
}

// write()
static ssize_t proxyfs_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    PROXYFS_DEBUG("name=%s, count=%zu\n", file->f_path.dentry->d_name.name, count);
    return kernel_write(proxyfs_lower_file(file), buf, count, ppos);
}

// ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
// ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
// int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *, unsigned int flags);

// iterate_shared()
static int proxyfs_iterate_shared(struct file *file, struct dir_context *ctx) {
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->iterate_shared) {
        return lower_file->f_op->iterate_shared(lower_file, ctx);
    }
    return -ENOSYS;
}

// __poll_t (*poll) (struct file *, struct poll_table_struct *);

// unlocked_ioctl()
static long proxyfs_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    PROXYFS_DEBUG("name=%s, cmd=0x%x\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->unlocked_ioctl) {
        return lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);
    }
    return -ENOTTY;
}

// compat_ioctl()
static long proxyfs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    PROXYFS_DEBUG("name=%s, cmd=0x%x\n", file->f_path.dentry->d_name.name, cmd);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->compat_ioctl) {
        return lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
    }
    return -ENOTTY;
}

// mmap()
static int proxyfs_mmap(struct file *file, struct vm_area_struct *vma) {
    PROXYFS_DEBUG("name=%s\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->mmap) {
        return lower_file->f_op->mmap(lower_file, vma);
    }
    return -ENOSYS;
}

// open()
static int proxyfs_open(struct inode *inode, struct file *file) {
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
static int proxyfs_flush(struct file *file, fl_owner_t id) {
    PROXYFS_DEBUG("name=%s flush\n", file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->flush) {
        return lower_file->f_op->flush(lower_file, id);
    }
    return 0;
}

// release() / close
static int proxyfs_release(struct inode *inode, struct file *file) {
    PROXYFS_DEBUG("inode=%lu, name=%s\n", inode->i_ino, file->f_path.dentry->d_name.name);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file) {
        fput(lower_file);
    }
    kfree(file->private_data);
    return 0;
}

// fsync()
static int proxyfs_fsync(struct file *file, loff_t start, loff_t end, int datasync) {
    PROXYFS_DEBUG("name=%s fsync start=%lld end=%lld datasync=%d\n", file->f_path.dentry->d_name.name, start, end, datasync);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fsync) {
        return lower_file->f_op->fsync(lower_file, start, end, datasync);
    }
    return 0;
}

// fasync()
static int proxyfs_fasync(int fd, struct file *file, int on) {
    PROXYFS_DEBUG("name=%s fasync fd=%d on=%d\n", file->f_path.dentry->d_name.name, fd, on);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->fasync) {
        return lower_file->f_op->fasync(fd, lower_file, on);
    }
    return -ENOSYS;
}

// int (*lock) (struct file *, int, struct file_lock *);
// unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);

// check_flags()
static int proxyfs_check_flags(int flags) {
    PROXYFS_DEBUG("check_flags flags=%x\n", flags);
    //
    // Usually it does nothing but can be delegated
    return 0;
}

// int (*flock) (struct file *, int, struct file_lock *);

// splice_write()
static ssize_t proxyfs_splice_write(struct pipe_inode_info *pipe,
                                    struct file *file,
                                    loff_t *ppos,
                                    size_t len,
                                    unsigned int flags) {
    PROXYFS_DEBUG("name=%s splice_write len=%zu\n", file->f_path.dentry->d_name.name, len);
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
                                   unsigned int flags) {
    PROXYFS_DEBUG("name=%s splice_read len=%zu\n", file->f_path.dentry->d_name.name, len);
    struct file *lower_file = proxyfs_lower_file(file);
    if (lower_file->f_op && lower_file->f_op->splice_read) {
        return lower_file->f_op->splice_read(lower_file, ppos, pipe, len, flags);
    }
    return -ENOSYS;
}

// void (*splice_eof)(struct file *file);
// int (*setlease)(struct file *, int, struct file_lease **, void **);
// long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
// void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
// unsigned (*mmap_capabilities)(struct file *);
#endif
// ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);
// loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in, struct file *file_out, loff_t pos_out, loff_t len, unsigned int remap_flags);
// int (*fadvise)(struct file *, loff_t, loff_t, int);
// int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
// int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *, unsigned int poll_flags);

// file_operations
const struct file_operations proxyfs_file_ops = {

	// struct module *owner;
    .owner = NULL,
	// fop_flags_t fop_flags;
    .fop_flags = 0,
	// loff_t (*llseek) (struct file *, loff_t, int);
    .llseek = proxyfs_llseek,
    // size_t (*read) (struct file *, char __user *, size_t, loff_t *);
    .read = proxyfs_read,
	// ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    .write = proxyfs_write,
	// ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
    .read_iter = NULL,
	// ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
    .write_iter = NULL,
	// int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *, unsigned int flags);
    .iopoll = NULL,
	// int (*iterate_shared) (struct file *, struct dir_context *);
    .iterate_shared = proxyfs_iterate_shared,
	// __poll_t (*poll) (struct file *, struct poll_table_struct *);
    .poll = NULL,
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
    .lock = NULL,
	// unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
    .get_unmapped_area = NULL,
	// int (*check_flags)(int);
    .check_flags = proxyfs_check_flags,
	// int (*flock) (struct file *, int, struct file_lock *);
    .flock = NULL,
	// ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    .splice_write = proxyfs_splice_write,
	// ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
    .splice_read = proxyfs_splice_read,
	// void (*splice_eof)(struct file *file);
    .splice_eof = NULL,
	// int (*setlease)(struct file *, int, struct file_lease **, void **);
    .setlease = NULL,
	// long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
    .fallocate = NULL,
	// void (*show_fdinfo)(struct seq_file *m, struct file *f);
    .show_fdinfo = NULL,
#ifndef CONFIG_MMU
	// unsigned (*mmap_capabilities)(struct file *);
    .mmap_capabilities = NULL,
#endif
	// ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);
    .copy_file_range = NULL,
	// loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in, struct file *file_out, loff_t pos_out, loff_t len, unsigned int remap_flags);
    .remap_file_range = NULL,
	// int (*fadvise)(struct file *, loff_t, loff_t, int);
    .fadvise = NULL,
	// int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
    .uring_cmd = NULL,
	// int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *, unsigned int poll_flags);
    .uring_cmd_iopoll = NULL,
};
