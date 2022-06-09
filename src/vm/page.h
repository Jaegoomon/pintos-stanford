#include <hash.h>

struct vm_entry
{
    struct hash_elem elem;
    uint32_t vaddr;
};

void vm_init(struct hash *vm);
void vm_destory(struct hash *vm);
struct vm_entry *find_vme(void *vadrr);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);
void load_file(void *kpage, struct vm_entry *vme);
