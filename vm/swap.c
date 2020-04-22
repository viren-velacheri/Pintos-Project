#include "vm/swap.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/synch.h"

//Jasper drove here
//Method to initialize the swap table to all zeroes
void swap_init() 
{
    swap_table = bitmap_create(block_size(block_get_role(BLOCK_SWAP)));
    lock_init(&swap_lock);
    bitmap_set_all(swap_table, 0);
}
//Jasper done driving