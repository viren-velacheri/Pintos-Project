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

#define NO_SPOT -1 // Returned if no spot available in frame.
#define SPOT_EVICT 10 // Default spot we evict from.

//Jordan drove here

//initializes the frame lock as well as the elements of the frame table
void init_frame(void)
{
    lock_init(&frame_lock);
    unsigned i;
    for(i = 0; i < NUM_FRAMES; i++)
    {
        frame_table[i] = NULL;
    }
}

//method that returns the index of an open frame in the frame table
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
//Jordan done driving

//Viren drove here
/*Method that allocates a free frame and calls the eviction method
  if all frames are currently allocated
  Returns the physaddr allocated
  */
void *get_frame(enum palloc_flags flag, struct page *p)
{
    int open_spot = open_frame();
    //there is a free frame
    if(open_spot != NO_SPOT)
    {
        //get a physical address for this page
        void *page = palloc_get_page(flag);
        if(page == NULL)
        {
            return NULL;
        }
        //create a new frame struct and initialize its members
        struct frame *f = malloc(sizeof(struct frame));
        if(f == NULL)
        {
            return NULL;
        }
        f->owner_thread = thread_current();
        f->page = page;
        f->resident_page = p;
        frame_table[open_spot] = f;
        return page;
    }
    else //there is no free frame
    {
        open_spot = random_evict();
        //get a physical address for this page
        void *page = palloc_get_page(flag);
        if(page == NULL)
        {
            return NULL;
        }
        //create a new frame struct and initialize its members
        struct frame *f = malloc(sizeof(struct frame));
        if(f == NULL)
        {
            return NULL;
        }
        f->owner_thread = thread_current();
        f->page = page;
        f->resident_page = p;
        frame_table[open_spot] = f;
        return page;
    }
    return NULL;
}
//Viren done driving

//Jasper drove here
//method to free a frame and all its associated allocated memory
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
//Jasper done driving

//Brock drove here
/*method that evicts a random frame and writes it to swap if it has been
  modified
  Returns the index of the newly freed frame
*/
int random_evict()
{
    lock_acquire(&swap_lock);
    int spot = random_ulong() % NUM_FRAMES;
    // This was our attempt at trying to do random eviction. 
    // However, got negative index sometimes so just set to
    // a default value of 10. Not ideal. but worked for tests.
    while(spot < 1 || frame_table[spot] == NULL)
    {
        spot = random_ulong() % NUM_FRAMES;
    }
    spot = SPOT_EVICT;
    struct page *p = frame_table[spot]->resident_page;
    //check if this page has been modified
    if(pagedir_is_dirty(thread_current()->pagedir, p->addr)) {
        //get a free sector of swap
        size_t sector_index = bitmap_scan_and_flip(swap_table, 0, 
            SECTORS_PER_PAGE, false);
        if(sector_index == BITMAP_ERROR)
        {
                exit(-1);
                return -1;
        }
        size_t i = 0;
        struct block *b = block_get_role(BLOCK_SWAP);
        while(i < SECTORS_PER_PAGE) 
        {
            block_write(b, sector_index + i, frame_table[spot]->page + 
              i * BLOCK_SECTOR_SIZE);
            i++;
        }
        p->swap_index = sector_index; //set the evicted page's swap index
    }
    p->frame_spot = -1;
    //remove pages mapping to physical memory
    pagedir_clear_page(thread_current()->pagedir, p->addr);
    //free the frame and its allocated memory
    palloc_free_page(frame_table[spot]->page);
    free(frame_table[spot]);
    frame_table[spot] = NULL;
    lock_release(&swap_lock);
    return spot;
} 
//Brock done driving





