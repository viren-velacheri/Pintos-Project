#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <debug.h>
#include <stdint.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "devices/block.h"

struct page 
{
    struct hash_elem hash_elem;
    block_sector_t swap_index; // position on swap if applicable
    block_sector_t file_index; // position in filesys if applicable
    void *addr; // pointer to beginning of page (virtual address)
};



#endif