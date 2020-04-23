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
#include "filesys/file.h"

struct page 
{
    struct hash_elem hash_elem; // used for accessing in suppl. page table.
    block_sector_t swap_index; // position on swap if applicable
    void *addr; //virtual address for this page
    struct file *resident_file; //file where the page is stored
    off_t offset; //offset for that file
    uint32_t read_bytes; //number of bytes to read
    uint32_t zero_bytes; //number of zero bytes
    bool writable; //whether this page is writable or not
    int frame_spot; //index of this page in the frame table
};

/* Hashes page. */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
/* Orders pages in suppl page table. */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
/* Helper method used for seeing if page with address exists in page table. */
struct page *find_page(void *fault_addr);
/* Used for removing pages in page reclamation in hash_destroy.*/
void page_removal(struct hash_elem *e, void *aux);

#endif