#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(int);
void process_activate(void);
struct thread *find_child(tid_t child_tid);

#endif /* userprog/process.h */
