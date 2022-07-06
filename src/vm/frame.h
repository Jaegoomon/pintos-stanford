#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"

struct lru_list
{
    struct list page_list;
    struct lock lru_list_lock;
    struct list_elem *lru_clock;
};

struct lru_list lru_list;

void lru_list_init(void);
void lru_list_push_back(struct page *page);
void lru_list_remove(struct page *page);

struct page *alloc_page(enum palloc_flags flags);
struct page *find_page(void *kaddr);
void free_page(struct page *page);
void free_thread_pages(struct thread *t);

#endif