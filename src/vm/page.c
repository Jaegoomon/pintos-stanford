#include <hash.h>
#include "vm/page.h"

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

struct vm_entry *find_vme(void *vadrr)
{
}

bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
}

bool delete_vme(struct hash *vm, struct vm_entry *vme)
{
}

void load_file(void *kpage, struct vm_entry *vme)
{
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
