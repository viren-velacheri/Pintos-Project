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
#include "threads/palloc.h"
#include "filesys/file.h"

#define NUM_FRAMES 367 // Number of pages in user pool.
#define SECTORS_PER_PAGE 8 //sectors are 512 bytes, pages are 4096
struct frame {
    struct thread *owner_thread; // thread that owns the page in this frame
    void *page; //physical address of the page in this frame
    struct page *resident_page; //page residing in this frame
};

struct frame *frame_table[NUM_FRAMES]; //frame table
struct lock frame_lock;

void init_frame(void);
int open_frame(void);
void * get_frame(enum palloc_flags flag, struct page *p);
void free_frame(void *page);
#endif