// File		:proxyfs.h
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:18:22 2025
#ifndef __PROXYFS_H__
#define __PROXYFS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/dcache.h>
#include <net/sock.h>

// #include <linux/pagemap.h>
// #include <linux/mount.h>
// #include <linux/skbuff.h>
// #include <linux/uaccess.h>
// #include <linux/seq_file.h>
// #include <linux/bitmap.h>
// #include <linux/slab.h>
// #include <linux/uaccess.h>

#include "proxyfs-buffer-pool.h"

#define PROXYFS_MAGIC 0x20250710
#define MODULE_NAME   "proxyfs"

#define PROXYFS_PROCFS_DIR     (MODULE_NAME)
#define PROXYFS_PROCFS_UNIT_ID "unit_id"
#define PROXYFS_PROCFS_FILTERS "filters"
#define PROXYFS_PROCFS_PIDS    "pids"

#define PROXYFS_NETLINK_USER    25

#define QSTR_FMT "%.*s"
#define QSTR_ARG(s) ((s) ? (s)->len : 0), ((s) ? (char *)(s)->name : "")

#define INODE_FMT "%lu"
#define INODE_ARG(i) ((i) ? (i)->i_ino : 0)

#define PROXYFS_DEBUG(fmt, ...) \
  pr_info("%s: %s: " fmt, MODULE_NAME, __func__, ##__VA_ARGS__)

static inline const char *proxyfs_dentry_name(const struct dentry *dentry) {
    return dentry ? (const char*)dentry->d_name.name : "?";
}

struct proxyfs_context_data {
    //
    // NETLINK socket (communication with a userspace process (client)
    struct sock* nl_socket;
    //
    // Userspace client PID
    atomic_t client_pid;
    //
    // Procfs directory with module specific entries
    struct proc_dir_entry* proc_dir;
    //
    // Pool of the buffers used to prepare ans send the messages
    // via NETLINK channel
    struct proxyfs_buffer_pool buffer_pool;
    //
    // Running state used to demonstrate if the module is in running
    // state or going to stop running
    atomic_t running_state;
    atomic_t handler_counter;
};

struct proxyfs_inode {
    struct inode vfs_inode;
    struct inode *lower_inode;
};

// Get inode of underlying FS from proxyfs inode
inline static struct inode *proxyfs_lower_inode(const struct inode *inode)
{
    if (inode != NULL) {
        return container_of(inode, struct proxyfs_inode, vfs_inode)->lower_inode;
    }
    return NULL;
}

struct proxyfs_file_info {
    struct file *lower_file;
};

// Get file of underlying FS from proxyfs file
inline static struct file *proxyfs_lower_file(const struct file *file)
{
    if (file) {
        return ((struct proxyfs_file_info *)file->private_data)->lower_file;
    }
    return NULL;
}

struct proxyfs_sb_info {
    struct super_block *lower_sb;
};

// Get super block of underlying FS from proxyfs super block
inline static struct super_block *proxyfs_lower_sb(const struct super_block *sb)
{
    if (sb != NULL) {
        return ((struct proxyfs_sb_info *)sb->s_fs_info)->lower_sb;
    }
    return NULL;
}

struct proxyfs_dentry_info {
    struct dentry *lower_dentry;
    struct vfsmount *lower_mnt;
};

inline static struct dentry *proxyfs_lower_dentry(struct dentry *dentry)
{
    if (dentry != NULL) {
        return ((struct proxyfs_dentry_info *)dentry->d_fsdata)->lower_dentry;
    }
    return NULL;
}

inline static struct vfsmount *proxyfs_lower_mnt(struct dentry *dentry)
{
    if (dentry != NULL) {
        return ((struct proxyfs_dentry_info *)dentry->d_fsdata)->lower_mnt;
    }
    return NULL;
}

//
// Module context specific routines
int proxyfs_context_set_client_pid(const int new_client_pid);
int proxyfs_context_get_client_pid(void);
bool proxyfs_context_check_uid(const int uid);
bool proxyfs_context_check_is_running(void);
void proxyfs_context_handler_counter_increment(void);
void proxyfs_context_handler_counter_decrement(void);
struct sock* proxyfs_context_get_nl_socket(void);

//
// Buffer pool specific routines
void* proxyfs_context_buffer_pool_alloc(struct proxyfs_context_data *context_data);
bool proxyfs_context_buffer_pool_free(struct proxyfs_context_data *context_data,
                                      void* buffer);
unsigned int proxyfs_context_buffer_pool_get_buffer_size(struct proxyfs_context_data *context_data);

//
// NETLINK communication specific routines
struct sock* proxyfs_socket_init(const int nl_unit_id);
void proxyfs_socket_release(struct sock* nl_socket);
void proxyfs_socket_send_msg(const char* msg_body,
                             size_t msg_len);

//
// Procfs specific routines
struct proc_dir_entry* proxyfs_procfs_setup(void);
void proxyfs_procfs_release(void);

extern const struct file_operations proxyfs_file_ops;
extern const struct inode_operations proxyfs_inode_ops;
extern const struct super_operations proxyfs_super_ops;
extern const struct dentry_operations proxyfs_dentry_ops;
extern const struct address_space_operations proxyfs_mapping_ops;

//
// This routine is used to poupulate proxyfs super block,
// see `struct proxyfs_sb_info` above
int proxyfs_fill_super_block(struct super_block *sb,
                             void *data,
                             int silent);

inline static void proxyfs_init_inode_ops(struct inode *inode)
{
    if (inode) {
        inode->i_fop = &proxyfs_file_ops;
        inode->i_op = &proxyfs_inode_ops;
        inode->i_mapping->a_ops = &proxyfs_mapping_ops;
    }
}

inline static void proxyfs_init_dentry_ops(struct dentry *dentry)
{
    if (dentry) {
        dentry->d_op = &proxyfs_dentry_ops;
    }
}

#endif //  !__PROXYFS_H__
