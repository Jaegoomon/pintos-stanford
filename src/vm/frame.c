#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void lru_list_init(void)
{
    list_init(&lru_list.page_list);
    lock_init(&lru_list.lru_list_lock);
    lru_list.lru_clock = NULL;
}

void lru_list_push_back(struct page *page)
{
    lock_acquire(&lru_list.lru_list_lock);
    list_push_back(&lru_list.page_list, &page->lru);
    lock_release(&lru_list.lru_list_lock);
}

void lru_list_remove(struct page *page)
{
    lock_acquire(&lru_list.lru_list_lock);
    list_remove(&page->lru);
    lock_release(&lru_list.lru_list_lock);
}

struct page *alloc_page(enum palloc_flags flags)
{
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL)
        return NULL;

    void *kaddr = palloc_get_page(flags);

    page->kaddr = kaddr;
    page->thread = thread_current();
    lru_list_push_back(page);

    return page;
}

struct page *find_page(void *kaddr)
{
    if (!list_empty(&lru_list.page_list))
    {
        struct page *p;
        struct list_elem *e;
        for (e = list_begin(&lru_list.page_list); e != list_end(&lru_list.page_list); e = list_next(e))
        {
            p = list_entry(e, struct page, lru);
            if (p->kaddr == kaddr)
                return p;
        }
    }
    return NULL;
}

void free_page(struct page *page)
{
    lru_list_remove(page);
    palloc_free_page(page->kaddr);
    free(page);
}
