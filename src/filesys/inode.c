#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

enum direct_t
{
    DIRECT,
    INDIRECT,
    DOUBLE_INDIRECT,
    OUT_LIMIT
};

struct sector_location
{
    enum direct_t directness;
    int index1;
    int index2;
};

struct inode_indirect_block
{
    block_sector_t map_table[INDIRECT_BLOCK_ENTRIES];
};

static struct inode_indirect_block *init_ind_blk(void)
{
    struct inode_indirect_block *ind_blk = malloc(BLOCK_SECTOR_SIZE);
    for (int i = 0; i < INDIRECT_BLOCK_ENTRIES; i++)
        ind_blk->map_table[i] = 0;

    return ind_blk;
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

static void locate_byte(off_t pos, struct sector_location *sec_loc);
static bool register_sector(struct inode_disk *inode_disk,
                            block_sector_t new_sector, struct sector_location sec_loc);
static bool inode_update_file_length(struct inode_disk *inode_disk,
                                     off_t start_pos, off_t end_pos);
static void free_inode_sectors(struct inode_disk *inode_disk);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector(const struct inode_disk *inode_disk, off_t pos)
{
    block_sector_t result_sec = 0;

    if (pos >= inode_disk->length)
        return result_sec;

    struct sector_location sec_loc;
    locate_byte(pos, &sec_loc);

    switch (sec_loc.directness)
    {
    case DIRECT:
    {
        result_sec = inode_disk->direct_map_table[sec_loc.index1];
        break;
    }
    case INDIRECT:
    {
        struct inode_indirect_block *ind_blk = malloc(BLOCK_SECTOR_SIZE);
        bc_read(inode_disk->indirect_block_sec, ind_blk, 0, BLOCK_SECTOR_SIZE, 0);

        result_sec = ind_blk->map_table[sec_loc.index1];
        free(ind_blk);
        break;
    }
    case DOUBLE_INDIRECT:
    {
        struct inode_indirect_block *ind_blk1 = malloc(BLOCK_SECTOR_SIZE);
        struct inode_indirect_block *ind_blk2 = malloc(BLOCK_SECTOR_SIZE);
        bc_read(inode_disk->double_indirect_block_sec, ind_blk1, 0, BLOCK_SECTOR_SIZE, 0);
        bc_read(ind_blk1->map_table[sec_loc.index1], ind_blk2, 0, BLOCK_SECTOR_SIZE, 0);

        result_sec = ind_blk2->map_table[sec_loc.index2];
        free(ind_blk1);
        free(ind_blk2);
        break;
    }
    default:
        break;
    }

    return result_sec;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void)
{
    list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir)
{
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL)
    {
        disk_inode->length = 0;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->is_dir = is_dir;
        for (int i = 0; i < DIRECT_BLOCK_ENTRIES; i++)
            disk_inode->direct_map_table[i] = 0;
        disk_inode->indirect_block_sec = 0;
        disk_inode->double_indirect_block_sec = 0;

        success = inode_update_file_length(disk_inode, 0, length);
        bc_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
        free(disk_inode);
    }
    return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e))
    {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector)
        {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    lock_init(&inode->inode_lock);
    return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
    return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode)
{
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0)
    {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed)
        {
            struct inode_disk *inode_disk = malloc(BLOCK_SECTOR_SIZE);
            get_disk_inode(inode, inode_disk);

            free_inode_sectors(inode_disk);
            free_map_release(inode->sector, 1);
            free(inode_disk);
        }

        free(inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode *inode)
{
    ASSERT(inode != NULL);
    inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    uint8_t *bounce = NULL;

    struct inode_disk *inode_disk = malloc(BLOCK_SECTOR_SIZE);
    get_disk_inode(inode, inode_disk);

    while (size > 0)
    {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode_disk, offset);
        if (sector_idx == 0)
            break;

        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        /* Inode read by using buffer cache. */
        bc_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    free(inode_disk);

    return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
                     off_t offset)
{
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    uint8_t *bounce = NULL;

    if (inode->deny_write_cnt)
        return 0;

    struct inode_disk *disk_inode = malloc(BLOCK_SECTOR_SIZE);
    if (disk_inode == NULL)
        return bytes_written;

    get_disk_inode(inode, disk_inode);

    lock_acquire(&inode->inode_lock);

    int old_length = disk_inode->length;
    int write_end = offset + size;
    if (write_end > old_length)
    {
        inode_update_file_length(disk_inode, old_length, write_end);
        bc_write(inode->sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
    }

    lock_release(&inode->inode_lock);

    while (size > 0)
    {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(disk_inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        /* Inode read by using buffer cache. */
        bc_write(sector_idx, buffer, bytes_written, chunk_size, sector_ofs);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }
    free(disk_inode);

    return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode)
{
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode)
{
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode)
{
    struct inode_disk *inode_disk = malloc(BLOCK_SECTOR_SIZE);
    get_disk_inode(inode, inode_disk);
    off_t length = inode_disk->length;

    free(inode_disk);
    return length;
}

void get_disk_inode(const struct inode *inode, struct inode_disk *inode_disk)
{
    bc_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
}

static void locate_byte(off_t pos, struct sector_location *sec_loc)
{
    int pos_sec = pos / BLOCK_SECTOR_SIZE;

    if (pos_sec < DIRECT_BLOCK_ENTRIES)
    {
        sec_loc->directness = DIRECT;
        sec_loc->index1 = pos_sec;
    }
    else if (pos_sec < DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES)
    {
        sec_loc->directness = INDIRECT;
        sec_loc->index1 = pos_sec - DIRECT_BLOCK_ENTRIES;
    }
    else if (pos_sec < DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES * (1 + INDIRECT_BLOCK_ENTRIES))
    {
        sec_loc->directness = DOUBLE_INDIRECT;
        off_t remainder = pos_sec - DIRECT_BLOCK_ENTRIES - INDIRECT_BLOCK_ENTRIES;
        sec_loc->index1 = remainder / INDIRECT_BLOCK_ENTRIES;
        sec_loc->index2 = remainder % INDIRECT_BLOCK_ENTRIES;
    }
    else
        sec_loc->directness = OUT_LIMIT;
}

static bool register_sector(struct inode_disk *inode_disk,
                            block_sector_t new_sector, struct sector_location sec_loc)
{
    switch (sec_loc.directness)
    {
    case DIRECT:
    {
        inode_disk->direct_map_table[sec_loc.index1] = new_sector;
        break;
    }
    case INDIRECT:
    {
        struct inode_indirect_block *ind_blk = init_ind_blk();
        if (inode_disk->indirect_block_sec == 0)
            free_map_allocate(1, &inode_disk->indirect_block_sec);
        else
            bc_read(inode_disk->indirect_block_sec, ind_blk, 0, BLOCK_SECTOR_SIZE, 0);

        ind_blk->map_table[sec_loc.index1] = new_sector;
        bc_write(inode_disk->indirect_block_sec, ind_blk, 0, BLOCK_SECTOR_SIZE, 0);

        free(ind_blk);
        break;
    }
    case DOUBLE_INDIRECT:
    {
        struct inode_indirect_block *ind_blk1 = init_ind_blk();
        struct inode_indirect_block *ind_blk2 = init_ind_blk();

        if (inode_disk->double_indirect_block_sec == 0)
        {
            free_map_allocate(1, &inode_disk->double_indirect_block_sec);
            free_map_allocate(1, &ind_blk1->map_table[sec_loc.index1]);
        }
        else
        {
            bc_read(inode_disk->double_indirect_block_sec, ind_blk1, 0, BLOCK_SECTOR_SIZE, 0);
            if (ind_blk1->map_table[sec_loc.index1] == 0)
                free_map_allocate(1, &ind_blk1->map_table[sec_loc.index1]);
            else
                bc_read(ind_blk1->map_table[sec_loc.index1], ind_blk2, 0, BLOCK_SECTOR_SIZE, 0);
        }

        ind_blk2->map_table[sec_loc.index2] = new_sector;
        bc_write(inode_disk->double_indirect_block_sec, ind_blk1, 0, BLOCK_SECTOR_SIZE, 0);
        bc_write(ind_blk1->map_table[sec_loc.index1], ind_blk2, 0, BLOCK_SECTOR_SIZE, 0);

        free(ind_blk1);
        free(ind_blk2);
        break;
    }
    default:
        return false;
    }

    return true;
}

static bool inode_update_file_length(struct inode_disk *inode_disk,
                                     off_t start_pos, off_t end_pos)
{
    size_t size = end_pos - start_pos;
    off_t offset = start_pos;

    while (size > 0)
    {
        int chunk_size = size > BLOCK_SECTOR_SIZE ? BLOCK_SECTOR_SIZE : size;
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;
        if (sector_ofs > 0)
        {
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            chunk_size = chunk_size < sector_left ? chunk_size : sector_left;
        }
        else
        {
            int sector_idx;
            if (free_map_allocate(1, &sector_idx))
            {
                struct sector_location sec_loc;
                locate_byte(offset, &sec_loc);
                register_sector(inode_disk, sector_idx, sec_loc);
            }
            else
                return false;

            static char zeros[BLOCK_SECTOR_SIZE];
            bc_write(sector_idx, zeros, 0, BLOCK_SECTOR_SIZE, 0);
        }

        size -= chunk_size;
        offset += chunk_size;
    }

    inode_disk->length += end_pos - start_pos;
    return true;
}

static void free_inode_sectors(struct inode_disk *inode_disk)
{
    /* Direct */
    int i = 0;
    while (inode_disk->direct_map_table[i] > 0)
    {
        free_map_release(inode_disk->direct_map_table[i], 1);
        i++;
    }

    /* Indirect */
    if (inode_disk->indirect_block_sec > 0)
    {
        struct inode_indirect_block *ind_blk = malloc(BLOCK_SECTOR_SIZE);
        bc_read(inode_disk->indirect_block_sec, ind_blk, 0, BLOCK_SECTOR_SIZE, 0);

        int i = 0;
        while (ind_blk->map_table[i] > 0)
        {
            free_map_release(ind_blk->map_table[i], 1);
            i++;
        }
        free(ind_blk);
        free_map_release(inode_disk->indirect_block_sec, 1);
    }

    /* Double indirect */
    if (inode_disk->double_indirect_block_sec > 0)
    {
        struct inode_indirect_block *ind_blk1 = malloc(BLOCK_SECTOR_SIZE);
        bc_read(inode_disk->indirect_block_sec, ind_blk1, 0, BLOCK_SECTOR_SIZE, 0);

        int i = 0;
        while (ind_blk1->map_table[i] > 0)
        {
            struct inode_indirect_block *ind_blk2 = malloc(BLOCK_SECTOR_SIZE);
            bc_read(ind_blk1->map_table[i], ind_blk2, 0, BLOCK_SECTOR_SIZE, 0);

            int j = 0;
            while (ind_blk2->map_table[j] > 0)
            {
                free_map_release(ind_blk2->map_table[j], 1);
                j++;
            }
            free(ind_blk2);

            free_map_release(ind_blk1->map_table[i], 1);
            i++;
        }
        free(ind_blk1);
        free_map_release(inode_disk->double_indirect_block_sec, 1);
    }
}

bool inode_is_dir(const struct inode *inode)
{
    uint32_t is_dir;

    struct inode_disk *disk_inode = malloc(BLOCK_SECTOR_SIZE);
    get_disk_inode(inode, disk_inode);
    is_dir = disk_inode->is_dir;
    free(disk_inode);

    return (bool)is_dir;
}
