#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surd.h"
#include "primitives.h"

#define MIN(x, y) ((x < y) ? x: y)


cell_t *
surd_p_cons(surd_t *s, cell_t *args)
{
  int l = surd_list_length(s, args);
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
  if (surd_list_length(s, args) == 1) {
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
  if (surd_list_length(s, args) == 1) {
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
  if (surd_list_length(s, args) == 1) {
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
  if (surd_list_length(s, args) == 1) {
    c = CAR(args);
    if (ISPRIM(c) || ISCLOSURE(c)) {
      return c;
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: procedure? takes 1 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_closurep(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (surd_list_length(s, args) == 1) {
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
cell_t *
surd_p_display(surd_t *s, cell_t *args)
{
  if (surd_list_length(s, args) == 1) {
    surd_display(s, stdout, CAR(args));
  }
  else {
    fprintf(stderr, "arity error: display takes 1 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_gt(surd_t *s, cell_t *args)
{
  cell_t *l, *r;
  char *ls, *rs;
  if (surd_list_length(s, args) == 2) {
    l = CAR(args);
    r = CAR(CDR(args));

    if (TYPE(l) == TYPE(r)) {
      switch (TYPE(l)) {
      case TFIXNUM:
        if (l->_value.num > r->_value.num) {
          return surd_intern(s, "true");
        }
        break;
      case TSYMBOL:
        ls = s->symbol_table[l->_value.num].name;
        rs = s->symbol_table[r->_value.num].name;
        if (strncmp(ls, rs, MIN(strlen(ls), strlen(rs))) > 0) {
          return surd_intern(s, "true");
        }
      }
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: > takes 2 argument\n");
    exit(1);
  }
  return s->nil;
}


cell_t *
surd_p_lt(surd_t *s, cell_t *args)
{
  cell_t *l, *r;
  char *ls, *rs;
  if (surd_list_length(s, args) == 2) {
    l = CAR(args);
    r = CAR(CDR(args));

    if (TYPE(l) == TYPE(r)) {
      switch (TYPE(l)) {
      case TFIXNUM:
        if (l->_value.num < r->_value.num) {
          return surd_intern(s, "true");
        }
        break;
      case TSYMBOL:
        ls = s->symbol_table[l->_value.num].name;
        rs = s->symbol_table[r->_value.num].name;
        if (strncmp(ls, rs, MIN(strlen(ls), strlen(rs))) < 0) {
          return surd_intern(s, "true");
        }
      }
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: < takes 2 argument\n");
    exit(1);
  }
  return s->nil;
}


cell_t *
surd_p_ge(surd_t *s, cell_t *args)
{
  cell_t *l, *r;
  char *ls, *rs;
  if (surd_list_length(s, args) == 2) {
    l = CAR(args);
    r = CAR(CDR(args));

    if (TYPE(l) == TYPE(r)) {
      switch (TYPE(l)) {
      case TFIXNUM:
        if (l->_value.num >= r->_value.num) {
          return surd_intern(s, "true");
        }
        break;
      case TSYMBOL:
        ls = s->symbol_table[l->_value.num].name;
        rs = s->symbol_table[r->_value.num].name;
        if (strncmp(ls, rs, MIN(strlen(ls), strlen(rs))) >= 0) {
          return surd_intern(s, "true");
        }
      }
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: >= takes 2 argument\n");
    exit(1);
  }
  return s->nil;
}


cell_t *
surd_p_le(surd_t *s, cell_t *args)
{
  cell_t *l, *r;
  char *ls, *rs;
  if (surd_list_length(s, args) == 2) {
    l = CAR(args);
    r = CAR(CDR(args));

    if (TYPE(l) == TYPE(r)) {
      switch (TYPE(l)) {
      case TFIXNUM:
        if (l->_value.num <= r->_value.num) {
          return surd_intern(s, "true");
        }
        break;
      case TSYMBOL:
        ls = s->symbol_table[l->_value.num].name;
        rs = s->symbol_table[r->_value.num].name;
        if (strncmp(ls, rs, MIN(strlen(ls), strlen(rs))) <= 0) {
          return surd_intern(s, "true");
        }
      }
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: <= takes 2 argument\n");
    exit(1);
  }
  return s->nil;
}


cell_t *
surd_p_eq(surd_t *s, cell_t *args)
{
  cell_t *l, *r;
  char *ls, *rs;
  if (surd_list_length(s, args) == 2) {
    l = CAR(args);
    r = CAR(CDR(args));

    if (TYPE(l) == TYPE(r)) {
      switch (TYPE(l)) {
      case TFIXNUM:
        if (l->_value.num == r->_value.num) {
          return surd_intern(s, "true");
        }
        break;
      case TSYMBOL:
        ls = s->symbol_table[l->_value.num].name;
        rs = s->symbol_table[r->_value.num].name;
        if (strncmp(ls, rs, MIN(strlen(ls), strlen(rs))) == 0) {
          return surd_intern(s, "true");
        }
      }
    }
    return s->nil;
  }
  else {
    fprintf(stderr, "arity error: >= takes 2 argument\n");
    exit(1);
  }
  return s->nil;
}

cell_t *
surd_p_read(surd_t *s, cell_t *args)
{
  cell_t *c;
  if (surd_list_length(s, args) == 0) {
    c = surd_read(s, stdin);
    if (c) {
      return c;
    }
    return s->nil;
  }
  fprintf(stderr, "arity error: read takes 0 arguments\n");
  exit(1);
}

cell_t *
surd_p_write(surd_t *s, cell_t *args)
{
  cell_t *tmp = args, *first;
  int i = 0;
  while (tmp != s->nil && ISCONS(tmp)) {
    first = CAR(tmp);
    if (i++ > 0) {
      fputc(' ', stdout);
    }
    surd_write(s, stdout, first);
    tmp = CDR(tmp);
  }
  return s->nil;
}


cell_t *
surd_p_symbols(surd_t *s, cell_t *ignore)
{
  int i;
  for (i = 0; i < s->symbol_table_index; i++) {
    fprintf(stderr, "%d. (%s)  0x%lx\n", i, s->symbol_table[i].name, (unsigned long)s->symbol_table[i].symbol);
  }
  return s->nil;
}
