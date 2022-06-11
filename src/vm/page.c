#include "vm/page.h"
#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static unsigned vm_hash_func(const struct hash_elem *e, void *aux);
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void vm_destroy_func(struct hash_elem *e, void *aux);

void vm_init(struct hash *vm)
{
    hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

void vm_destory(struct hash *vm)
{
    hash_destroy(vm, vm_destroy_func);
}

struct vm_entry *find_vme(void *vaddr)
{
    struct hash vm = thread_current()->vm;
    struct hash_iterator i;

    hash_first(&i, &vm);
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
    struct vm_entry *vme_b = hash_entry(a, struct vm_entry, elem);

    if (vme_a->vaddr <= vme_b->vaddr)
        return true;

    return false;
}

static void vm_destroy_func(struct hash_elem *e, void *aux)
{
    struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
    free(vme);
}
