#include "filesys/cache.h"
#include <string.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct buffer_head *bc_find_empty(void);

void bc_init(void)
{
    /* Allocate buffer_cache. */
    buffer_cache = malloc(BLOCK_SECTOR_SIZE * BUFFER_CACHE_ENTRY_SIZE);

    /* Initialize buffer_head. */
    struct buffer_head *bh;
    for (int i = 0; i < BUFFER_CACHE_ENTRY_SIZE; i++)
    {
        bh = malloc(sizeof(struct buffer_head));
        bh->dirty = 0;
        bh->accessed = 0;
        bh->sector = -1;
        bh->data = buffer_cache + i * BLOCK_SECTOR_SIZE;
        buffer_haed[i] = bh;
    }

    lock_init(&buffer_cache_lock);
    clock_head = 0;
}

void bc_free(void)
{
    /* Destroy buffer_head. */
    for (int i = 0; i < BUFFER_CACHE_ENTRY_SIZE; i++)
    {
        bc_flush(buffer_haed[i]);
        free(buffer_haed[i]);
    }

    /* Destroy buffer_cache. */
    free(buffer_cache);
}

void bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
    lock_acquire(&buffer_cache_lock);

    struct buffer_head *bh = bc_lookup(sector_idx);
    if (bh == NULL)
    {
        bh = bc_find_empty();
        if (bh == NULL)
        {
            bh = bc_find_victim();
            bc_flush(bh);
        }
        /* Read data from disk to buffer cache. */
        block_read(fs_device, sector_idx, bh->data);
        bh->sector = sector_idx;
    }

    /* Read data from buffer cache to buffer. */
    memcpy(buffer + bytes_read, bh->data + sector_ofs, chunk_size);

    /* Update buffer head. */
    bh->accessed = 1;

    lock_release(&buffer_cache_lock);
}

void bc_write(block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
    lock_acquire(&buffer_cache_lock);

    struct buffer_head *bh = bc_lookup(sector_idx);
    if (bh == NULL)
    {
        bh = bc_find_empty();
        if (bh == NULL)
        {
            bh = bc_find_victim();
            bc_flush(bh);
        }
        /* Read data from disk to buffer cache. */
        block_read(fs_device, sector_idx, bh->data);
        bh->sector = sector_idx;
    }

    /* Read data from buffer cache to buffer. */
    memcpy(bh->data + sector_ofs, buffer + bytes_written, chunk_size);

    /* Update buffer head. */
    bh->dirty = 1;
    bh->accessed = 1;

    lock_release(&buffer_cache_lock);
}

struct buffer_head *bc_lookup(block_sector_t sector)
{
    struct buffer_head *bh;
    for (int i = 0; i < BUFFER_CACHE_ENTRY_SIZE; i++)
    {
        bh = buffer_haed[i];
        if (bh->sector == sector)
            return bh;
    }
    return NULL;
}

static struct buffer_head *bc_find_empty(void)
{
    struct buffer_head *bh;
    for (int i = 0; i < BUFFER_CACHE_ENTRY_SIZE; i++)
    {
        bh = buffer_haed[i];
        if (bh->sector == -1)
            return bh;
    }
    return NULL;
}

struct buffer_head *bc_find_victim(void)
{
    struct buffer_head *bh;
    while (true)
    {
        if (clock_head >= BUFFER_CACHE_ENTRY_SIZE)
            clock_head = 0;

        for (int i = clock_head; i < BUFFER_CACHE_ENTRY_SIZE; i++)
        {
            bh = buffer_haed[i];
            if (!bh->accessed)
            {
                clock_head = i + 1;
                return bh;
            }
            else
                bh->accessed = false;
        }
    }
}

void bc_flush(struct buffer_head *bh)
{
    /* Flush victim entry to disk. */
    if (bh->dirty)
        block_write(fs_device, bh->sector, bh->data);

    /* Release victim entry from buffer head. */
    bh->dirty = 0;
    bh->accessed = 0;
    bh->sector = -1;
    memset(bh->data, 0, BLOCK_SECTOR_SIZE);
}
