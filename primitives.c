#include <stdio.h>
#include "surd.h"
#include "primitives.h"

cell_t *
surd_p_cons(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_first(surd_t *s, cell_t *args)
{
  cell_t *arg1;
  if (args != s->nil) {
    arg1 = CAR(args);
    if (ISCONS(arg1)) {
      return CAR(arg1);
    }
  }
  return s->nil;
}

cell_t *
surd_p_rest(surd_t *s, cell_t *args)
{
  cell_t *arg1 = CAR(args);
  if (ISCONS(arg1)) {
    return CDR(arg1);
  }
  return s->nil;
}

cell_t *
surd_p_second(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_third(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_fourth(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_fifth(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_nth(surd_t *s, cell_t *args)
{
  return s->nil;
}

// predicates
cell_t *
surd_p_consp(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_nilp(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_fixnump(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_symbolp(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_procedurep(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_closurep(surd_t *s, cell_t *args)
{
  return s->nil;
}

// basic arithmetic
cell_t *
surd_p_plus(surd_t *s, cell_t *args)
{
  cell_t *tmp = args, *first;
  int val = 0;
  while (tmp != s->nil && ISCONS(tmp)) {
    first = CAR(tmp);
    if (ISFIXNUM(first)) {
      val += first->_value.num;
    }
    else {
      fprintf(stderr, "attempt to add a non fixnum: %d\n", tmp->flags);
    }
    tmp = CDR(tmp);
  }
  tmp = surd_new_cell(s);
  surd_num_init(s, tmp, val);
  return tmp;
}

cell_t *
surd_p_minus(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_mult(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_div(surd_t *s, cell_t *args)
{
  return s->nil;
}

cell_t *
surd_p_mod(surd_t *s, cell_t *args)
{
  return s->nil;
}

// display
cell_t *surd_p_display(surd_t *s, cell_t *args)
{
  return s->nil;
}

