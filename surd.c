#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "surd.h"

#define _PRE_INTERNED_SYMBOLS_SIZE 4
char *_symbols_to_intern[] = {
  "quote", "if", "lambda", "def"
};

static int
_symbol_position(surd_t *s, char *sym) 
{
  int i;
  int slen = strlen(sym);

  for (i = 0; i < s->symbol_table_index; i++) {
    if (strncmp(s->symbol_table[i].name, sym, slen) == 0) {
      return i;
    }
  }
  return -1;
}

static cell_t *
_env_lookup(surd_t *s, cell_t *env, cell_t *sym)
{
  cell_t *tmp;
  tmp = env;
  while (tmp != s->nil) {
    surd_display(s, stdout, tmp);
    if (CAR(CAR(tmp)) == sym) {
      return CAR(CDR(CAR(tmp)));
    }
    tmp = CDR(tmp);
  }
  fprintf(stderr, "symbol %s not found\n", s->symbol_table[sym->_value.num].name);
  exit(1);
}

static cell_t *
_env_insert(surd_t *s, cell_t *env, cell_t *sym, cell_t *value)
{
  cell_t *c = surd_cons(s, sym, value);
  return surd_cons(s, c, env);
}

static cell_t *
_eval_list(surd_t *s, cell_t *list, cell_t *env)
{
  cell_t *first, *next, *next_next, *tmp, *evaled;
  if (list == s->nil) {
    return s->nil;
  }
  else {
    first = surd_cons(s, surd_eval(s, CAR(list), env),  s->nil);
  }

  next = first;
  tmp = CDR(list);

  while (tmp != s->nil) {
    evaled = surd_eval(s, CAR(tmp), env);
    next_next = surd_cons(s, evaled, s->nil);
    next->_value.cons.cdr = next_next;
    next = next_next;
    tmp = CDR(tmp);
  }

  return first;
}

static cell_t *
_eval_if(surd_t *s, cell_t *exp, cell_t *env)
{
  return s->nil;
}


void 
surd_init(surd_t *s, int hs, int ss)
{
  int i;
  s->heap = malloc(sizeof(s->heap) * hs);
  s->heap_size = hs;
  bzero(s->heap, sizeof(s->heap) * hs);
  s->symbol_table = malloc(sizeof(s->symbol_table) * ss);
  s->symbol_table_index = 0;
  s->symbol_table_size = ss;
  s->nil = malloc(sizeof(s->nil));
  s->env = surd_new_cell(s);

  // intern some useful symbols.
  for (i = 0; i < _PRE_INTERNED_SYMBOLS_SIZE; i++) {
    surd_intern(s, _symbols_to_intern[i]);
  }
}

void
surd_destroy(surd_t *s)
{
  int i;

  // kills the heap, kills the symbol table.
  if (s->heap) {
    free(s->heap);
  }
  if (s->symbol_table) {
    // free all the symbols
    for (i = 0; i < s->symbol_table_index; i++) {
      free(s->symbol_table[i].name);
    }
    free(s->symbol_table);
    s->symbol_table_size = 0;
    s->symbol_table_index = 0;
  }
  s->env = NULL;
  free(s->nil);
  s->nil = NULL;
}


