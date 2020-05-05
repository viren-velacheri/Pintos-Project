#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

char ** get_path(const char *name);

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
filesys_create (const char *name, off_t initial_size) 
{
  //printf("name: %s\n", name);
  block_sector_t inode_sector = 0;
  struct inode *inode = NULL;
  struct dir *dir = dir_open_root ();
  // if(name[0] != '/') //relative path
  //   dir = thread_current()->cwd;
  char ** path = get_path(name);
  if(path == NULL)
    return false;
  // int j = 0;
  // while(path[j] != NULL) {
  //   //printf("path[i]: %s\n", path[j]);
  //   j++;
  // }
  int i = 0;
  while(path[i + 1] != NULL) {
    //ASSERT(0);
    if(dir != NULL) 
    {
      //printf("path: %s\n", path[i]);
      //printf("path + 1: %s\n", path[i + 1]);
      dir_lookup(dir, path[i], &inode);
      if(inode == NULL)
      {
        break;
      }
      dir_close(dir);
      dir = dir_open(inode);
    }
    else
    {
      break;
    }
    i++;
  }

  bool success = (dir != NULL && path[i] != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, path[i], inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(path);
  return success;
}

char ** get_path(const char *name)
{
  char** temp_args = malloc(strlen(name));
      //ASSERT(0);
  if(temp_args == NULL)
  {
    // printf("This 1\n");
    return NULL;
  }
  char* name_copy = malloc(strlen(name) + 1);
  if(name_copy == NULL)
  {
    // printf("This 2\n");
    return NULL;
  }
        
  memcpy(name_copy, name, strlen(name) + 1);
  char *token, save_ptr;
  int argc = 0;
  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
  {
    temp_args[argc] = token;
    //printf("Names: %s \n", temp_args[argc]);
    argc++; 
  }
  temp_args[argc] = NULL;
  free(name_copy);
  return temp_args;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  printf("Name:  %s\n", name);
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  char ** path = get_path(name);
  
  if(path == NULL)
    return NULL;
  int i = 0;
  //printf("count: %d\n", i);
  while(path[i] != NULL && path[i + 1] != NULL)
  {

    printf("count: %d\n", i);
    //ASSERT(0);
    //struct inode *inode = NULL;
    if(dir != NULL) 
    {
      dir_lookup(dir, path[i], &inode);
      if(inode == NULL)
      {
        break;
      }
      dir_close(dir);
      dir = dir_open(inode);
    }
    else
    {
      break;
    }
    i++;
  }
  if(dir != NULL && path[i] != NULL)
    dir_lookup(dir, path[i], &inode);
  dir_close(dir);
  // if (dir != NULL)
  //   dir_lookup (dir, name, &inode);
  // dir_close (dir);
  free(path);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
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
