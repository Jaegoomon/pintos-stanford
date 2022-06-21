#include "vm/page.h"
#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

static unsigned vm_hash_func(const struct hash_elem *e, void *aux);
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void vm_destroy_func(struct hash_elem *e, void *aux);

void vm_init(struct hash *vm)
{
    hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

void vm_destory(struct hash *vm)
{
    lock_acquire(&lru_list.lru_list_lock);
    hash_destroy(vm, vm_destroy_func);
    lock_release(&lru_list.lru_list_lock);
}

struct vm_entry *find_vme(void *vaddr)
{
    struct hash *vm = &thread_current()->vm;
    struct hash_iterator i;

    hash_first(&i, vm);
    while (hash_next(&i))
    {
        struct vm_entry *vme = hash_entry(hash_cur(&i), struct vm_entry, elem);
        if (pg_no(vme->vaddr) == pg_no(vaddr))
            return vme;
    }
    return NULL;
}

bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
    struct hash_elem *result = hash_insert(vm, &vme->elem);
    if (result == NULL)
        return true;
    return false;
}

bool delete_vme(struct hash *vm, struct vm_entry *vme)
{
    struct hash_elem *result = hash_delete(vm, &vme->elem);
    if (result == NULL)
        return false;
    return true;
}

bool load_file(void *kaddr, struct vm_entry *vme)
{
    file_seek(vme->file, vme->offset);

    if (file_read(vme->file, kaddr, vme->read_bytes) != (int)vme->read_bytes)
    {
        palloc_free_page(kaddr);
        return false;
    }

    memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
    return true;
}

static unsigned vm_hash_func(const struct hash_elem *e, void *aux)
{
    return ((unsigned)e >> 2) % 4;
}

static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    struct vm_entry *vme_a = hash_entry(a, struct vm_entry, elem);
    struct vm_entry *vme_b = hash_entry(b, struct vm_entry, elem);

    if (vme_a->vaddr < vme_b->vaddr)
        return true;

    return false;
}

static void vm_destroy_func(struct hash_elem *e, void *aux)
{
    struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
    void *kaddr = pagedir_get_page(thread_current()->pagedir, vme->vaddr);
    if (kaddr != NULL)
    {
        struct page *page = find_page(kaddr);
        if (page != NULL)
        {
            lru_list_remove(page);
            free(page);
        }
    }
    free(vme);
}

void mmunmap_file(struct mmap_file *mmap_file)
{
    struct thread *cur = thread_current();

    struct list *vme_list = &mmap_file->vme_list;
    if (!list_empty(vme_list))
    {
        struct list_elem *e = list_begin(vme_list);
        struct vm_entry *vme;
        while (e != list_end(vme_list))
        {
            struct list_elem *next = list_next(e);

            /* Delete vm_entry */
            vme = list_entry(e, struct vm_entry, mmap_elem);
            delete_vme(&cur->vm, vme);

            void *kaddr = pagedir_get_page(cur->pagedir, vme->vaddr);
            if (kaddr != NULL)
            {
                /* Dirty checking */
                bool is_dirty = pagedir_is_dirty(cur->pagedir, vme->vaddr);
                if (is_dirty)
                    file_write_at(vme->file, kaddr, PGSIZE, vme->offset);

                struct page *page = find_page(kaddr);
                if (page != NULL)
                    free_page(page);
            }

            /* Clear page table entry */
            pagedir_clear_page(cur->pagedir, vme->vaddr);
            /* Free vm_entry */
            free(vme);

            e = next;
        }
    }
}
