#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void valid_pointer_check(void * ptr); /*checks if pointer is valid. */
struct lock file_lock; /* Treats file system code as critical section. */
struct lock write_lock; /* Used for critical section invovling write calls. */
void exit(int status);

#endif /* userprog/syscall.h */
