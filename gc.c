#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surd.h"

/**
   One pass procecedures
 */

static int
marked(cell_t *c)
{
  return c->flags & (1 << MARK_BIT);
}

static void
mark(cell_t *c)
{
  c->flags |= 1<<MARK_BIT;
}

static void
unmark(cell_t *c)
{
  if (marked(c)) {
    c->flags ^= 1<<MARK_BIT;
  }
}

static int
type(cell_t *c)
{
  return c->flags & ((1 << (TYPE_BITS+1)) - 1);
}

static void
possibly_mark_object(cell_t *c)
{
  switch(type(c)) {
  case TCONS:
    mark(c->_value.cons.car); 
    mark(c->_value.cons.cdr); 
    break;
  case TCLOSURE:
    mark(c->_value.cons.car); 
    mark(c->_value.cons.cdr); 
    break;
  }
}

static void
free_cell(surd_t *s, cell_t *c)
{
  c->_value.cons.cdr = s->free_list;
  s->free_list = c;
  s->free_list_cells++;
}

static void
mark_roots(surd_t *s)
{
  int i;

  mark(s->env);
  mark(s->top_env);

  // mark symbols
  for (i = 0; i < s->symbol_table_index; i++) {
    mark(s->symbol_table[i].symbol);
  }
}

cell_t *
surd_new_cell(surd_t *s)
{
  int reclaimed;
  int tries = 1;
  cell_t *next;

 search:
  if (s->free_list != s->nil && s->free_list_cells > 0) {
    // printf("Free list is: %p, nil is: %p\n", s->free_list, s->nil);
    // zero out to avoid leaking free list pointers
    next = s->free_list;
    s->free_list = s->free_list->_value.cons.cdr;

    memset(next, 0, sizeof(*next));
    s->free_list_cells--;
    next->hist = s->last_alloc;
    s->last_alloc = next;
    return next;
  } else if (s->bump < s->heap_ceil) {
    fprintf(stderr, "Bumping!\n");
    /* bump allocate as long as we have space! */
    next = s->bump++;
    next->hist = s->last_alloc;
    s->last_alloc = next;

    if (s->first_alloc == NULL) {
      s->first_alloc = next;
    }
    return next;
  } else {
    fprintf(stderr, "Free list is empty and no bump space?\n");
  }
  tries--;

  // try gc. if it frees up at least one cell, we're good.
  fprintf(stderr, "gc...");
  reclaimed = surd_gc(s);
  if (reclaimed > 0) {
    fprintf(stderr, "reclaimed %d cells\n", reclaimed);
    goto search;
  } else {
    fprintf(stderr, "OUT OF MEMORY!!\n");
    exit(1);
  }
  // will never get here.. 
  return s->nil;
}

/* TODO: Make this incremental after we know it works! */
int
surd_gc(surd_t *s)
{
  cell_t *tmp, *last;
  int count = 0;
  mark_roots(s);
  last = s->last_alloc;
  s->SCAV = last->hist;
  while (s->SCAV != s->first_alloc) {
    if (marked(s->SCAV)) {
      possibly_mark_object(s->SCAV);
      unmark(s->SCAV);
      last = s->SCAV;
      s->SCAV = last->hist;
    }
    else {
      tmp = s->SCAV;
      s->SCAV = s->SCAV->hist;
      last->hist = s->SCAV;
      free_cell(s, tmp);
      count++;
    }
  }
  return count;
}
