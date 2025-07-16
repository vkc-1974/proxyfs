// File		:proxyfs-context.c
// Author	:Victor Kovalevich
// Created	:Wed Jul 16 13:47:36 2025
#include "proxyfs.h"

static struct proxyfs_context_data proxyfs_context = {
    .nl_socket = NULL,
    .client_pid = {
        .counter = 0
    },
    .proc_dir = NULL,
    .buffer_pool = {
        .buffers = NULL,
        .bitmap = NULL,
        .size = 0,
        .count = 0
    },
    .running_state = {
        .counter = 0
    },
    .handler_counter = {
        .counter = 0
    }
};

int proxyfs_context_set_client_pid(const int new_client_pid)
{
    int res = atomic_read(&proxyfs_context.client_pid);
    atomic_set(&proxyfs_context.client_pid, new_client_pid);
    return res;
}

int proxyfs_context_get_client_pid(void)
{
    return atomic_read(&proxyfs_context.client_pid);
}

bool proxyfs_context_check_uid(const int uid)
{
    // TBD: this function is intended to check if an incomming message received
    //      via NETLINK channel is from allowed user account (will be implemented
    //      later)
    return true;
}

bool proxyfs_context_check_is_running(void)
{
    return atomic_read(&proxyfs_context.running_state);
}

void proxyfs_context_handler_counter_increment(void)
{
    atomic_inc(&proxyfs_context.handler_counter);
}

void proxyfs_context_handler_counter_decrement(void)
{
    atomic_dec(&proxyfs_context.handler_counter);
}

struct sock* proxyfs_context_get_nl_socket(void)
{
    return proxyfs_context.nl_socket;
}

void* proxyfs_context_buffer_pool_alloc(struct proxyfs_context_data *context_data)
{
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
        buffer = context_data->buffer_pool.buffers[i];
        atomic_inc(&context_data->buffer_pool.in_use);
    }
    spin_unlock_irqrestore(&context_data->buffer_pool.lock, flags);

    return buffer;
}

bool proxyfs_context_buffer_pool_free(struct proxyfs_context_data *context_data,
                                      void* buffer)
{
    if (context_data == NULL || buffer == NULL) {
        return false;
    }
    unsigned long flags;
    unsigned int i;
    bool found = false;

    spin_lock_irqsave(&context_data->buffer_pool.lock, flags);
    for (i = 0; i < context_data->buffer_pool.count; i++) {
        if (context_data->buffer_pool.buffers[i] == buffer) {
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

unsigned int proxyfs_context_buffer_pool_get_buffer_size(struct proxyfs_context_data *context_data)
{
    if (context_data == NULL) {
        return 0;
    }
    return context_data->buffer_pool.size;
}
