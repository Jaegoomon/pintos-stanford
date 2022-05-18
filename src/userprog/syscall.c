#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
    // hex_dump(f->esp, f->esp, 100, 1);
    uint32_t esp = f->esp;

    switch (*(uint32_t *)esp)
    {
    case SYS_HALT: /* Halt the operating system. */
    {
        shutdown_power_off();
        break;
    }
    case SYS_EXIT: /* Terminate this process. */
    {
        thread_exit(0);
        break;
    }
    case SYS_EXEC: /* Switch current process. */
        break;
    case SYS_WAIT: /* Wait for a child process to die. */
        break;
    case SYS_CREATE: /* Create a file. */
        break;
    case SYS_REMOVE: /* Delete a file. */
        break;
    case SYS_OPEN: /* Open a file. */
        break;
    case SYS_FILESIZE: /* Obtain a file's size. */
        break;
    case SYS_READ: /* Read from a file. */
        break;
    case SYS_WRITE: /* Write to a file. */
    {
        if (*(uint32_t *)(esp + 20) == 1)
            putbuf(*(uint32_t *)(esp + 24), *(uint32_t *)(esp + 28));
        break;
    }
    case SYS_SEEK: /* Change position in a file. */
        break;
    case SYS_TELL: /* Report current position in a file. */
        break;
    case SYS_CLOSE: /* Close a file. */
        break;
    default:
        break;
    }
}