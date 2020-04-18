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
  void *pg = frame_table[p->frame_spot]->page;
  frame_table[p->frame_spot] = NULL;
  palloc_free_page(pg);
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
