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
    /* Destroy buffer_cache. */
    free(buffer_cache);

    /* Destroy buffer_head. */
    for (int i = 0; i < BUFFER_CACHE_ENTRY_SIZE; i++)
    {
        bc_flush(buffer_haed[i]);
        free(buffer_haed[i]);
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
