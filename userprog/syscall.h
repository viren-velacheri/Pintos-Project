#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void valid_pointer_check(void * ptr); /*checks if pointer is valid. */
struct lock write_lock; /* Used for critical section invovling write calls. */

#endif /* userprog/syscall.h */
