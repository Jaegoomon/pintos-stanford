#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init(bool format)
{
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
        PANIC("No file system device found, can't initialize file system.");

    inode_init();
    free_map_init();

    /* Allocate and initialize buffer cache. */
    bc_init();

    if (format)
        do_format();

    free_map_open();

    /* Set thread directory as root. */
    thread_current()->cur_dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done(void)
{
    free_map_close();

    /* Destroy buffer cache */
    bc_free();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size)
{
    block_sector_t inode_sector = 0;

    char *path_name = malloc(strlen(name) + 1);
    char *file_name = malloc(NAME_MAX + 1);

    strlcpy(path_name, name, strlen(name) + 1);
    struct dir *dir = parse_path(path_name, file_name);

    bool success = (dir != NULL && free_map_allocate(1, &inode_sector) && inode_create(inode_sector, initial_size, FILE) && dir_add(dir, file_name, inode_sector));
    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);
    dir_close(dir);

    free(path_name);
    free(file_name);

    return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open(const char *name)
{
    char *path_name = malloc(strlen(name) + 1);
    char *file_name = malloc(NAME_MAX + 1);

    strlcpy(path_name, name, strlen(name) + 1);
    struct dir *dir = parse_path(path_name, file_name);

    struct inode *inode = NULL;

    if (dir != NULL)
    {
        if (strlen(file_name) != 0)
            dir_lookup(dir, file_name, &inode);
        else if (is_root_dir(dir))
            inode = inode_open(ROOT_DIR_SECTOR);
    }
    dir_close(dir);

    free(path_name);
    free(file_name);

    return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove(const char *name)
{
    char *path_name = malloc(strlen(name) + 1);
    char *file_name = malloc(NAME_MAX + 1);
    char *child_name = malloc(NAME_MAX + 1);

    strlcpy(path_name, name, strlen(name) + 1);
    struct dir *dir = parse_path(path_name, file_name);

    struct inode *inode = NULL;
    bool success = false;
    bool has_child = false;

    /* Check if target is directory. */
    if (dir_lookup(dir, file_name, &inode))
    {
        if (inode_is_dir(inode))
        {
            /* Directory must not have any children. */
            struct dir *cur = dir_open(inode);
            has_child = dir_has_child(cur, child_name);
            dir_close(cur);

            if (has_child)
                goto done;
        }
        else
            inode_close(inode);
    }

    success = dir != NULL && dir_remove(dir, file_name);
done:
    dir_close(dir);

    free(path_name);
    free(file_name);
    free(child_name);

    return success;
}

bool filesys_create_dir(const char *name)
{
    struct inode *inode;

    char *path_name = malloc(strlen(name) + 1);
    char *file_name = malloc(NAME_MAX + 1);

    strlcpy(path_name, name, strlen(name) + 1);
    struct dir *dir = parse_path(path_name, file_name);

    block_sector_t inode_sector = 0;
    bool success = dir != NULL && free_map_allocate(1, &inode_sector) && dir_create(inode_sector, 1) && dir_add(dir, file_name, inode_sector);
    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);

    if (success)
    {
        struct dir *dir_ = dir_open(inode_open(inode_sector));
        char cur_dir_name[2] = ".";
        char prev_dir_name[3] = "..";

        dir_add(dir_, cur_dir_name, inode_sector);
        dir_add(dir_, prev_dir_name, dir->inode->sector);

        struct inode *test = dir_->inode;
        dir_close(dir_);
    }

    dir_close(dir);
    free(path_name);
    free(file_name);

    return success;
}

/* Formats the file system. */
static void
do_format(void)
{
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16))
        PANIC("root directory creation failed");

    /* Add . and .. directory in root. */
    struct dir *root = dir_open_root();

    char cur_dir_name[2] = ".";
    char prev_dir_name[3] = "..";

    dir_add(root, cur_dir_name, root->inode->sector);
    dir_add(root, prev_dir_name, root->inode->sector);
    dir_close(root);

    free_map_close();
    printf("done.\n");
}

struct dir *parse_path(char *path_name, char *file_name)
{
    struct dir *dir = NULL;
    bool success = true;

    if (path_name == NULL || file_name == NULL)
        return NULL;
    if (strlen(path_name) == 0)
        return NULL;

    bool is_abs_path = path_name[0] == '/';

    char *token, *next_token, *save_ptr;
    token = strtok_r(path_name, "/", &save_ptr);
    next_token = strtok_r(NULL, "/", &save_ptr);

    if (!is_abs_path)
    {
        struct thread *cur = thread_current();
        if (cur->cur_dir != NULL)
        {
            if (cur->cur_dir->inode->removed)
            {
                dir_close(cur->cur_dir);
                cur->cur_dir = NULL;
            }
            else
                dir = dir_reopen(cur->cur_dir);
        }
    }
    else
        dir = dir_open_root();

    while (token != NULL && next_token != NULL)
    {
        struct inode *inode;
        if (dir_lookup(dir, token, &inode))
        {
            dir_close(dir);

            struct inode_disk *disk_inode = malloc(BLOCK_SECTOR_SIZE);
            get_disk_inode(inode, disk_inode);
            if (disk_inode->is_dir)
                dir = dir_open(inode);
            else
            {
                success = false;
                goto done;
            }
            free(disk_inode);
        }

        token = next_token;
        next_token = strtok_r(NULL, "/", &save_ptr);
    }

done:
    if (!success)
        return NULL;

    if (token != NULL && strlen(token) <= NAME_MAX)
        strlcpy(file_name, token, strlen(token) + 1);
    else
        file_name[0] = '\0';

    return dir;
}
