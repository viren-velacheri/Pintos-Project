#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <debug.h>
#include <stdint.h>
#include <string.h>
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "filesys/file.h"

#define NUM_FRAMES 367

struct frame {
    struct thread *owner_thread; // thread that owns the page in this frame
	void *upage;
    void *kpage;
    void *page;
    struct page *resident_page;
    bool writeable;
};

struct frame *frame_table[NUM_FRAMES]; 
struct lock frame_lock;

void init_frame(void);
int open_frame(void);
void * frame_available(struct thread *t);
#endif