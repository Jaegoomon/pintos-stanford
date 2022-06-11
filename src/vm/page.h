#include <hash.h>
#include "filesys/file.h"

struct vm_entry
{
    uint8_t *vaddr;
    size_t read_bytes;
    size_t zero_bytes;
    unsigned offset;
    struct file *file;
    bool writable;

    struct hash_elem elem;
};

void vm_init(struct hash *vm);
void vm_destory(struct hash *vm);
struct vm_entry *find_vme(void *vaddr);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
bool load_file(void *kpage, struct vm_entry *vme);