cell_t *
surd_new_cell(surd_t *s)
{
  int i = 0;
  int tries = 1;

 search:
  // TODO: don't linear search the heap (HORRIBLY SLOW)
  for (i = 0; i < s->heap_size; i++) {
    if (s->heap[i].flags == 0) {
      return &(s->heap[i]);
    }
  }
  tries--;

  // try gc. if it frees up at least one cell, we're good.
  if (surd_gc(s)) {
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

cell_t *
surd_intern(surd_t *s, char *str)
{
  cell_t *c;
  int i, newsize;
  int slen = strlen(str);

  i = _symbol_position(s, str);
  if (i >= 0) {
    return s->symbol_table[i].symbol;
  }
  i = s->symbol_table_index;
  // didn't find it, so put the index at the end

  if (i < s->symbol_table_size) {
    c = surd_new_cell(s);
    if (c) {
      c->flags = TSYMBOL;
      c->_value.num = i;
      s->symbol_table[i].name = strndup(str, slen);
      s->symbol_table[i].symbol = c;
      s->symbol_table_index++;
      return c;
    }
    else {
      fprintf(stderr, "out of memory in intern()\n");
      exit(1);
    }
  }
  else {
    newsize = sizeof(*s->symbol_table) * s->symbol_table_size * 2;
    s->symbol_table = realloc(s->symbol_table, newsize);
    if (s->symbol_table) {
      c = surd_new_cell(s);
      if (c) {
        c->flags = TSYMBOL;
        c->_value.num = i;
        s->symbol_table[i].name = strndup(str, slen);
        s->symbol_table[i].symbol = c;
        s->symbol_table_index++;
      }
      else {
        fprintf(stderr, "out of memory in intern()\n");
        exit(1);
      }
      return c;
    }
    else {
      fprintf(stderr, "out of memory in intern()\n");
      exit(1);
    }
  }
}

cell_t *
surd_cons(surd_t *s, cell_t *car, cell_t *cdr)
{
  cell_t *new = surd_new_cell(s);
  if (new) {
    new->flags = TCONS;
    new->_value.cons.car = car;
    new->_value.cons.cdr = cdr;
  }
  return new;
}

cell_t *
surd_make_closure(surd_t *s, cell_t *code, cell_t *env)
{
  cell_t *cls;
  cls = surd_cons(s, code, env);
  cls->flags = TCLOSURE;
  return cls;
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
  exit(1);
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

static void
_eat_comment(FILE *in)
{
  int c;
  do {
    c = fgetc(in);
  } while(c != '\r' || c != '\n' || c != EOF);
}

static cell_t *
_read_symbol(surd_t *s, FILE *in)
{
  cell_t *sym;
  char buffer[128];
  int c, i = 0;

  c = fgetc(in);
  if (ISINITIAL(c)) {
    buffer[i++] = (char)c;
    buffer[i] = 0;
  }
  else {
    fprintf(stderr, "not a symbol: invalid first char '%c'\n", (char)c);
  }

  for (;;) {
    c = fgetc(in);
    if (ISSYMCHAR(c)) {
      buffer[i++] = (char)c;
      buffer[i] = 0;
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
  sym = surd_new_cell(s);
  surd_intern(s, buffer);
  return sym;
}

static cell_t *
_read_list(surd_t *s, FILE *in)
{
  cell_t *first, *next, *next_next, *tmp;
  int c;

  tmp = _read(s, in);
  if (!tmp) {
    fgetc(in); // TODO we really need "next_token" instead of fgetc
    return s->nil;
  }

  first = surd_cons(s, tmp, s->nil);
  next = first;

  for (;;) {
    tmp = _read(s, in);
    if (!tmp) { // got an end delimiter, so we're done
      goto done;
    }
    printf("read: %p\n", tmp);
    next_next = surd_cons(s, tmp, s->nil);
    next->_value.cons.cdr = next_next;
    next = next_next;
  }

 done:
  c = fgetc(in);
  if (c != ')') {
    fprintf(stderr, "expected ')', but found '%c'\n", c);
    exit(1);
  }
  return first;
}

static cell_t *
_read(surd_t *s, FILE *in)
{
  int c, l;
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
      _eat_comment(in);
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
      ungetc(c, in);
      return _read_fixnum(s, in, 1);

    case '\'': // quote
      sym = surd_intern(s, "quote");
      tmp = surd_read(s, in);
      return surd_cons(s, sym, surd_cons(s, tmp, s->nil));

    case '(':
      return _read_list(s, in);

    case ')':
      ungetc(c, in);
      return NULL;
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

void
surd_display(surd_t *s, FILE *out, cell_t *exp)
{
  int sep = 0;
  cell_t *tmp;
  if (exp == s->nil) {
    fprintf(out, "()");
  }
  else if (ISFIXNUM(exp)) {
    fprintf(out, "%d", exp->_value.num);
  }
  else if (ISSYM(exp)) {
    fprintf(out, "%s", s->symbol_table[exp->_value.num].name);
  }
  else if (ISCONS(exp)) {
    fprintf(out, "(");
    tmp = exp;
    while (tmp != s->nil) {
      if (sep) { fprintf(out, " "); }
      surd_display(s, out, CAR(tmp));
      tmp = CDR(tmp);
      sep = 1;
    }
    fprintf(out, ")");
  }
  else if (ISCLOSURE(exp)) {
    fprintf(out, "<#closure: %p>", exp);
  }
  else {
    fprintf(out, "umm...");
  }
}

void
surd_write(surd_t *s, FILE *out, cell_t *exp)
{
  // not correct, but we'll take it for now.
  surd_display(s, out, exp);
}

cell_t *
surd_eval(surd_t *s, cell_t *exp, cell_t *env)
{
  cell_t *car, *tmp;
  int sym;
  if (ISFIXNUM(exp) || ISCLOSURE(exp) || ISPRIM(exp)) {
    return exp;
  }
  else if (ISSYM(exp)) {
    return _env_lookup(s, env, exp);
  }
  else {
    car = CAR(exp);
    if (car == surd_intern(s, "quote")) {
      tmp = CDR(exp);
      if (ISCONS(tmp)) {
        return CAR(tmp);
      }
      else {
        fprintf(stderr, "attempted to take the car of nil\n");
        // dont' blow up, just return nil
        return s->nil;
      }
    }
    else if (car == surd_intern(s, "if")) {
      return _eval_if(s, exp, env);
    }
    else if (car == surd_intern(s, "lambda")) {
      return surd_make_closure(s, exp, env);
    }
    else {
      tmp = surd_eval(s, car, env);
      if (ISPRIM(tmp) || ISCLOSURE(tmp)) {
        fprintf(stderr, "attempt to apply!");
        // surd_apply(s, tmp, _eval_list(s, CDR(exp)))
        return s->nil;
      }
    }
  }

  return s->nil;
}

cell_t *
surd_apply(surd_t *s, cell_t *closure, cell_t *args)
{
  return s->nil;
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

