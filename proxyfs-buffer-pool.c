// File		:proxyfs-buffer-pool.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:27:18 2025
#include "proxyfs.h"

bool proxyfs_buffer_pool_init(struct proxyfs_buffer_pool *buffer_pool,
                              unsigned int count,
                              unsigned int size)
{
    if (buffer_pool == NULL || count == 0 || size == 0) {
        return false;
    }

    unsigned int i;
    void** bufs = NULL;
    unsigned long* bitmap = NULL;
    bool init_res = false;

    do {
        if ((bufs = kcalloc(count, sizeof(void *), GFP_KERNEL)) == NULL) {
            pr_err("%s: unable to allocate memory segement for memory buffer pool",
                   MODULE_NAME);
            break;
        }
        if ((bitmap = kcalloc(BITS_TO_LONGS(count), sizeof(unsigned long), GFP_KERNEL)) == NULL) {
            pr_err("%s: unable to allocate bitmap structure for memory buffer pool",
                   MODULE_NAME);
            break;
        }
        for (i = 0; i < count; i++) {
            if ((bufs[i] = kmalloc(size, GFP_KERNEL)) == NULL) {
                pr_err("%s: unable to allocate %d buffer for memory buffer pool",
                       MODULE_NAME,
                       i);
                break;
            }
        }
        if (i < count) {
            break;
        }
        init_res = true;
    } while (false);

    if (init_res == true) {
        buffer_pool->buffers = bufs;
        buffer_pool->bitmap = bitmap;
        buffer_pool->size = size;
        buffer_pool->count = count;
        bitmap_zero(buffer_pool->bitmap, count);
        spin_lock_init(&buffer_pool->lock);
        atomic_set(&buffer_pool->in_use, 0);
        pr_info("%s: memory buffer pool with %u buffers %u bytes length is ready for use",
                MODULE_NAME,
                buffer_pool->count,
                buffer_pool->size);
        return true;
    }
    while (i--) {
        kfree(bufs[i]);
    }
    if (bitmap) {
        kfree(bitmap);
    }
    if (bufs) {
        kfree(bufs);
    }
    return false;
}

void proxyfs_buffer_pool_destroy(struct proxyfs_buffer_pool *buffer_pool)
{
    unsigned int i;
    if (!buffer_pool || !buffer_pool->buffers || !buffer_pool->bitmap) {
        return;
    }
    for (i = 0; i < buffer_pool->count; i++) {
        kfree(buffer_pool->buffers[i]);
    }
    kfree(buffer_pool->bitmap);
    kfree(buffer_pool->buffers);
    buffer_pool->buffers = NULL;
    buffer_pool->bitmap = NULL;
    buffer_pool->count = 0;
    buffer_pool->size = 0;
}

int proxyfs_buffer_pool_in_use(struct proxyfs_buffer_pool* pool)
{
    return atomic_read(&pool->in_use);
}
