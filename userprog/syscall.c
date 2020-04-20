#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"

#define ERROR -1  /* Used when a pointer or file is invalid */
#define STDIN 0   /* Standard Input File Descriptor */
#define STDOUT 1  /* Standard Output File Descriptor */
#define STDERR 2  /* Standard Error File Descriptor */

static void syscall_handler (struct intr_frame *);

//Viren driving now

/* This method is used for determining whether 
the given address/pointer is valid or not. 
Exits with -1 status if not valid. */
void valid_pointer_check(void * ptr) 
{
  // If the pointer is null or it is a kernel and not user address 
  // or if address is unmapped, exit with -1 error status.
  // Otherwise, nothing happens.
  if(ptr == NULL || is_kernel_vaddr (ptr)) //|| pagedir_get_page(thread_current()->pagedir, ptr) == NULL
    {
      exit(-1);
    }
}

/* This is a helper method used for checking if given fd is between 
curr_file_index and the least possible fd value it could be (0 or 2).
if it isn't, exit with -1 status. Parameters are t or the current 
thread, the given fd, and the low_limit (either 0 or 2). */
void fd_exist_check(struct thread *t, int fd, int low_limit)
{
  // A simple check to see if file exists or not based on given fd.
  if(fd < low_limit || fd >= t->curr_file_index) 
  {
    exit(-1);
  }
}

/* Used to check if file exists or not. Used in case where 
fd is valid but file was closed earlier. Only parameter is the respective
file to check. If it is NULL, exit with -1 error status. */ // Yeah I think i have the same settings but not sure. Oh whoa We are failing less
void file_exist_check(struct file *f) 
{
  if(f == NULL)
  {
    exit(-1);
  }
}

//Viren done driving

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  // This is where we initialize our locks.
  lock_init(&file_lock);
  lock_init(&write_lock);
}

//Jasper driving now

/* This is our exit helper method 
that is used for when a thread 
exits and to retrieve its status
before doing so. Status lock is used
to ensure that this is done atomically. */
void exit(int status) 
{
  thread_current()->exit_status = status;
  
  //close this thread's executable (allows writes)
  if(!lock_held_by_current_thread(&file_lock))
  {
    lock_acquire(&file_lock);
  }
  file_close(thread_current()->executable);
  lock_release_check(&file_lock);

  thread_exit();
}

//Jasper done driving

