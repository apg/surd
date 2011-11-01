#include <stdio.h>
#include <stdlib.h>
#include "surd.h"
#include "primitives.h"

static int
_list_length(surd_t *s, cell_t *c)
{
  int len = -1;
  if (ISCONS(c)) {
    len = 0;
    while (c != s->nil) {
      if (ISCONS(c)) {
        len++;
        c = c->_value.cons.cdr;
      }
      else {
        len = -1;
        break;
      }
    }
  }
  return len;
}

cell_t *
surd_p_cons(surd_t *s, cell_t *args)
{
  int l = _list_length(s, args);
  cell_t *c = s->nil;

  if (l == 2) {
    c = surd_cons(s, args->_value.cons.car, 
                  args->_value.cons.cdr->_value.cons.car);
    if (c == s->nil) {
      fprintf(stderr, "error: out of memory in primitive cons\n");
      exit(1);
    }
  }
  else {
    fprintf(stderr, "arity error: cons takes 2 arguments\n");
    exit(1);
  }
  return c;
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
surd_p_nth(surd_t *s, cell_t *args)
{
  return s->nil;
}

// predicates
cell_t *
surd_p_consp(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISCONS(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: cons? takes 1 argument\n");
    exit(1);
  }
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
  cell_t *c;
  if (_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISFIXNUM(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: fixnum? takes 1 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_symbolp(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISSYM(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: symbol? takes 1 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_procedurep(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISPRIM(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: primitive? takes 1 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_closurep(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISCLOSURE(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: closure? takes 1 argument\n");
    exit(1);
  }
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
      exit(1);
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
  cell_t *tmp = args, *first;
  int val = 0;
  int nands = 0;
  while (tmp != s->nil && ISCONS(tmp)) {
    first = CAR(tmp);
    if (ISFIXNUM(first)) {
      if (nands > 0) {
        val -= first->_value.num;
      }
      else {
        val = first->_value.num;
      }
      nands++;
    }
    else {
      fprintf(stderr, "error: attempt to add a non fixnum: %d\n", tmp->flags);
      exit(1);
    }
    tmp = CDR(tmp);
  }
  if (nands == 1) {
    val *= -1;
  }
  tmp = surd_new_cell(s);
  surd_num_init(s, tmp, val);
  return tmp;

  return s->nil;
}

cell_t *
surd_p_mult(surd_t *s, cell_t *args)
{
  cell_t *tmp = args, *first;
  int val = 1;
  while (tmp != s->nil && ISCONS(tmp)) {
    first = CAR(tmp);
    if (ISFIXNUM(first)) {
      val *= first->_value.num;
    }
    else {
      fprintf(stderr, "error: attempt to multiply a non fixnum: %d\n", tmp->flags);
      exit(1);
    }
    tmp = CDR(tmp);
  }
  tmp = surd_new_cell(s);
  surd_num_init(s, tmp, val);
  return tmp;
}

cell_t *
surd_p_div(surd_t *s, cell_t *args)
{
  cell_t *tmp = args, *first;
  int val = 0;
  int nands = 0;
  while (tmp != s->nil && ISCONS(tmp)) {
    first = CAR(tmp);
    if (ISFIXNUM(first)) {
      if (nands > 0) {
        if (first->_value.num) {
          val /= first->_value.num;        
        }
        else {
          fprintf(stderr, "error: attempt to divide by zero\n");
          exit(1);
        }
      }
      else {
        val = first->_value.num;
      }
      nands++;
    }
    else {
      fprintf(stderr, "attempt to add a non fixnum: %d\n", tmp->flags);
    }
    tmp = CDR(tmp);
  }
  if (nands == 1) {
    val = 1 / val;
  }

  tmp = surd_new_cell(s);
  surd_num_init(s, tmp, val);
  return tmp;
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

