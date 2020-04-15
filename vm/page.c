#include "vm/page.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"

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

struct page *find_page(void *fault_addr){
  struct page *p;
  struct hash_elem *e;
  p->addr = fault_addr;
  e = hash_find(&thread_current()->page_table, &p->hash_elem);
  if(e == NULL)
    return NULL;
  return hash_entry(e, struct page, hash_elem);
}
