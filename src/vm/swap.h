#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "vm/page.h"

struct swap_partition
{
    struct bitmap *bitmap;
    struct lock swap_lock;
};

struct swap_partition swap_partition;

struct page *find_victim();
void swap_bitmap_init();
size_t swap_out(void *kaddr);
void swap_in(size_t sec_idx, void *kaddr);

#endif