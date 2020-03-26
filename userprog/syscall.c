#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/synch.h"


static void syscall_handler (struct intr_frame *);

int valid_pointer(void * ptr) {
  if(ptr == NULL || pagedir_get_page (thread_current()->pagedir,
    ptr) == NULL || is_kernel_vaddr (ptr))
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
  lock_init(&file_lock);
}

void exit(int status) {
  thread_current()->exit_status = status;
  //sema_down()
  //sema_up(thread_current()->proc_wait);
  process_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *temp_esp = f->esp;
  if(!valid_pointer(temp_esp))
  {
    pagedir_destroy (thread_current()->pagedir);
    return;
  }
  switch(*(int *)temp_esp) {
    case SYS_HALT:
      shutdown_power_off();
      break;
    
    case SYS_EXIT:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int status = *(int *) temp_esp;
      exit(status);
      /*
      thread_current()->exit_status = status;
      //sema_down()
      sema_up(thread_current()->proc_wait);
      process_exit();
      */
      break;
    
    case SYS_EXEC:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      char * cmd_line = *(int *) temp_esp; //(get_argument(f->esp));
      if(!valid_pointer(cmd_line))
        exit(-1);
      tid_t child_tid = process_execute(cmd_line);
      sema_down(&get_thread_from_tid(child_tid)->exec_sema);
      if(!thread_current()->childLoaded)
        child_tid = -1;
      f->eax = child_tid;
      break;
    
    case SYS_WAIT:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      tid_t pid = *(int *) temp_esp; //(get_argument(f->esp));
      f->eax = process_wait(pid);
      break;
    
    case SYS_CREATE:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      char * file = *(int *) temp_esp;
      if(!valid_pointer(file))
        return;
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      uint32_t initial_size = *(int *) temp_esp;
      lock_acquire(&file_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(&file_lock);
      break;
    
    case SYS_REMOVE:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      char * file2 = *(int *) temp_esp;
      if(!valid_pointer(file2))
        return;
      lock_acquire(&file_lock);
      f->eax = filesys_remove(file2);
      lock_release(&file_lock);
      break;
    
    case SYS_OPEN:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      char * file3 = *(int *)temp_esp;
      if(!valid_pointer(file3))
        return;
      lock_acquire(&file_lock);
      struct file *open_file = filesys_open(file3);
      lock_release(&file_lock);
      if(open_file == NULL) {
        f->eax = -1;
      } else {
        struct thread *t = thread_current();
        t->set_of_files[t->curr_file_index] = open_file;
        f->eax = t->curr_file_index;
        t->curr_file_index++;
      }
      break;
    
    case SYS_FILESIZE:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd = *(int *) temp_esp;
      struct thread *t_size = thread_current();
      if(fd < 2 || fd > t_size->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index = t_size->set_of_files[fd];
      lock_acquire(&file_lock);
      f->eax = file_length(file_at_index);
      lock_release(&file_lock);
      break;
    
    case SYS_READ:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd_read = *(int *) temp_esp;
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      void *buffer = *(int *) temp_esp;
      if(!valid_pointer(buffer))
        exit(-1);
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      unsigned size = *(int *)temp_esp;
      struct thread *t_read = thread_current();
      if(fd_read == 0) {
        f->eax = input_getc();
        break;
      }
      if(fd_read < 2 || fd_read > t_read->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index2 = t_read->set_of_files[fd_read];
      lock_acquire(&file_lock);
      f->eax = file_read(file_at_index2, buffer, size);
      lock_release(&file_lock);
      break;
    
    case SYS_WRITE:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd_write = *(int *) temp_esp;
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      const void *buffer_write = *(int *) temp_esp;
      if(!valid_pointer(buffer_write))
        exit(-1);
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      unsigned size_write = *(int *)temp_esp;
      struct thread *t_write = thread_current();
      if(fd_write < 2 || fd_write > t_write->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index3 = t_write->set_of_files[fd_write];
      lock_acquire(&file_lock);
      putbuf((const char *)buffer_write, size_write);
      f->eax = file_write(file_at_index3, buffer_write, size_write);
      lock_release(&file_lock);

      break;
    
    case SYS_SEEK:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd_seek = *(int *) temp_esp;
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      unsigned position = *(int *)temp_esp;
      struct thread *t_seek = thread_current();
      if(fd_seek < 2 || fd_seek > t_seek->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index4 = t_seek->set_of_files[fd_seek];
      lock_acquire(&file_lock);
      f->eax = file_seek(file_at_index4, position);
      lock_release(&file_lock);
      break;
    
    case SYS_TELL:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd_tell = *(int *) temp_esp;
      struct thread *t_tell = thread_current();
      if(fd_tell < 2 || fd_tell > t_tell->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index5 = t_tell->set_of_files[fd_tell];
      lock_acquire(&file_lock);
      f->eax = file_tell(file_at_index5);
      lock_release(&file_lock);
      break;

    case SYS_CLOSE:
      (int *)(temp_esp)++;
      if(!valid_pointer(temp_esp))
        exit(-1);
      int fd_close = *(int *) temp_esp;
      struct thread *t_close = thread_current();
      if(fd_close < 2 || fd_close > t_close->curr_file_index) {
        exit(-1);
      }
      struct file *file_at_index6 = t_close->set_of_files[fd_close];
      lock_acquire(&file_lock);
      f->eax = file_close(file_at_index6);
      lock_release(&file_lock);
      break;
  }
  thread_exit ();

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