/* This method is used to handle all the system calls and do
what is required respectively. */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //Jordan driving now
  thread_current()->esp_copy = f->esp;
  // A temporary stack pointer so actual
  // stack pointer isn't modified directly.
  char *temp_esp = f->esp;
  valid_pointer_check(temp_esp);

  int sys_call_num = *(int *)temp_esp;
  switch(sys_call_num) 
  {

    // System call where Pintos is terminated.
    case SYS_HALT:
      shutdown_power_off();
      break;

    //Jordan done driving
    //Brock driving now
    // System call that terminates current user program, returning 
    // its exit status to the kernel. Usually 0 if successful
    // and -1 or some other nonzero value if not.
    case SYS_EXIT:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int status = *(int *) temp_esp;
      exit(status);
      break;
    
    // System call that runs the executable based off name
    // given in the cmd_line argument. Process's pid is 
    // returned or -1 if can't load or run.
    case SYS_EXEC:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      char * cmd_line = *(int *) temp_esp; 
      valid_pointer_check(cmd_line);
      tid_t child_tid = process_execute(cmd_line);
      //block the child's exec semaphore until after 
      //the process has been loaded
      sema_down(&get_thread_from_tid(child_tid)->exec_sema);
      //return -1 if child didn't load properly
      if(!get_thread_from_tid(child_tid)->childLoaded) 
        child_tid = ERROR;
      f->eax = child_tid;
      break;

    //Brock done driving
    //Viren driving now
    // System call where process waits for a child process pid 
    // and retrieve's its exit status. -1 returned if process_wait
    // fails.
    case SYS_WAIT:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      tid_t pid = *(int *) temp_esp;
      f->eax = process_wait(pid);
      break;
    
    // System call where a new file is created with a certain 
    // initial size. True if successful, false if not. 
    // Since creating a file does not open it, we don't store
    // it in our array of files opened.
    case SYS_CREATE:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      char * file = *(int *) temp_esp;
      valid_pointer_check(file);
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      uint32_t initial_size = *(int *) temp_esp;
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release_check(&file_lock);
      break;

    //Viren done driving
    //Jasper driving now
    // System call that deletes the given file. True if successful,
    // false if not. Since a file can be removed regardless of whether
    // it is open or not, we don't modify our array of files opened.
    case SYS_REMOVE:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      char * file_to_remove = *(int *) temp_esp;
      valid_pointer_check(file_to_remove);
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      f->eax = filesys_remove(file_to_remove);
      lock_release_check(&file_lock);
      break;
    
    // System call opens a file. The file descriptor or index in the array
    // of open files that it was added in is returned or -1 if file
    // could not be opened or was NULL.
    case SYS_OPEN:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      char * file_to_open = *(int *)temp_esp;
      valid_pointer_check(file_to_open);
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      struct file *open_file = filesys_open(file_to_open);
      lock_release_check(&file_lock);
      
      if(open_file == NULL) 
      {
        f->eax = ERROR;
      } 
      else 
      {
        struct thread *t = thread_current();
        //add this file to thread's array of open files
        t->set_of_files[t->curr_file_index] = open_file;
        f->eax = t->curr_file_index; //return fd
        t->curr_file_index++; //increment fd
      }
      break;

    //Jasper done driving
    //Jordan driving now
    // System call that returns the size of the open file. Do a check
    // to see if file would exist at given fd. If not, exit with error
    // status. Otherwise, get file at spot file descriptor indexes into
    // and get the size.
    case SYS_FILESIZE:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd = *(int *) temp_esp;
      struct thread *t_size = thread_current();
      //exit with error if paramater fd is not a valid file
      fd_exist_check(t_size, fd, STDERR);
      struct file *file_at_fd = t_size->set_of_files[fd];
      // Additional check done in case a file with valid fd
      // was closed earlier, so exit with -1 error status
      // when such a thing happens.
      file_exist_check(file_at_fd);
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      f->eax = file_length(file_at_fd);
      lock_release_check(&file_lock);
      break;
    
    // System call that reads a certain number of bytes of open file
    // into buffer. Returns number of bytes read or -1 if file could not
    // be read besides it being at the end of file.
    case SYS_READ:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd_read = *(int *) temp_esp;
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      void *buffer = *(int *) temp_esp;
      valid_pointer_check(buffer);
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      unsigned size = *(int *)temp_esp;
      
      struct thread *t_read = thread_current();
      //case where fd is 0 and we are reading from user
      if(fd_read == STDIN) 
      {
        f->eax = input_getc();
        break;
      }
      //exit with error if paramater fd is not a valid file
      fd_exist_check(t_read, fd_read, STDERR);
      struct file *file_to_read = t_read->set_of_files[fd_read];
      // Additional check done in case a file with valid fd
      // was closed earlier, so exit with -1 error status
      // when such a thing happens.
      file_exist_check(file_to_read);
      //use synchronization with the file lock when
      //accessing the file system
      if(!lock_held_by_current_thread(&file_lock))
      {
      lock_acquire(&file_lock);
      }
      f->eax = file_read(file_to_read, buffer, size);
      if(lock_held_by_current_thread(&file_lock))
      {
      lock_release(&file_lock);
      }
      break;

    //Jordan done driving
    //Brock driving now
    // System call that writes a certain amount of bytes from buffer 
    // to the open file designated by file descriptor. Returns number
    // of bytes written or 0 if none could be written.
    case SYS_WRITE:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd_write = *(int *) temp_esp;
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      const void *buffer_write = *(int *) temp_esp;
      valid_pointer_check(buffer_write);
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      unsigned size_write = *(int *)temp_esp;

      struct thread *t_write = thread_current();
      fd_exist_check(t_write, fd_write, STDIN);
      struct file *file_to_write = t_write->set_of_files[fd_write];
      
      //when fd is 1 write to the console using putbuf
      if(fd_write == STDOUT) 
      {
        lock_acquire_check(&write_lock);
        putbuf((char *)buffer_write, size_write);
        f->eax = size_write;
        lock_release_check(&write_lock);
      }
      //other cases when writing to a file
      else if(fd_write > STDOUT) 
      {
        // Additional check done in case a file with valid fd
        // was closed earlier, so exit with -1 error status
        // when such a thing happens.
        file_exist_check(file_to_write);
        //use synchronization with the file lock when
        //accessing the file system
        lock_acquire_check(&file_lock);
        f->eax = file_write(file_to_write, buffer_write, size_write);
        lock_release_check(&file_lock);
      }
      break;
    
    // System call that changes the next byte to be read or written 
    // in open file designated by given file descriptor, relative to 
    // bytes from beginning of file.
    case SYS_SEEK:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd_seek = *(int *) temp_esp;
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      unsigned position = *(int *)temp_esp;
      
      struct thread *t_seek = thread_current();
      fd_exist_check(t_seek, fd_seek, STDERR);
      struct file *file_to_seek = t_seek->set_of_files[fd_seek];
      // Additional check done in case a file with valid fd
      // was closed earlier, so exit with -1 error status
      // when such a thing happens.
      file_exist_check(file_to_seek);
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      file_seek(file_to_seek, position);
      lock_release_check(&file_lock);
      break;

    //Brock done driving
    //Viren driving now
    // System call returns position of next byte to be read or written
    // in open file designated by given file descriptor, relative to bytes
    // from beginning of file.
    case SYS_TELL:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd_tell = *(int *) temp_esp;
      
      struct thread *t_tell = thread_current();
      fd_exist_check(t_tell, fd_tell, STDERR);
      struct file *file_to_tell = t_tell->set_of_files[fd_tell];
      // Additional check done in case a file with valid fd
      // was closed earlier, so exit with -1 error status
      // when such a thing happens.
      file_exist_check(file_to_tell);
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      f->eax = file_tell(file_to_tell);
      lock_release_check(&file_lock);
      break;

    // System call closes the open file with given file descriptor.
    case SYS_CLOSE:
      temp_esp += sizeof(int);
      valid_pointer_check(temp_esp);
      int fd_close = *(int *) temp_esp;
      
      struct thread *t_close = thread_current();
      fd_exist_check(t_close, fd_close, STDERR);
      struct file *file_to_close = t_close->set_of_files[fd_close];
      //null out file to be closed
      t_close->set_of_files[fd_close] = NULL;
      // Adjusts the current file descriptor index in the case that 
      // the latest open file (curr_file_index - 1) was the one closed.
      // If so, loop through and go down through a potential row of 
      // consecutive closed files until standard error or 2.
      if(fd_close == t_close->curr_file_index - 1) 
      {
        while(t_close->curr_file_index > STDERR && 
        t_close->set_of_files[t_close->curr_file_index - 1] == NULL)
        {
          t_close->curr_file_index--;
        }
      }
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire_check(&file_lock);
      file_close(file_to_close);
      lock_release_check(&file_lock);
      break;
  }

  //Viren done driving
}
