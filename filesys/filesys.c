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
#include "threads/palloc.h"

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
  if(name[0] != '/') //relative path
    dir = dir_reopen(thread_current()->cwd);

  char ** path = get_path(name);
  if(path == NULL) {
    return false;
  }

  int i = 0;
  while(path[i + 1] != NULL) {
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


  bool success = (dir != NULL && path[i] != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, 0)
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
  if(temp_args == NULL)
  {
    return NULL;
  }
  char* name_copy = malloc(strlen(name) + 1);
  if(name_copy == NULL)
  {
    return NULL;
  }
  memcpy(name_copy, name, strlen(name) + 1);

  char *token, save_ptr;
  int argc = 0;
  for (token = strtok_r (name_copy, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
  {
    temp_args[argc] = token;
    argc++; 
  }
  temp_args[argc] = NULL;
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
  struct dir *dir = dir_open_root ();
  if(name[0] != '/') //relative path
    dir = dir_reopen(thread_current()->cwd);
  struct inode *inode = NULL;

  char ** path = get_path(name);
  if(path == NULL)
    return NULL;
  int i = 0;
  while(path[i] != NULL && path[i + 1] != NULL)
  {
    if(dir != NULL) 
    {
      if(path[i] == '.') 
      {
        dir = dir_reopen(thread_current()->cwd);
      }
      else{
      dir_lookup(dir, path[i], &inode);
      if(inode == NULL)
      {
        break;
      }
      dir_close(dir);
      dir = dir_open(inode);
      }
    }
    else
    {
      break;
    }
    i++;
  }
  if(dir != NULL && path[i] != NULL) {
    dir_lookup(dir, path[i], &inode);
  }
  dir_close(dir);
  free(path);
  struct file *f = file_open (inode);
  return f;
}


bool filesys_chdir(const char *dir) 
{
  struct inode *inode = NULL;
  
  struct dir *directory = dir_open_root();
  if(dir[0] != '/') //relative path
    directory = dir_reopen(thread_current()->cwd);
  char ** path = get_path(dir);
  if(path == NULL)
    return false;
  
  int i = 0;
  while(path[i] != NULL && path[i + 1] != NULL)
  {
    if(directory != NULL) 
    {
      dir_lookup(directory, path[i], &inode);
      if(inode == NULL)
      {
        return false;
      }
      dir_close(directory);
      directory = dir_open(inode);
    }
    else
    {
      return false;
    }
    i++;
  }
  if(directory == NULL)
    return false;
  dir_lookup(directory, path[i], &inode);
  dir_close(directory);
  directory = dir_open(inode);
  if(directory == NULL) {
    return false;
  }
  thread_current()->cwd = dir_reopen(directory);
  dir_close(directory);
  return true;

}

bool mkdir(const char *directory)
{
  block_sector_t inode_sector = 0;
  struct inode *inode = NULL;
  
  struct dir *dir = dir_open_root ();
  if(directory[0] != '/') //relative path
    dir = dir_reopen(thread_current()->cwd);

  char ** path = get_path(directory);
  if(path == NULL) {
    return false;
  }

  int i = 0;
  while(path[i + 1] != NULL) {
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


  bool success = (dir != NULL && path[i] != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, path[i], inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  if(success)
  {
    dir_lookup(dir, path[i], &inode);
    dir_close(dir);
    dir = dir_open(inode);
    inode_deny_write(inode);
  }
  dir_close (dir);
  free(path);
  return success;
}

bool isdir(struct file *file)
{
  return false;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //struct inode *inode;
  struct dir *directory = dir_open_root ();
  // struct dir *temp_dir;
  // if(name[0] != '/') //relative path
  //   directory = dir_reopen(thread_current()->cwd);
  // //printf("cwd: %p\n", directory);
  // char ** path = get_path(name);
  // if(path == NULL)
  //   return false;
  
  
  // int i = 0;
  // // if(path[i] != NULL)
  // // {
  // //   dir_lookup(path[i], )
  // // }
  // while(path[i] != NULL && path[i + 1] != NULL)
  // {
  //   //printf("Path: %s\n", path[i]);
  //   if(directory != NULL) 
  //   {
  //     dir_lookup(directory, path[i], &inode);
  //     if(inode == NULL)
  //     {
  //       return false;
  //     }
  //     dir_close(directory);
  //     directory = dir_open(inode);
  //   }
  //   else
  //   {
  //     return false;
  //   }
  //   i++;
  // }

  // if(directory == NULL)
  // {
  //   return false;
  // }
  // char *temp_name_still = path[i];
  // if(temp_name_still != NULL) 
  // {
  // dir_lookup(directory, temp_name_still, &inode);
  // }
  // temp_dir = dir_open(inode);

  // char *temp_name = palloc_get_page(PAL_USER);

  //bool success = directory != NULL && temp_name_still != NULL && !dir_readdir(temp_dir, temp_name) && dir_remove (directory, temp_name_still);
  //bool success = directory != NULL && path[i] != NULL && dir_remove (directory, path[i]);
  bool success = directory != NULL && dir_remove(directory, name);
  dir_close (directory); 
  // dir_close(temp_dir);
  //free(path);
  // palloc_free_page(temp_name);
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
