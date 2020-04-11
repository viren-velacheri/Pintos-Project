#include "vm/frame.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "threads/palloc.h"

#define NO_SPOT -1


void init_frame(void)
{
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

static bool frame_available(struct thread *t)
{
    int open_spot = open_frame();
    if(open_spot != NO_SPOT)
    {
        struct frame *f = palloc_get_page(PAL_USER);
        f->owner_thread = t;
        // f->resident_page = p;
        frame_table[open_spot] = f;
        return true;
    }
    else
    {
        return false;
    }
}





