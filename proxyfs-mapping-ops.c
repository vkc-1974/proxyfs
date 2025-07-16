// File		:proxyfs-mapping-ops.c
// Author	:Victor Kovalevich
// Created	:Thu Jul 17 03:29:38 2025
#include "proxyfs.h"

const struct address_space_operations proxyfs_mapping_ops = {
    // int (*writepage)(struct page *page, struct writeback_control *wbc);
    .writepage = NULL,
	// int (*read_folio)(struct file *, struct folio *);
    .read_folio = NULL,
	// int (*writepages)(struct address_space *, struct writeback_control *);
    .writepages = NULL,
	// bool (*dirty_folio)(struct address_space *, struct folio *);
    .dirty_folio = NULL,
	// void (*readahead)(struct readahead_control *);
    .readahead = NULL,
	// int (*write_begin)(struct file *, struct address_space *mapping, loff_t pos, unsigned len, struct folio **foliop, void **fsdata);
    .write_begin = NULL,
	// int (*write_end)(struct file *, struct address_space *mapping, loff_t pos, unsigned len, unsigned copied, struct folio *folio, void *fsdata);
    .write_end = NULL,
	// sector_t (*bmap)(struct address_space *, sector_t);
    .bmap = NULL,
	// void (*invalidate_folio) (struct folio *, size_t offset, size_t len);
    .invalidate_folio = NULL,
	// bool (*release_folio)(struct folio *, gfp_t);
    .release_folio = NULL,
	// void (*free_folio)(struct folio *folio);
    .free_folio = NULL,
	//ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter);
    .direct_IO = NULL,
	// int (*migrate_folio)(struct address_space *, struct folio *dst, struct folio *src, enum migrate_mode);
    .migrate_folio = NULL,
	// int (*launder_folio)(struct folio *);
    .launder_folio = NULL,
	// bool (*is_partially_uptodate) (struct folio *, size_t from, size_t count);
    .is_partially_uptodate = NULL,
	// void (*is_dirty_writeback) (struct folio *, bool *dirty, bool *wb);
    .is_dirty_writeback = NULL,
	//int (*error_remove_folio)(struct address_space *, struct folio *);
    .error_remove_folio = NULL,
	// int (*swap_activate)(struct swap_info_struct *sis, struct file *file, sector_t *span);
    .swap_activate = NULL,
	// void (*swap_deactivate)(struct file *file);
    .swap_deactivate = NULL,
	// int (*swap_rw)(struct kiocb *iocb, struct iov_iter *iter);
    .swap_rw = NULL
};
