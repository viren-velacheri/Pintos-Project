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

#define ERROR -1 /* Indicates process exited or didn't load due to an error */
static void syscall_handler (struct intr_frame *);

//Viren driving now

/* This method is used for determining whether 
the given address/pointer is valid or not. 
Returns 0 if not and 1 if it is. */
int valid_pointer(void * ptr) {
  // If the pointer is null or it is a kernel and not user address 
  // or if user address is unmapped, 0 (false)
  // is returned. Otherwise, 1 (true) is returned.
  if(ptr == NULL || is_kernel_vaddr (ptr) || 
  thread_current()->pagedir == NULL || 
  pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
    {
      return 0;
    }
    return 1;
}

//Viren done driving

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  // This is where we initialize our locks.
  lock_init(&file_lock);
  lock_init(&write_lock);
  lock_init(&status_lock);
}

//Jasper driving now

/* This is our exit helper method 
that is used for when a thread 
exits and to retrieve its status
before doing so. Status lock is used
to ensure that this is done atomically. */
void exit(int status) {
  //set exit status atomically
  lock_acquire(&status_lock);
  thread_current()->exit_status = status;
  lock_release(&status_lock);
  
  //close this thread's executable (allows writes)
  lock_acquire(&file_lock);
  file_close(thread_current()->executable);
  lock_release(&file_lock);

  thread_exit();
}

//Jasper done driving

/* This method is used to handle all the system calls and do
what is required respectively. */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //Jordan driving now

  // A temporary stack pointer so actual
  // stack pointer isn't modified directly.
  char *temp_esp = f->esp;
  if(!valid_pointer(temp_esp))
  {
    exit(ERROR);
  }

  switch(*(int *)temp_esp) {

    case SYS_HALT:
      shutdown_power_off();
      break;

    //Jordan done driving
    //Brock driving now
    
    case SYS_EXIT:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      
      int status = *(int *) temp_esp;
      exit(status);
      break;
    
    case SYS_EXEC:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      char * cmd_line = *(int *) temp_esp; 
      if(!valid_pointer(cmd_line))
        exit(ERROR);
      
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
    
    case SYS_WAIT:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      tid_t pid = *(int *) temp_esp;
      
      f->eax = process_wait(pid);
      break;
    
    case SYS_CREATE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      char * file = *(int *) temp_esp;
      if(!valid_pointer(file))
        exit(ERROR);
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      uint32_t initial_size = *(int *) temp_esp;
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(&file_lock);
      break;

    //Viren done driving
    //Jasper driving now
    
    case SYS_REMOVE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      char * file2 = *(int *) temp_esp;
      if(!valid_pointer(file2))
        exit(ERROR);
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      f->eax = filesys_remove(file2);
      lock_release(&file_lock);
      break;
    
    case SYS_OPEN:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      char * file3 = *(int *)temp_esp;
      if(!valid_pointer(file3))
        exit(ERROR);
      
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      struct file *open_file = filesys_open(file3);
      lock_release(&file_lock);
      
      if(open_file == NULL) {
        f->eax = ERROR;
      } else {
        struct thread *t = thread_current();
        //add this file to thread's array of open files
        t->set_of_files[t->curr_file_index] = open_file;
        f->eax = t->curr_file_index; //return fd
        t->curr_file_index++; //increment fd
      }
      break;

    //Jasper done driving
    //Jordan driving now
    
    case SYS_FILESIZE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd = *(int *) temp_esp;
      struct thread *t_size = thread_current();
      //exit with error if paramater fd is not a valid file
      if(fd < 2 || fd > t_size->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index = t_size->set_of_files[fd];
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      f->eax = file_length(file_at_index);
      lock_release(&file_lock);
      break;
    
    case SYS_READ:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd_read = *(int *) temp_esp;
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      void *buffer = *(int *) temp_esp;
      if(!valid_pointer(buffer))
        exit(ERROR);
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      unsigned size = *(int *)temp_esp;
      
      struct thread *t_read = thread_current();
      //case where fd is 0 and we are reading from user
      if(fd_read == 0) {
        f->eax = input_getc();
        break;
      }
      //exit with error if paramater fd is not a valid file
      if(fd_read < 2 || fd_read > t_read->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index2 = t_read->set_of_files[fd_read];
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      f->eax = file_read(file_at_index2, buffer, size);
      lock_release(&file_lock);
      break;

    //Jordan done driving
    //Brock driving now
    
    case SYS_WRITE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd_write = *(int *) temp_esp;
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      const void *buffer_write = *(int *) temp_esp;
      if(!valid_pointer(buffer_write))
        exit(ERROR);
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      unsigned size_write = *(int *)temp_esp;

      struct thread *t_write = thread_current();
      if(fd_write < 0 || fd_write > t_write->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index3 = t_write->set_of_files[fd_write];
      
      //when fd is 1 write to the console using putbuf
      if(fd_write == 1) {
        lock_acquire(&write_lock);
        putbuf((char *)buffer_write, size_write);
        f->eax = size_write;
        lock_release(&write_lock);
      }
      //other cases when writing to a file
      else if(fd_write > 1) {
        //use synchronization with the file lock when
        //accessing the file system
        lock_acquire(&file_lock);
        f->eax = file_write(file_at_index3, buffer_write, size_write);
        lock_release(&file_lock);
      }
      break;
    
    case SYS_SEEK:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd_seek = *(int *) temp_esp;
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      unsigned position = *(int *)temp_esp;
      
      struct thread *t_seek = thread_current();
      if(fd_seek < 2 || fd_seek > t_seek->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index4 = t_seek->set_of_files[fd_seek];
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      file_seek(file_at_index4, position);
      lock_release(&file_lock);
      break;

    //Brock done driving
    //Viren driving now
    
    case SYS_TELL:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd_tell = *(int *) temp_esp;
      
      struct thread *t_tell = thread_current();
      if(fd_tell < 2 || fd_tell > t_tell->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index5 = t_tell->set_of_files[fd_tell];
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      f->eax = file_tell(file_at_index5);
      lock_release(&file_lock);
      break;

    case SYS_CLOSE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd_close = *(int *) temp_esp;
      
      struct thread *t_close = thread_current();
      if(fd_close < 2 || fd_close > t_close->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index6 = t_close->set_of_files[fd_close];
      //null out file to be closed
      t_close->set_of_files[fd_close] = NULL;
      if(file_at_index6 == NULL)
        exit(ERROR);
      //use synchronization with the file lock when
      //accessing the file system
      lock_acquire(&file_lock);
      file_close(file_at_index6);
      lock_release(&file_lock);
      break;
  }

  //Viren done driving
}
