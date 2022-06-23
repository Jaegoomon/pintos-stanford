#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

struct page *find_victim()
{
    struct list_elem *clock = lru_list.lru_clock;
    if (clock == NULL || clock->next == NULL)
        clock = list_begin(&lru_list.page_list);

    struct page *p;
    while (true)
    {
        if (clock == list_end(&lru_list.page_list))
            clock = list_begin(&lru_list.page_list);

        p = list_entry(clock, struct page, lru);
        bool is_accessed = pagedir_is_accessed(p->thread->pagedir, p->vme->vaddr);
        if (is_accessed)
            pagedir_set_accessed(p->thread->pagedir, p->vme->vaddr, !is_accessed);
        else
        {
            if (!p->pinned)
            {
                lru_list.lru_clock = list_next(clock);
                return p;
            }
        }
        clock = list_next(clock);
    }
}

void swap_bitmap_init()
{
    size_t sec_size = block_size(block_get_role(BLOCK_SWAP)) * BLOCK_SECTOR_SIZE / PGSIZE;
    swap_partition.bitmap = bitmap_create(sec_size);
    lock_init(&swap_partition.swap_lock);
}

size_t swap_out(void *kaddr)
{
    struct bitmap *swap_bitmap = swap_partition.bitmap;
    struct block *swap_block = block_get_role(BLOCK_SWAP);
    size_t sec_idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

    for (size_t i = 0; i < 8; i++)
        block_write(swap_block, sec_idx * 8 + i, kaddr + BLOCK_SECTOR_SIZE * i);
    return sec_idx;
}

void swap_in(size_t sec_idx, void *kaddr)
{
    struct bitmap *swap_bitmap = swap_partition.bitmap;
    struct block *swap_block = block_get_role(BLOCK_SWAP);

    for (size_t i = 0; i < 8; i++)
        block_read(swap_block, sec_idx * 8 + i, kaddr + BLOCK_SECTOR_SIZE * i);
    bitmap_set(swap_bitmap, sec_idx, false);
}
