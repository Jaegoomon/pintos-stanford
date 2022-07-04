#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"

static void syscall_handler(struct intr_frame *);
static void is_valid_addr(uint32_t *vaddr);

static void halt(void);
static pid_t exec(const char *cmd_line);
static int wait(pid_t pid);
static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned size);
static int write(int fd, const void *buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);
static int mmap(int fd, void *addr);

void syscall_init(void)
{
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
    // hex_dump(esp, esp, 0xc0000000 - esp, true);
    uint32_t esp = f->esp;
    struct thread *cur = thread_current();
    if (esp == NULL || esp >= PHYS_BASE || pagedir_get_page(cur->pagedir, esp) == NULL)
        exit(-1);

    switch (*(uint32_t *)esp)
    {
    case SYS_HALT: /* Halt the operating system. */
    {
        halt();
        break;
    }
    case SYS_EXIT: /* Terminate this process. */
    {
        is_valid_addr((uint32_t *)(esp + 4));
        exit(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_EXEC: /* Switch current process. */
    {
        f->eax = exec(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_WAIT: /* Wait for a child process to die. */
    {
        f->eax = wait(*(uint32_t *)(esp + 4));
        break;
    }
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
    {
        f->eax = filesize(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_READ: /* Read from a file. */
    {
        f->eax = read(*(uint32_t *)(esp + 20), *(uint32_t *)(esp + 24), *(uint32_t *)(esp + 28));
        break;
    }
    case SYS_WRITE: /* Write to a file. */
    {
        f->eax = write(*(uint32_t *)(esp + 20), *(uint32_t *)(esp + 24), *(uint32_t *)(esp + 28));
        break;
    }
    case SYS_SEEK: /* Change position in a file. */
    {
        seek(*(uint32_t *)(esp + 16), *(uint32_t *)(esp + 20));
        break;
    }
    case SYS_TELL: /* Report current position in a file. */
    {
        f->eax = tell(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_CLOSE: /* Close a file. */
    {
        close(*(uint32_t *)(esp + 4));
        break;
    }
    case SYS_MMAP: /* Map a file into memory. */
    {
        f->eax = mmap(*(uint32_t *)(esp + 16), *(uint32_t *)(esp + 20));
        break;
    }
    case SYS_MUNMAP: /* Remove a memory mapping. */
    {
        munmap(*(uint32_t *)(esp + 4));
        break;
    }
    default:
        break;
    }

    unpin_page();
}

static void is_valid_addr(uint32_t *vaddr)
{
    struct thread *cur = thread_current();
    if (vaddr == NULL || vaddr >= PHYS_BASE)
        exit(-1);
}

static void halt()
{
    shutdown_power_off();
}

void exit(int status)
{
    struct thread *cur = thread_current();
    if (cur->parent != NULL)
        sema_down(&cur->exit_sema);

    printf("%s: exit(%d)\n", cur->name, status);

    thread_exit(status);
}

static pid_t exec(const char *cmd_line)
{
    is_valid_addr(cmd_line);

    char *file_name;
    struct thread *cur = thread_current();

    file_name = palloc_get_page(0);
    strlcpy(file_name, cmd_line, PGSIZE);

    pid_t pid = process_execute(file_name);
    struct thread *child = find_child(pid);

    palloc_free_page(file_name);
    sema_down(&child->exec_sema);

    if (child->load_status == 0)
        return -1;

    return pid;
}

static int wait(pid_t pid)
{
    return process_wait(pid);
}

static bool create(const char *file, unsigned initial_size)
{
    is_valid_addr(file);
    if (file == NULL)
        exit(-1);

    bool success = filesys_create(file, initial_size);
    return success;
}

static bool remove(const char *file)
{
    is_valid_addr(file);
    if (file == NULL)
        exit(-1);

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

static int filesize(int fd)
{
    struct thread *cur = thread_current();
    struct file *file = cur->fdt[fd];
    return file_length(file);
}

static int read(int fd, void *buffer, unsigned size)
{
    lock_acquire(&filesys_lock);
    unpin_page();

    int size_read = -1;
    check_valid_buffer(buffer, size);

    if (fd == 0)
        size_read = input_getc();
    else
    {
        struct thread *cur = thread_current();
        if (fd > 1 && fd < cur->next_fd)
        {
            struct file *file = cur->fdt[fd];
            size_read = file_read(file, buffer, size);
        }
    }

    unpin_page();
    lock_release(&filesys_lock);

    return size_read;
}

static int write(int fd, const void *buffer, unsigned size)
{
    lock_acquire(&filesys_lock);
    unpin_page();

    int size_written = -1;
    check_valid_buffer(buffer, size);

    if (fd == 1)
        putbuf(buffer, size);
    else
    {
        struct thread *cur = thread_current();
        if (fd > 0 && fd < cur->next_fd)
        {
            struct file *file = cur->fdt[fd];
            size_written = file_write(file, buffer, size);
        }
    }

    unpin_page();
    lock_release(&filesys_lock);

    return size_written;
}

static void seek(int fd, unsigned position)
{
    struct thread *cur = thread_current();
    struct file *file = cur->fdt[fd];

    file_seek(file, position);
}

static unsigned tell(int fd)
{
    struct thread *cur = thread_current();
    struct file *file = cur->fdt[fd];

    return file_tell(file);
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

static int mmap(int fd, void *addr)
{
    /* Null checking */
    if (addr == NULL)
        return -1;

    is_valid_addr(addr);
    if (fd < 0 || fd > 128)
        return -1;

    /* Disallow overlap */
    if (find_vme(addr) != NULL)
        return -1;

    /* Check alignment */
    if (pg_ofs(addr) != 0)
        return -1;

    struct thread *cur = thread_current();
    struct file *file = cur->fdt[fd];
    struct file *reopened_file = file_reopen(file);
    int mapid = cur->next_fd++;
    cur->fdt[mapid] = reopened_file;

    struct mmap_file *mmap_file = malloc(sizeof(struct mmap_file));
    mmap_file->mapid = mapid;
    mmap_file->file = reopened_file;
    list_push_front(&cur->mmap_list, &mmap_file->elem);
    list_init(&mmap_file->vme_list);

    uint32_t read_bytes = file_length(reopened_file);
    while (read_bytes > 0)
    {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct vm_entry *vme = malloc(sizeof(struct vm_entry));
        list_push_back(&mmap_file->vme_list, &vme->mmap_elem);
        vme->file = reopened_file;
        vme->vaddr = addr;
        vme->read_bytes = page_read_bytes;
        vme->zero_bytes = page_zero_bytes;
        vme->offset = reopened_file->pos;
        vme->writable = true;
        vme->type = VM_FILE;

        file_seek(reopened_file, file_tell(reopened_file) + page_read_bytes);
        bool success = insert_vme(&cur->vm, vme);

        read_bytes -= page_read_bytes;
        addr += PGSIZE;
    }

    return mmap_file->mapid;
}

void munmap(int mapid)
{
    struct list *mmap_list = &thread_current()->mmap_list;

    if (!list_empty(mmap_list))
    {
        struct mmap_file *mf;
        struct list_elem *e = list_begin(mmap_list);
        while (e != list_end(mmap_list))
        {
            struct list_elem *next = list_next(e);

            mf = list_entry(e, struct mmap_file, elem);
            if (mf->mapid == mapid || mapid == INT32_MAX)
            {
                mmunmap_file(mf);
                list_remove(e);
                free(mf);
                close(mf->mapid);
            }

            e = next;
        }
    }
}
