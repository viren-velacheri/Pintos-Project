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

void page_removal(struct hash_elem *e, void *aux) 
{
  struct page *p = hash_entry (e, struct page, hash_elem);
  if(p->frame_spot != -1) 
  {
  void *pg = frame_table[p->frame_spot]->page;
  //printf("Page File: %p   Offset: %d", pg, pg_ofs(pg));
  free(frame_table[p->frame_spot]);
  frame_table[p->frame_spot] = NULL;
  //palloc_free_page(pg);
  }
  else if(p->swap_index != -1)
  {
    bitmap_set_multiple(swap_table, p->swap_index, 8, 0);
    p->swap_index = -1;
  }
  free(p);
}

struct page *find_page(void *fault_addr){
  struct page p;
  struct hash_elem *e;
  p.addr = pg_round_down(fault_addr);
  e = hash_find(&thread_current()->page_table, &p.hash_elem);
  if(e == NULL)
    return NULL;
  return hash_entry(e, struct page, hash_elem);
}
