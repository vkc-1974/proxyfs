// File		:proxyfs_procfs.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 13:09:17 2025
#include "proxyfs.h"

static int proxyfs_procfs_unitid_show(struct seq_file* m, void* v) {
    seq_printf(m, "%d\n", NETLINK_USER);
    return 0;
}

static int proxyfs_procfs_filters_show(struct seq_file* m, void* v) {
    seq_printf(m, "%s\n", "filters - NOT IMPLEMENTED YET");
    return 0;
}

static int proxyfs_procfs_pids_show(struct seq_file* m, void* v) {
    seq_printf(m, "%s\n", "pids - NOT IMPLEMENTED YET");
    return 0;
}

static int proxyfs_procfs_unitid_open(struct inode* inode, struct file* file) {
    return single_open(file, proxyfs_procfs_unitid_show, NULL);
}

static int proxyfs_procfs_filters_open(struct inode* inode, struct file* file) {
    return single_open(file, proxyfs_procfs_filters_show, NULL);
}

static int proxyfs_procfs_pids_open(struct inode* inode, struct file* file) {
    return single_open(file, proxyfs_procfs_pids_show, NULL);
}

static const struct proc_ops proxyfs_procfs_unitid_ops = {
    .proc_open = proxyfs_procfs_unitid_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops proxyfs_procfs_filters_ops = {
    .proc_open = proxyfs_procfs_filters_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops proxyfs_procfs_pids_ops = {
    .proc_open = proxyfs_procfs_pids_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

struct proc_dir_entry* proxyfs_procfs_setup(void) {
    struct proc_dir_entry* lsm_proc_dir;

    if ((lsm_proc_dir = proc_mkdir(PROCFS_PROXYFS_DIR, NULL)) == NULL) {
        pr_err("%s: unable to create /proc/%s\n",
               MODULE_NAME,
               PROCFS_PROXYFS_DIR);
        return NULL;
    }

    pr_info("%s: created /proc/%s\n",
            MODULE_NAME,
            PROCFS_PROXYFS_DIR);

    proc_create(PROCFS_PROXYFS_UNIT_ID, 0444, lsm_proc_dir, &proxyfs_procfs_unitid_ops);
    pr_info("%s: created /proc/%s/%s\n",
            MODULE_NAME,
            PROCFS_PROXYFS_DIR,
            PROCFS_PROXYFS_UNIT_ID);
    proc_create(PROCFS_PROXYFS_FILTERS, 0444, lsm_proc_dir, &proxyfs_procfs_filters_ops);
    pr_info("%s: created /proc/%s/%s\n",
            MODULE_NAME,
            PROCFS_PROXYFS_DIR,
            PROCFS_PROXYFS_FILTERS);
    proc_create(PROCFS_PROXYFS_PIDS, 0444, lsm_proc_dir, &proxyfs_procfs_pids_ops);
    pr_info("%s: created /proc/%s/%s\n",
            MODULE_NAME,
            PROCFS_PROXYFS_DIR,
            PROCFS_PROXYFS_PIDS);

    return lsm_proc_dir;
}

void proxyfs_procfs_release(void) {
    remove_proc_subtree(PROCFS_PROXYFS_DIR, NULL);
}
