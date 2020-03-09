#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  if(f->esp == NULL || pagedir_get_page (thread_current()->pagedir,
    f->esp) == NULL || is_kernel_vaddr (f->esp))
      pagedir_destroy (thread_current()->pagedir);

  int sys_num = f->esp;
  thread_exit ();
}
