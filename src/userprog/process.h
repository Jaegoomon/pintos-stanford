#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"

#define MAX_STACK_SIZE (1 << 23)

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);
struct thread *find_child(tid_t child_tid);
bool handle_mm_fault(struct vm_entry *vme);
bool expand_stack(void *addr);

#endif /* userprog/process.h */
