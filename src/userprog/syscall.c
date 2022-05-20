#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"

static void syscall_handler(struct intr_frame *);
static void is_valid_addr(uint32_t *vaddr);

static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static void close(int fd);

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
    // hex_dump(esp, esp, 0xc0000000 - esp, true);
    uint32_t esp = f->esp;
    is_valid_addr(esp);

    switch (*(uint32_t *)esp)
    {
    case SYS_HALT: /* Halt the operating system. */
    {
        shutdown_power_off();
        break;
    }
    case SYS_EXIT: /* Terminate this process. */
    {
        is_valid_addr((uint32_t *)(esp + 4));
        int status = *(uint32_t *)(esp + 4);
        thread_exit(status);
        break;
    }
    case SYS_EXEC: /* Switch current process. */
        break;
    case SYS_WAIT: /* Wait for a child process to die. */
        break;
    case SYS_CREATE: /* Create a file. */
    {
        f->eax = create(*(uint32_t *)(esp + 16), *(uint32_t *)(esp + 20));
        break;
    }
    case SYS_REMOVE: /* Delete a file. */
    {
        f->eax = remove(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_OPEN: /* Open a file. */
    {
        f->eax = open(*(uint32_t *)(esp + 4));
        break;
    }
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
    {
        close(*(uint32_t *)(esp + 4));
        break;
    }
    default:
        break;
    }
}

static void is_valid_addr(uint32_t *vaddr)
{
    struct thread *cur = thread_current();
    if (vaddr == NULL || vaddr >= PHYS_BASE || pagedir_get_page(cur->pagedir, vaddr) == NULL)
    {
        thread_exit(-1);
    }
}

static bool create(const char *file, unsigned initial_size)
{
    is_valid_addr(file);
    if (file == NULL)
        thread_exit(-1);

    bool success = filesys_create(file, initial_size);
    return success;
}

static bool remove(const char *file)
{
    is_valid_addr(file);
    if (file == NULL)
        thread_exit(-1);

    bool success = filesys_remove(file);
    return success;
}

static int open(const char *file)
{
    is_valid_addr(file);

    struct thread *cur = thread_current();
    struct file *opend_file = filesys_open(file);
    int fd = (opend_file == NULL) ? -1 : cur->next_fd++;

    if (fd != -1)
        cur->fdt[fd] = opend_file;

    return fd;
}

static void close(int fd)
{
    if (fd != 0 && fd != 1)
    {
        struct thread *cur = thread_current();
        if (fd > 0 && fd < cur->next_fd)
        {
            struct file *file = cur->fdt[fd];
            file_close(file);
            cur->fdt[fd] = NULL;
        }
    }
}