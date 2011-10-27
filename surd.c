#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "surd.h"

void
surd_init(surd_t *s, int hs, int ss)
{
  s->heap = malloc(sizeof(s->heap) * hs);
  s->heap_size = hs;
  bzero(s->heap, sizeof(s->heap) * hs);
  s->symbol_table = malloc(sizeof(s->symbol_table) * ss);
  s->symbol_table_index = 0;
  s->symbol_table_size = ss;

}


void
surd_destroy(surd_t *s)
{
  // kills the heap, kills the symbol table.
  if (s->heap) {
    free(s->heap);
  }
  if (s->symbol_table) {
    // free all the symbols
    for (i = 0; i < s->symbol_table_index; i++) {
      free(s->symbol_table[i]);
    }
    free(s->symbol_table);
    s->symbol_table_size = 0;
    s->symbol_table_index = 0;
  }
  s->env = 0;
  s->nil = 0;
}


cell_t *
surd_new_cell(surd_t *s)
{
  int i = 0;
  int tries = 1;

 search:

  // TODO: don't linear search the heap (HORRIBLY SLOW)
  for (i = 0; i < s->heap_size; i++) {
    if (s->heap[i] == NULL) {
      return &(s->heap[i]);
    }
  }
  tries--;

  // try gc. if it frees up at least one cell, we're good.
  if (gc(s)) {
    goto search;
  }

  return NULL;
}


void
surd_num_init(surd_t *s, cell_t *c, int value)
{
  c->flags = TFIXNUM;
  c->_value.num = value;
}

void
surd_intern(surd_t *s, cell_t *c, char *str)
{
  int i, newsize;
  int slen = strlen(str);

  c->flags = TSYMBOL;

  // linear search through s->symbol_table 
  for (i = 0; i < s->symbol_table_index; i++) {
    if (strncmp(s->symbol_table[i], str, slen) == 0) {
      c->_value.num = i;
      return;
    }
  }

  if (i < s->symbol_table_size) {
    s->symbol_table[i] = strndup(str, slen);
    c->_value.num = i;
  }
  else {
    newsize = sizeof(s*->symbol_table) * s->symbol_table_size * 2;
    s->symbol_table = realloc(s->symbol_table, newsize);
    if (s->symbol_table) {
      s->symbol_table[i] = strndup(str, slen);
      c->_value.num = i;
    }
    else {
      fprintf(stderr, "out of memory in intern()\n");
    }
  }
}

cell_t *
surd_cons(surd_t *s, cell_t *car, cell_t *cdr)
{
  cell_t *new = surd_new_cell(s);
  if (new) {
    new->flags = CONS;
    new->_value.cons->car = car;
    new->_value.cons->cdr = cdr;
  }
  return new;
}

static cell_t *_read(surd_t *s, FILE *in);

static cell_t *
_read_fixnum(surd_t *s, FILE *in, int sign)
{
  cell_t *fix;
  int c, i = 0;
  
  c = fgetc(in);
  while (isdigit(c)) {
    i = (i * 10) + (c - '0');
    c = fgetc(in);
  }
  ungetc(c, in);

  fix = surd_new_cell(s);
  if (fix) {
    surd_num_init(s, fix, i * sign);
    return fix;
  }

  fprintf(stderr, "Couldn't read fixnum\n");
  exit(0);
}

#define ISINITIAL(c) (isalpha(c) || \
                      c == '-' || c == '+' ||   \
                      c == '*' || c == '/' ||   \
                      c == '>' || c == '<' ||   \
                      c == '=' || c == '$' ||   \
                      c == '!' || c == '%' ||   \
                      c == '_' || c == '~' ||   \
                      c == '^' || c == '|' ||   \
                      c == '&' || \
                      c == '?' || c == '.')

#define ISSYMCHAR(c) (ISINITIAL(c) || isdigit(c) || c == '\'')
#define ISDELIM(c) (c == '(' || c == ')' || c == ';' || isspace(c))

static cell_t *
_read_symbol(surd_t *s, FILE *in)
{
  cell_t *sym;
  char buffer[128];
  int c, i = 0;

  if (ISINITIAL(c)) {
    buffer[i++] = (char)c;
  }
  else {
    fprintf(stderr, "not a symbol: invalid first char '%c'\n", (char)c);
  }

  for (;;) {
    c = fgetc(in);
    if (ISSYMCHAR(c)) {
      buffer[i++] = (char)c;
    }
    else if (ISDELIM(c)) {
      ungetc(c, in);
      goto done;
    }
    else {
      fprintf(stderr, "'%c' not a symbol character\n", (char)c);
    }
  }
 done:
  surd_intern(s, sym, buffer);
  return sym;
}

static cell_t *
_read_list(surd_t *s, FILE *in)
{
  cell_t *first, *next, *next_next, *tmp;
  int c;

  first = s->nil;

  tmp = _read(s, in);
  if (!tmp) {
    goto done;
  }

  first = surd_cons(s, tmp, s->nil);
  next = first;

  for (;;) {
    tmp = _read(s, in);
    if (!tmp) {
      goto done;
    }
    next_next = surd_cons(s, next, s->nil);
    next->_value.cons->cdr = next_next;
  }

 done:
  c = fgetc(s, in);
  if (c != ')') {
    fprintf(stderr, "expected ')', but found '%c'\n", c);
    exit(0);
  }

  return first;
}

static cell_t *
_read(surd_t *s, FILE *in)
{
  int c;
  cell_t *sym, *tmp;
  for (;;) {
    c = fgetc(in);
    switch (c) {
    case '\t':
    case '\n':
    case '\r':
    case ' ':
      continue;

    case ';':
      for (;;) {
        c = fgetc(in);
        if (c == '\n' || c == '\r' || c == EOF) {
          break;
        }
      }
      continue;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      unget(c, in);
      return _read_fixnum(s, in, 1);

    case '\'': // quote
      sym = surd_intern(s, "quote");
      tmp = surd_read(s, in);
      return surd_cons(s, sym, surd_cons(s, tmp, s->nil));

    case '(':
      return _read_list(s, in);

    case ')':
      unget(c, in);
      return 0;
    case '-':
    case '+':
      l = fgetc(in);
      if (isdigit(l)) {
        ungetc(l, in);
        return _read_fixnum(s, in, c == '-' ? -1: 1);
      }
    default:
      ungetc(c, in);
      return _read_symbol(s, in);
    }
  }
  return NULL;
}


cell_t *
surd_read(surd_t *s, FILE *in)
{
  cell_t *tmp;
  tmp = _read(s, in);
  return tmp;
}


cell_t *
surd_write(surd_t *s, FILE *out, cell_t *exp)
{
  return NULL;
}


cell_t *
surd_eval(surd_t *s, cell_t *exp)
{
  return NULL;
}


cell_t *
surd_apply(surd_t *s, cell_t *closure, cell_t *args)
{
  return NULL;
}


static void
_mark(surd_t *s)
{
  
}

static void
_sweep(surd_t *s)
{

}

int
surd_gc(surd_t *s)
{
  return 0;
}

