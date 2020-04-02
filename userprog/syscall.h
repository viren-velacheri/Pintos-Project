#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
int valid_pointer(void * ptr); /*checks if address/pointer is valid. */
struct lock file_lock; /* Used for treating file system code as critical section. */
struct lock write_lock; /* Used for critical section invovling write calls. */
struct lock status_lock; /*Used for getting exit status atomically. */

#endif /* userprog/syscall.h */
