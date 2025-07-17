// File		:proxyfs-mapping-ops.c
// Author	:Victor Kovalevich
// Created	:Thu Jul 17 03:29:38 2025
#include <linux/pagemap.h>
#include "proxyfs.h"

// writepage()
static int proxyfs_writepage(struct page *page,
                             struct writeback_control *wbc)
{
    if (page && page->mapping) {
        struct inode *inode = page->mapping->host;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        if (lower_inode &&
            lower_inode->i_mapping &&
            lower_inode->i_mapping->a_ops &&
            lower_inode->i_mapping->a_ops->writepage) {
            return lower_inode->i_mapping->a_ops->writepage(page, wbc);
        }
    }
    return -EIO;
}

// read_folio
static int proxyfs_read_folio(struct file *file,
                              struct folio *folio)
{
    if (file) {
        struct inode *inode = file->f_inode;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        struct file *lower_file = proxyfs_lower_file(file);
        if (lower_inode &&
            lower_inode->i_mapping &&
            lower_inode->i_mapping->a_ops &&
            lower_inode->i_mapping->a_ops->read_folio &&
            lower_file)
        {
            return lower_inode->i_mapping->a_ops->read_folio(lower_file, folio);
        }
    }
    return -EIO;
}

// writepages()
static int proxyfs_writepages(struct address_space *mapping,
                              struct writeback_control *wbc)
{
    if (mapping) {
        struct inode *inode = mapping->host;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        struct address_space *lower_mapping = lower_inode ? lower_inode->i_mapping : NULL;
        if (lower_mapping &&
            lower_mapping->a_ops &&
            lower_mapping->a_ops->writepages)
        {
            return lower_mapping->a_ops->writepages(lower_mapping, wbc);
        }
    }
    return -EIO;
}

// dirty_folio()
static bool proxyfs_dirty_folio(struct address_space *mapping,
                                struct folio *folio)
{
    if (mapping) {
        struct inode *inode = mapping->host;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        struct address_space *lower_mapping = lower_inode ? lower_inode->i_mapping : NULL;
        if (lower_mapping &&
            lower_mapping->a_ops &&
            lower_mapping->a_ops->dirty_folio)
        {
            return lower_mapping->a_ops->dirty_folio(lower_mapping, folio);
        }
    }
    return false;
}

// readahead()
static void proxyfs_readahead(struct readahead_control *rac)
{
    if (rac &&
        rac->mapping) {
        struct inode *inode = rac->mapping->host;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        struct address_space *lower_mapping = lower_inode ? lower_inode->i_mapping : NULL;
        if (lower_mapping && lower_mapping->a_ops &&
            lower_mapping->a_ops->readahead)
        {
            lower_mapping->a_ops->readahead(rac);
        }
    }
}

const struct address_space_operations proxyfs_mapping_ops = {
    // int (*writepage)(struct page *page, struct writeback_control *wbc);
    .writepage = proxyfs_writepage,
	// int (*read_folio)(struct file *, struct folio *);
    .read_folio = proxyfs_read_folio,
	// int (*writepages)(struct address_space *, struct writeback_control *);
    .writepages = proxyfs_writepages,
	// bool (*dirty_folio)(struct address_space *, struct folio *);
    .dirty_folio = proxyfs_dirty_folio,
	// void (*readahead)(struct readahead_control *);
    .readahead = proxyfs_readahead,
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
