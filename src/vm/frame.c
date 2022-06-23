#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

void lru_list_init(void)
{
    list_init(&lru_list.page_list);
    lock_init(&lru_list.lru_list_lock);
    lru_list.lru_clock = NULL;
}

void lru_list_push_back(struct page *page)
{
    list_push_back(&lru_list.page_list, &page->lru);
}

void lru_list_remove(struct page *page)
{
    list_remove(&page->lru);
}

struct page *alloc_page(enum palloc_flags flags)
{
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL)
        return NULL;

    void *kaddr;
    do
    {
        kaddr = palloc_get_page(flags);
        if (kaddr != NULL)
            break;

        struct page *victim = find_victim();
        struct vm_entry *vme = victim->vme;
        struct thread *t = victim->thread;
        bool is_dirty = pagedir_is_dirty(t->pagedir, victim->vme->vaddr);

        switch (vme->type)
        {
        case VM_BIN:
        {
            if (is_dirty)
                vme->sec_idx = swap_out(victim->kaddr);
            vme->type = VM_ANON;
            break;
        }
        case VM_FILE:
        {
            if (is_dirty)
                file_write_at(vme->file, victim->kaddr, PGSIZE, vme->offset);
            break;
        }
        case VM_ANON:
        {
            vme->sec_idx = swap_out(victim->kaddr);
            break;
        }
        default:
            break;
        }

        free_page(victim);
        pagedir_clear_page(t->pagedir, vme->vaddr);
    } while (kaddr == NULL);

    page->kaddr = kaddr;
    page->thread = thread_current();
    page->pinned = false;
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
