#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
int valid_pointer(void * ptr);
struct lock file_lock;
struct lock write_lock;
struct lock status_lock;

#endif /* userprog/syscall.h */
