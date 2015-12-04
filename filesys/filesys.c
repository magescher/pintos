#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

#define FILENAME_COPY(name)                  \
  char filename[READDIR_MAX_LEN+1];          \
  memcpy (filename, name, strlen(name) + 1); \

static
struct dir *
get_path (char* name)
{
  int i, len = strlen (name);

  if (len == 0) {
    return NULL;
  }

  struct thread *t = thread_current ();
  struct dir* cwd = dir_reopen (t->cwd);
  char *fn = name;
  if (name[0] == '/') {
    cwd = dir_open_root();
    fn = name+1;
  }
  ASSERT (cwd != NULL);

  struct inode *node = NULL;
  for (i = 1; i < len; i++) {
    if (name[i] == '/') {
      name[i] = '\0';
      if (!dir_lookup (cwd, fn, &node)) {
        /* invalid path, what are you doing?! */
        return NULL;
      }
      fn = (char *)(name+i+1);
      if (inode_isdir (node)) {
        dir_close (cwd);
        cwd = dir_open (node);
      } else {
        /* get outta here! */
        inode_close (node);
        return NULL;
      }
    }
  }

  memcpy(name, fn, strlen(fn)+1);
  return cwd;
}

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  /* Threads are initialized before filesys so we must set up cwd here */
  thread_current()->cwd = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  FILENAME_COPY (name);
  block_sector_t inode_sector = 0;
  struct dir *dir = get_path (filename);

  bool success = false;
  if (is_dir) {
    success = (dir != NULL
               && free_map_allocate (1, &inode_sector)
               && dir_create (inode_sector, 16)
               && dir_add (dir, filename, inode_sector));
  } else {
    success = (dir != NULL
               && free_map_allocate (1, &inode_sector)
               && inode_create (inode_sector, initial_size, false)
               && dir_add (dir, filename, inode_sector));
  }

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  FILENAME_COPY (name);
  struct dir *dir = get_path(filename);
  struct inode *inode = NULL;
  if (dir == NULL) {
    return NULL;
  }

  if (filename[0] == '\0') {
    return (struct file *) dir_open_root ();
  }

  if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  dir_close (dir);

  if (inode_isdir (inode)) {
    return (struct file *) dir_open (inode);
  } else {
    return file_open (inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  FILENAME_COPY (name);
  struct dir *dir = get_path (filename);
  bool success = dir != NULL && dir_remove (dir, filename);
  dir_close (dir); 

  return success;
}

struct dir *
filesys_chdir(const char *name)
{
  FILENAME_COPY (name);

  struct inode *inode;
  struct thread *t = thread_current ();
  struct dir *dir = get_path (filename);
  if (dir_lookup (dir, filename, &inode)) {
    dir_close(t->cwd);
    return dir_open (inode);
  }
  return NULL;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
