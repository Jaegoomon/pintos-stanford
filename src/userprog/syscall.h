#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

typedef int pid_t;

struct lock filesys_lock;

void syscall_init(void);
void exit(int status);

#endif /* userprog/syscall.h */
