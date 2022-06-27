#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/inode.h"
#include "threads/synch.h"

#define BUFFER_CACHE_ENTRY_SIZE 64

struct buffer_head
{
    bool dirty;
    bool accessed;
    block_sector_t sector;
    void *data;
};

struct buffer_head *buffer_haed[BUFFER_CACHE_ENTRY_SIZE];

void *buffer_cache;

struct lock buffer_cache_lock;

int clock_head;

void bc_init(void);
void bc_free(void);
void bc_flush(struct buffer_head *bh);

#endif
