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

// write_begin()
static int proxyfs_write_begin(struct file *file,
                               struct address_space *mapping,
                               loff_t pos,
                               unsigned len,
                               struct folio **foliop,
                               void **fsdata)
{
    pgoff_t index = pos >> PAGE_SHIFT;
    struct folio *folio = NULL;
    struct folio *lower_folio = NULL;
    struct file *lower_file = proxyfs_lower_file(file);
    struct address_space *lower_mapping = lower_file->f_mapping;
    struct proxyfs_folio_info *info = NULL;
    int ret = -ENOSYS;

    do {
        // 1. Invoke underlying FS (if available) to create its own folio object
        if (lower_mapping->a_ops && lower_mapping->a_ops->write_begin) {
            if ((ret = lower_mapping->a_ops->write_begin(lower_file,
                                                         lower_mapping,
                                                         pos,
                                                         len,
                                                         &lower_folio,
                                                         fsdata)) != 0)  {
                break;
            }
        } else {
            ret = -ENOSYS;
            break;
        }

        // 2. Get existing or create a new proxyfs level folio instance
        folio = filemap_grab_folio(mapping, index);
        if (IS_ERR(folio)) {
            ret = PTR_ERR(folio);
            break;
        }

        // 3. Bind lower FS folio wuth proxyfs folio via `private` data
        if ((info = kmalloc(sizeof(*info), GFP_KERNEL)) == NULL) {
            ret = -ENOMEM;
            break;
        }
        info->lower_folio = lower_folio;
        folio_attach_private(folio, info);

        // 4. proxyfs is ready
        *foliop = folio;
        ret = 0;
    } while (false);

    if (ret == 0) {
        return ret;
    }

    if (folio != NULL) {
        if (info != NULL) {
            folio_detach_private(folio);
        }

        folio_put(folio);
        folio = NULL;
    }

    if (lower_folio != NULL) {
        //
        // TBD: probably we need to call `folio_put(lower_folio)` if reference
        //      count is increased (extra check is required)
        if (lower_mapping->a_ops->release_folio) {
            lower_mapping->a_ops->release_folio(lower_folio, GFP_KERNEL);
        }
        lower_folio = NULL;
    }
    return ret;
}

// write_end()
static int proxyfs_write_end(struct file *file,
                             struct address_space *mapping,
                             loff_t pos,
                             unsigned len,
                             unsigned copied,
                             struct folio *folio,
                             void *fsdata)
{
    struct file *lower_file = proxyfs_lower_file(file);
    if (!lower_file) {
        return -EIO;
    }
    struct address_space *lower_mapping = lower_file->f_mapping;
    if (lower_mapping->a_ops && lower_mapping->a_ops->write_end) {
        return lower_mapping->a_ops->write_end(lower_file, lower_mapping, pos, len, copied, folio, fsdata);
    }
    return -ENOSYS;
}

// bmap()
static sector_t proxyfs_bmap(struct address_space *mapping,
                             sector_t block)
{
    if (mapping) {
        struct inode *inode = mapping->host;
        struct inode *lower_inode = proxyfs_lower_inode(inode);
        struct address_space *lower_mapping = lower_inode ? lower_inode->i_mapping : NULL;
        if (lower_mapping && lower_mapping->a_ops && lower_mapping->a_ops->bmap) {
            return lower_mapping->a_ops->bmap(lower_mapping, block);
        }
    }
    return 0;
}

// invalidate_folio()
static void proxyfs_invalidate_folio(struct folio *,
                                     size_t offset,
                                     size_t len)
{
}

// release_folio()
static  bool proxyfs_release_folio(struct folio *folio,
                                   gfp_t gfp)
{
    struct proxyfs_folio_info *info = (struct proxyfs_folio_info *)folio->private;
    if (info != NULL) {
        kfree(info);
        folio->private = NULL;
    }
    return true;
}

// free_folio()
static void proxyfs_free_folio(struct folio *folio)
{
    struct proxyfs_folio_info *info = (struct proxyfs_folio_info *)folio->private;
    if (info != NULL) {
        kfree(info);
        folio->private = NULL;
    }
}

// direct_IO()
static ssize_t proxyfs_direct_IO(struct kiocb *iocb,
                                 struct iov_iter *iter)
{
    if (iocb == NULL) {
        return -EINVAL;
    }
    struct file *lower_file = proxyfs_lower_file(iocb->ki_filp);
    if (lower_file &&
        lower_file->f_mapping &&
        lower_file->f_mapping->a_ops &&
        lower_file->f_mapping->a_ops->direct_IO) {
        struct kiocb lower_iocb = *iocb;
        lower_iocb.ki_filp = lower_file;
        return lower_file->f_mapping->a_ops->direct_IO(&lower_iocb, iter);
    }
    return -ENOSYS;
}

// migrate_folio()
static int proxyfs_migrate_folio(struct address_space *,
                                 struct folio *dst,
                                 struct folio *src,
                                 enum migrate_mode mode)
{
    int ret = -1;

    return ret;
}

// launder_folio()
static int proxyfs_launder_folio(struct folio *)
{
    int ret = -1;

    return ret;
}

// is_partially_uptodate()
static bool proxyfs_is_partially_uptodate(struct folio *,
                                          size_t from,
                                          size_t count)
{
    bool ret = false;

    return ret;
}

// is_dirty_writeback()
static void proxyfs_is_dirty_writeback(struct folio *,
                                       bool *dirty,
                                       bool *wb)
{
}

// error_remove_folio()
static int proxyfs_error_remove_folio(struct address_space *,
                                      struct folio *)
{
    int ret = -1;

    return ret;
}

// swap_activate()
static int proxyfs_swap_activate(struct swap_info_struct *sis,
                                 struct file *file,
                                 sector_t *span)
{
    int ret = -1;

    return ret;
}

// swap_deactivate()
static void proxyfs_swap_deactivate(struct file *file)
{
}

// swap_rw()
static int proxyfs_swap_rw(struct kiocb *iocb,
                           struct iov_iter *iter)
{
    int ret = -1;

    return ret;
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
    .write_begin = proxyfs_write_begin,
	// int (*write_end)(struct file *, struct address_space *mapping, loff_t pos, unsigned len, unsigned copied, struct folio *folio, void *fsdata);
    .write_end = proxyfs_write_end,
	// sector_t (*bmap)(struct address_space *, sector_t);
    .bmap = proxyfs_bmap,
	// void (*invalidate_folio) (struct folio *, size_t offset, size_t len);
    .invalidate_folio = NULL,
	// bool (*release_folio)(struct folio *, gfp_t);
    .release_folio = NULL,
	// void (*free_folio)(struct folio *folio);
    .free_folio = NULL,
	//ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter);
    .direct_IO = proxyfs_direct_IO,
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
