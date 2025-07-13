// File		:proxyfs_buffer_pool.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 01:27:18 2025
#include "lsm_netlink.h"

bool proxyfs_context_buffer_pool_init(struct proxyfs_buffer_pool *buffer_pool,
                                      unsigned int count,
                                      unsigned int size) {
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
        buffer_pool->bufs = bufs;
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

void proxyfs_context_buffer_pool_destroy(struct proxyfs_buffer_pool *buffer_pool) {
    unsigned int i;
    if (!buffer_pool || !buffer_pool->bufs || !buffer_pool->bitmap) {
        return;
    }
    for (i = 0; i < buffer_pool->count; i++) {
        kfree(buffer_pool->bufs[i]);
    }
    kfree(buffer_pool->bitmap);
    kfree(buffer_pool->bufs);
    buffer_pool->bufs = NULL;
    buffer_pool->bitmap = NULL;
    buffer_pool->count = 0;
    buffer_pool->size = 0;
}

void* proxyfs_context_buffer_pool_alloc(struct proxyfs_context_data *context_data) {
    if (context_data == NULL) {
        return NULL;
    }
    unsigned long flags;
    unsigned int i;
    void *buffer = NULL;

    spin_lock_irqsave(&context_data->buffer_pool.lock, flags);
    if ((i = find_first_zero_bit(context_data->buffer_pool.bitmap,
                                 context_data->buffer_pool.count)) < context_data->buffer_pool.count) {
        set_bit(i, context_data->buffer_pool.bitmap);
        buffer = context_data->buffer_pool.bufs[i];
        atomic_inc(&context_data->buffer_pool.in_use);
    }
    spin_unlock_irqrestore(&context_data->buffer_pool.lock, flags);

    return buffer;
}

bool proxyfs_context_buffer_pool_free(struct proxyfs_context_data *context_data,
                                      void* buffer) {
    if (context_data == NULL || buffer == NULL) {
        return false;
    }
    unsigned long flags;
    unsigned int i;
    bool found = false;

    spin_lock_irqsave(&context_data->buffer_pool.lock, flags);
    for (i = 0; i < context_data->buffer_pool.count; i++) {
        if (context_data->buffer_pool.bufs[i] == buffer) {
            if (test_and_clear_bit(i, context_data->buffer_pool.bitmap)) {
                atomic_dec(&context_data->buffer_pool.in_use);
                found = true;
            }
            break;
        }
    }
    spin_unlock_irqrestore(&context_data->buffer_pool.lock, flags);
    return found;
}

unsigned int proxyfs_context_buffer_pool_get_buffer_size(struct proxyfs_context_data *context_data) {
    if (context_data == NULL) {
        return 0;
    }
    return context_data->buffer_pool.size;
}

int proxyfs_buffer_pool_in_use(struct proxyfs_buffer_pool* pool)
{
    return atomic_read(&pool->in_use);
}
