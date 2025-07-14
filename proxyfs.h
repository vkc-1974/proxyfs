// File		:proxyfs.h
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:18:22 2025
#ifndef __PROXYFS_H__
#define __PROXYFS_H__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/namei.h>
#include <linux/uaccess.h>
#include <linux/printk.h>

#define PROXYFS_MAGIC 0x20250710
#define MODULE_NAME   "proxyfs"

#define PROXYFS_PROCFS_DIR     (MODULE_NAME)
#define PROXYFS_PROCFS_UNIT_ID "unit_id"
#define PROXYFS_PROCFS_FILTERS "filters"
#define PROXYFS_PROCFS_PIDS    "pids"

#define PROXYFS_NETLINK_USER    25

struct proxyfs_buffer_pool {
    void **buffers;
    unsigned long *bitmap;
    unsigned int size;
    unsigned int count;
    spinlock_t lock;
    atomic_t in_use;
};

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
bool proxyfs_context_buffer_pool_init(struct proxyfs_buffer_pool *buffer_pool,
                                      unsigned int count,
                                      unsigned int size);
void proxyfs_context_buffer_pool_destroy(struct proxyfs_buffer_pool* pool);
int proxyfs_buffer_pool_in_use(struct proxyfs_buffer_pool* pool);
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

#endif //  !__PROXYFS_H__
