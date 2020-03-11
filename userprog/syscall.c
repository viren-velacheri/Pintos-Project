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
  lock_init(file_lock);
}

void exit(int status) {
  thread_current()->exit_status = status;
  //sema_down()
  sema_up(thread_current()->proc_wait);
  process_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  if(!valid_pointer(f->esp))
  {
    pagedir_destroy (thread_current()->pagedir);
    return;
  }
  switch(*(int *)f->esp) {
    case SYS_HALT:
      shutdown_power_off();
      break;
    
    case SYS_EXIT:
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      int status = *(int *) f->esp;
      exit(status);
      /*
      thread_current()->exit_status = status;
      //sema_down()
      sema_up(thread_current()->proc_wait);
      process_exit();
      */
      break;
    
    case SYS_EXEC:
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      char * cmd_line = *(int *) f->esp; //(get_argument(f->esp));
      if(!valid_pointer(cmd_line))
        exit(-1);
      tid_t child_tid = process_execute(cmd_line);
      sema_down(thread_current()->exec_sema);
      if(!thread_current()->childLoaded)
        child_tid = -1;
      f->eax = child_tid;
      break;
    
    case SYS_WAIT:
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      tid_t pid = *(int *) f->esp; //(get_argument(f->esp));
      f->eax = process_wait(pid);
      break;
    
    case SYS_CREATE:
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      char * file = *(int *) f->esp;
      if(!valid_pointer(file))
        return;
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      uint32_t initial_size = *(int *) f->esp;
      lock_acquire(file_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(file_lock);
      break;
    
    case SYS_REMOVE:
      (int *)(f->esp)++;
      if(!valid_pointer(f->esp))
        exit(-1);
      char * file2 = *(int *) f->esp;
      if(!valid_pointer(file2))
        return;
      lock_acquire(file_lock);
      f->eax = filesys_remove(file2);
      lock_release(file_lock);
      break;
    
    case SYS_OPEN:

      break;
    
    case SYS_FILESIZE:

      break;
    
    case SYS_READ:

      break;
    
    case SYS_WRITE:

      break;
    
    case SYS_SEEK:

      break;
    
    case SYS_TELL:

      break;

    case SYS_CLOSE:

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
