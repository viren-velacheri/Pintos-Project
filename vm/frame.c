#include "vm/frame.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/swap.h"
#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#define NO_SPOT -1
// void init_frame(void);
// int open_frame(void);
// void * frame_available(void);

void init_frame(void)
{
    lock_init(&frame_lock);
    unsigned i;
    for(i = 0; i < NUM_FRAMES; i++)
    {
        frame_table[i] = NULL;
    }

}

int open_frame(void)
{
    int i;
    for(i = 0; i < NUM_FRAMES; i++)
    {
        if(frame_table[i] == NULL)
        {
            return i;
        }
    }
    return NO_SPOT;
}

void *get_frame(enum palloc_flags flag, struct page *p)
{
    int open_spot = open_frame();

    if(open_spot != NO_SPOT)
    {
        void *page = palloc_get_page(flag);
        if(page == NULL)
        {
            return NULL;
        }
        struct frame *f = malloc(sizeof(struct frame));
        f->owner_thread = thread_current();
        f->page = page;
        // f->upage = upage;
        // f->kpage = kpage;
        //f->writeable = writeable;
        //f->resident_page = p;
        frame_table[open_spot] = f;
        return page;
    }
    else
    {
        open_spot = random_evict(p);
        void *page = palloc_get_page(flag);
        if(page == NULL)
        {
            return NULL;
        }
        struct frame *f = malloc(sizeof(struct frame));
        f->owner_thread = thread_current();
        f->page = page;
        // f->upage = upage;
        // f->kpage = kpage;
        //f->writeable = writeable;
        // f->resident_page = p;
        frame_table[open_spot] = f;
        return page;
        
    }
    return NULL;
}

void free_frame(void *page)
{
    int i;
    for(i = 0; i < NUM_FRAMES; i++)
    {
        if(frame_table[i]->page == page)
        {
            palloc_free_page(page);
            free(frame_table[i]);
            frame_table[i] = NULL;
        }
    }
}

int random_evict(struct page *p)
{
    lock_acquire(&swap_lock);
    int spot = random_ulong() % NUM_FRAMES;
    while(frame_table[spot] == NULL)
    {
        spot = random_ulong() % NUM_FRAMES;
    }
    size_t sector_index = bitmap_scan_and_flip(swap_table, 0, 8, false);
    if(sector_index != BITMAP_ERROR)
    {
    size_t i = 0;
    struct block *b = block_get_role(BLOCK_SWAP);
        while(i < 8) 
        {
            //printf("i: %d", i);
            block_write(b, sector_index + i, frame_table[spot]->page + i * 512);
            i++;
        }
    p->swap_index = sector_index;
    p->frame_spot = -1;
    palloc_free_page(frame_table[spot]->page);
    free(frame_table[spot]);
    frame_table[spot] = NULL;
    lock_release(&swap_lock);
    return spot;
    }
    exit(-1);
    return -1;
} 





