#include "vm/page.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "vm/swap.h"

#define NOT_AVAIL -1 // used when spot in frame or swap isn't there.
#define NUM_SECTORS_IN_PAGE 8// number of sectors in page based off size.
//Viren drove here
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}
//Viren done driving

//Brock drove here
/*destructor method to free the page table's resources*/
void page_removal(struct hash_elem *e, void *aux) 
{
  struct page *p = hash_entry (e, struct page, hash_elem);
  //free its frame and associated memory if in the frame table
  if(p->frame_spot != NOT_AVAIL) 
  {
    void *pg = frame_table[p->frame_spot]->page;
    free(frame_table[p->frame_spot]);
    frame_table[p->frame_spot] = NULL;
  }
  //free the swap sectors for this page if on swap
  else if(p->swap_index != NOT_AVAIL)
  {
    bitmap_set_multiple(swap_table, p->swap_index, NUM_SECTORS_IN_PAGE, 0);
    p->swap_index = NOT_AVAIL;
  }
  free(p);
}

/*Method that returns the page associated with fault_addr if it is
  in the page table, otherwise returns NUL
*/
struct page *find_page(void *fault_addr){
  struct page p;
  struct hash_elem *e;
  p.addr = pg_round_down(fault_addr);
  e = hash_find(&thread_current()->page_table, &p.hash_elem);
  if(e == NULL)
    return NULL;
  return hash_entry(e, struct page, hash_elem);
}
//Brock done driving
