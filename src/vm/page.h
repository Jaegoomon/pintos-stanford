#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum vm_type
{
    VM_BIN,
    VM_FILE,
    VM_ANON
};

struct page
{
    uint8_t *kaddr;
    struct vm_entry *vme;
    struct thread *thread;
    struct list_elem lru;
};

struct vm_entry
{
    uint8_t *vaddr;
    size_t read_bytes;
    size_t zero_bytes;
    unsigned offset;
    struct file *file;
    bool writable;
    enum vm_type type;
    size_t sec_idx;

    struct hash_elem elem;
    struct list_elem mmap_elem;
};

struct mmap_file
{
    int mapid;
    struct file *file;
    struct list_elem elem;
    struct list vme_list;
};

void vm_init(struct hash *vm);
void vm_destory(struct hash *vm);
struct vm_entry *find_vme(void *vaddr);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
bool load_file(void *kpage, struct vm_entry *vme);
void mmunmap_file(struct mmap_file *mmap_file);

#endif