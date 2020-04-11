#include "threads/thread.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "vm/page.h"

#define NUM_FRAMES 367

struct frame {
    struct thread *owner_thread; //thread that owns the page in this frame
	void *resident_page;
    bool writeable;
};

struct frame *frame_table[NUM_FRAMES]; 

void init_frame(void);
int open_frame(void);
static bool frame_available(struct thread *t);

