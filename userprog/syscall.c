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

/* This method is used for determining whether 
the given address/pointer is valid or not. 
Returns 0 if not and 1 if it is. */
int valid_pointer(void * ptr) {
  // If the pointer is null or it is a kernel 
  // and not user address or the pagedir is null 
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

int* get_argument(void *esp){
  (int *)esp++;
  return (int *)esp;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  // This is where we initialize our locks.
  lock_init(&file_lock);
  lock_init(&write_lock);
  lock_init(&status_lock);
}

/* This is our exit helper method 
that is used for when a thread 
exits and to retrieve its status
before doing so. Status lock is used
to ensure that this is done atomically. */
void exit(int status) {
  lock_acquire(&status_lock);
  thread_current()->exit_status = status;
  lock_release(&status_lock);
  thread_exit();
}

/* This method is used to handle all the system calls and do
what is required respectively. */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
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
      sema_down(&get_thread_from_tid(child_tid)->exec_sema);
      if(!get_thread_from_tid(child_tid)->childLoaded) 
        child_tid = ERROR;
      f->eax = child_tid;
      break;
    
    case SYS_WAIT:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      tid_t pid = *(int *) temp_esp; //(get_argument(f->esp));
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
      lock_acquire(&file_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(&file_lock);
      break;
    
    case SYS_REMOVE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      char * file2 = *(int *) temp_esp;
      if(!valid_pointer(file2))
        exit(ERROR);
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
      lock_acquire(&file_lock);
      struct file *open_file = filesys_open(file3);
      lock_release(&file_lock);
      if(open_file == NULL) {
        f->eax = ERROR;
      } else {
        struct thread *t = thread_current();
        t->set_of_files[t->curr_file_index] = open_file;
        f->eax = t->curr_file_index;
        t->curr_file_index++;
      }
      break;
    
    case SYS_FILESIZE:
      temp_esp += sizeof(int);
      if(!valid_pointer(temp_esp))
        exit(ERROR);
      int fd = *(int *) temp_esp;
      struct thread *t_size = thread_current();
      if(fd < 2 || fd > t_size->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index = t_size->set_of_files[fd];
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
      if(fd_read == 0) {
        f->eax = input_getc();
        break;
      }
      if(fd_read < 2 || fd_read > t_read->curr_file_index) {
        exit(ERROR);
      }
      struct file *file_at_index2 = t_read->set_of_files[fd_read];
      lock_acquire(&file_lock);
      f->eax = file_read(file_at_index2, buffer, size);
      lock_release(&file_lock);
      break;
    
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
     
      if(fd_write == 1) {
        lock_acquire(&write_lock);
        putbuf((char *)buffer_write, size_write);
        f->eax = size_write;
        lock_release(&write_lock);
      }
         
      else if(fd_write > 1) {
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
      lock_acquire(&file_lock);
      file_seek(file_at_index4, position);
      lock_release(&file_lock);
      break;
    
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
      t_close->set_of_files[fd_close] = NULL;
      if(file_at_index6 == NULL)
        exit(ERROR);
      lock_acquire(&file_lock);
      file_close(file_at_index6);
      lock_release(&file_lock);
      break;
  }
  //thread_exit ();

  /*
  int sys_num = *(int *)f->esp;
  if(sys_num == SYS_HALT) 
  {
    shutdown_power_off();
  }
  else if(sys_num == SYS_EXIT)
  {
    *(int *)f->esp++;
    int status = *(int *) f->esp;
    thread_current()->exit_status = status;
    sema_up(thread_current()->parent->proc_wait);
    process_exit();
  }
  else if(sys_num == SYS_EXEC)
  {

  }
  else if(sys_num == SYS_WAIT)
  {

  }
  else if(sys_num == SYS_CREATE)
  {

  }
  else if(sys_num == SYS_REMOVE)
  {

  }
  else if(sys_num == SYS_OPEN)
  {

  }
  else if(sys_num == SYS_FILESIZE)
  {

  }
  else if(sys_num == SYS_READ)
  {

  }
  else if(sys_num == SYS_WRITE)
  {

  }
  else if(sys_num == SYS_SEEK)
  {

  }
  else if(sys_num == SYS_TELL)
  {

  }
  else if(sys_num == SYS_CLOSE)
  {

  }
  */

}
