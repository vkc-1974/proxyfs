// File		:proxyfs-buffer-pool.h
// Author	:Victor Kovalevich
// Created	:Wed Jul 16 13:39:07 2025
#ifndef __PROXYFS_BUFFER_POOL_H__
#define __PROXYFS_BUFFER_POOL_H__
#include <linux/spinlock.h>
#include <linux/atomic.h>
    
struct proxyfs_buffer_pool {
    void **buffers;
    unsigned long *bitmap;
    unsigned int size;
    unsigned int count;
    spinlock_t lock;
    atomic_t in_use;
};

bool proxyfs_buffer_pool_init(struct proxyfs_buffer_pool *buffer_pool,
                              unsigned int count,
                              unsigned int size);
void proxyfs_buffer_pool_destroy(struct proxyfs_buffer_pool* pool);
int proxyfs_buffer_pool_in_use(struct proxyfs_buffer_pool* pool);


#endif //  !__PROXYFS_BUFFER_POOL_H__

