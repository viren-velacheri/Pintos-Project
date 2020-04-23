#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <debug.h>
#include <stdint.h>
#include <string.h>
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"

struct bitmap *swap_table; // Swap table struct
void swap_init(void); // initializes swap table.
struct lock swap_lock; // lock used for swap table accesses.

#endif