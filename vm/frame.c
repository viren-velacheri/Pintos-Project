#include "vm/frame.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/synch.h"

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

void *get_frame(enum palloc_flags flag)
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
        // f->resident_page = p;
        frame_table[open_spot] = f;
        return page;
    }
    else
    {
        PANIC("PAGE FAULT");
    }
    return NULL;
}





