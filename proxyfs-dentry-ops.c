// File		:proxyfs-dentry-ops.c
// Author	:Victor Kovalevich
// Created	:Wed Jul 16 01:32:40 2025
#include "proxyfs.h"
#include <linux/dcache.h>

const struct dentry_operations proxyfs_dentry_ops = {
	// int (*d_revalidate)(struct inode *, const struct qstr *, struct dentry *, unsigned int);
    .d_revalidate = NULL,
	// int (*d_weak_revalidate)(struct dentry *, unsigned int);
    .d_weak_revalidate = NULL,
	// int (*d_hash)(const struct dentry *, struct qstr *);
    .d_hash = NULL,
	// int (*d_compare)(const struct dentry *, unsigned int, const char *, const struct qstr *);
    .d_compare = NULL,
	// int (*d_delete)(const struct dentry *);
    .d_delete = NULL,
	// int (*d_init)(struct dentry *);
    .d_init = NULL,
	// void (*d_release)(struct dentry *);
    .d_release = NULL,
	// void (*d_prune)(struct dentry *);
    .d_prune = NULL,
	// void (*d_iput)(struct dentry *, struct inode *);
    .d_iput = NULL,
	// char *(*d_dname)(struct dentry *, char *, int);
    .d_dname = NULL,
	// struct vfsmount *(*d_automount)(struct path *);
    .d_automount = NULL,
	// int (*d_manage)(const struct path *, bool);
    .d_manage = NULL,
	// struct dentry *(*d_real)(struct dentry *, enum d_real_type type);
    .d_real = NULL,
	// bool (*d_unalias_trylock)(const struct dentry *);
    .d_unalias_trylock = NULL,
	// void (*d_unalias_unlock)(const struct dentry *);
    .d_unalias_unlock = NULL,
};
