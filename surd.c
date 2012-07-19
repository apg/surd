#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "surd.h"

#include "primitives.h"

#define _PRE_INTERNED_SYMBOLS_SIZE 4
static char *_symbols_to_intern[] = {
  "quote", "if", "lambda", "def"
};

static inline int
_is_initial(int c) {
  return (isalpha(c) || \
          c == '-' || c == '+' ||               \
          c == '*' || c == '/' ||               \
          c == '>' || c == '<' ||               \
          c == '=' || c == '$' ||               \
          c == '!' || c == '%' ||               \
          c == '_' || c == '~' ||               \
          c == '^' || c == '|' ||               \
          c == '&' ||                           \
          c == '?' || c == '.');
}

static inline int
_is_sym_char(int c) 
{
  return (_is_initial(c) || isdigit(c) || c == '\'');
}

static inline int
_is_delim(int c)
{
  return (c == '(' || c == ')' || c == ';' || isspace(c));
}

inline cell_t *
surd_car(surd_t *s, cell_t *c)
{
  if (ISCONS(c)) {
    return CAR(c);
  }
  return s->nil;
}

inline cell_t *
surd_cdr(surd_t *s, cell_t *c)
{
  if (ISCONS(c)) {
    return CDR(c);
  }
  return s->nil;
}

static int
_symbol_position(surd_t *s, char *sym)
{
  int i;
  int slen = strlen(sym);
  for (i = 0; i < s->symbol_table_index; i++) {
    if (s->symbol_table[i].name) {
      if (strncmp(s->symbol_table[i].name, sym, slen) == 0) {
        return i;
      }
    }
    else {
      break;
    }
  }
  return -1;
}


static cell_t *
_env_lookup(surd_t *s, cell_t *env, cell_t *sym)
{
  cell_t *envs[] = { env, s->top_env };
  cell_t *tmp;
  int i;

  for (i = 0; i < 2; i++) {
    fprintf(stderr, "debug: in here\n");
    tmp = envs[i];
    while (tmp != s->nil && tmp != NULL) {
      if (ISCONS(tmp)) {
        if (ISCONS(CAR(tmp))) {
          if (CAR(CAR(tmp)) == sym) {
            return CDR(CAR(tmp));
          }
        }
      }
      tmp = CDR(tmp);
    }
    fprintf(stderr, "debug: didn't find it in that pass\n");
  }

  fprintf(stderr, "error: symbol %s not found\n", s->symbol_table[sym->_value.num].name);
  fflush(stderr);
  exit(1);
}

static cell_t *
_env_insert(surd_t *s, cell_t *env, cell_t *sym, cell_t *value)
{
  cell_t *c = surd_cons(s, sym, value);
  return surd_cons(s, c, env);
}

static cell_t *
_env_extend(surd_t *s, cell_t *env, cell_t *params, cell_t *args)
{
  cell_t *sym, *val;
  for (;;) {
    if (params == s->nil && args == s->nil) {
      return env;
    }
    else if (params == s->nil && args != s->nil) {
      fprintf(stderr, "error: too many arguments\n");
      exit(1);
    }
    else if (args == s->nil && params != s->nil) {
      fprintf(stderr, "arity error: too few arguments\n");
      exit(1);
    }
    else {
      sym = surd_car(s, params);
      val = surd_car(s, args);
      env = _env_insert(s, env, sym, val);
      params = surd_cdr(s, params);
      args = surd_cdr(s, args);
    }
  }
  return env;
}

static cell_t *
_eval_list(surd_t *s, cell_t *list, cell_t *env)
{
  cell_t *first, *next, *next_next, *tmp, *evaled;
  if (list == s->nil) {
    return s->nil;
  }
  else {
    first = surd_cons(s, surd_eval(s, CAR(list), env, 0),  s->nil);
  }

  next = first;
  tmp = CDR(list);

  while (tmp != s->nil) {
    evaled = surd_eval(s, CAR(tmp), env, 0);
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
  cell_t *condition = surd_car(s, surd_cdr(s, exp));
  cell_t *consequent = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  cell_t *alternate = surd_car(s, surd_cdr(s, surd_cdr(s, surd_cdr(s, exp))));
  cell_t *val = surd_eval(s, condition, env, 0);
  if (val == s->nil) {
    if (alternate == s->nil) {
      return s->nil;
    }
    return surd_eval(s, alternate, env, 0);
  }
  else {
    if (consequent == s->nil) {
      return s->nil;
    }
    return surd_eval(s, consequent, env, 0);
  }
}

static cell_t *
_eval_def(surd_t *s, cell_t *exp, cell_t *env)
{
  cell_t *symbol = surd_car(s, surd_cdr(s, exp));
  cell_t *value = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  cell_t *evaled;

  // TODO: check arity!
  if (ISSYM(symbol)) {
    evaled = surd_eval(s, value, env, 0);
    // TODO: should probably store boxes so we can safely replace...
    s->top_env = _env_insert(s, s->top_env, symbol, evaled);
  } 
  else {
    fprintf(stderr, "error: def expected symbol as second argument\n");
  }

  return s->nil;
}


void 
surd_init(surd_t *s, int hs, int ss)
{
  int i;
  s->heap = malloc(sizeof(*s->heap) * hs);
  s->heap_size = hs;
  s->free_list_cells = hs - 1; // we take nil off the top

  s->symbol_table = malloc(sizeof(*s->symbol_table) * ss);

  s->symbol_table_index = 0;
  s->symbol_table_size = ss;
  s->nil = s->heap;

  memset(s->heap, 0, sizeof(s->heap) * hs);
  memset(s->symbol_table, 0, sizeof(*s->symbol_table) * ss);

  memset(s->nil, 0, sizeof(s->nil));
  s->nil->flags = TNIL;
  s->nil->_value.num = 0;

  // incr heap pointer since nil is right before it.
  s->heap += 1;
  s->env = s->nil;
  s->top_env = s->nil;

  // link the heap into a free_list so we don't have to linear search
  s->free_list = NULL;

  // intern some key symbols
  for (i = 0; i < _PRE_INTERNED_SYMBOLS_SIZE; i++) {
    surd_intern(s, _symbols_to_intern[i]);
  }

  surd_install_primitive(s, "cons", surd_p_cons, 2);
  surd_install_primitive(s, "first", surd_p_first, 1);
  surd_install_primitive(s, "rest", surd_p_rest, 1);

  surd_install_primitive(s, "cons?", surd_p_consp, 1);
  surd_install_primitive(s, "fixnum?", surd_p_fixnump, 1);
  surd_install_primitive(s, "symbol?", surd_p_symbolp, 1);
  surd_install_primitive(s, "nil?", surd_p_nilp, 1);
  surd_install_primitive(s, "procedure?", surd_p_procedurep, 1);
  surd_install_primitive(s, "closure?", surd_p_closurep, 1);

  surd_install_primitive(s, "+", surd_p_plus, -1);
  surd_install_primitive(s, "-", surd_p_minus, -1);
  surd_install_primitive(s, "*", surd_p_mult, -1);
  surd_install_primitive(s, "/", surd_p_div, -1);
  surd_install_primitive(s, "%", surd_p_mod, -1);
}

void
surd_destroy(surd_t *s)
{
  int i;

  // kills the heap
  // NIL is the the pointer to the originally allocated space.
  if (s->nil) {
    free(s->nil);
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
  s->nil = NULL;
}


cell_t *
surd_new_cell(surd_t *s)
{
  int tries = 1;
  cell_t *next;

 search:
  if (s->free_list != s->nil && s->free_list_cells > 0) {
    // printf("Free list is: %p, nil is: %p\n", s->free_list, s->nil);

    // zero out to avoid leaking free list pointers
    next = s->free_list;
    s->free_list = s->free_list->_value.cons.cdr;

    memset(next, 0, sizeof(*next));
    // printf(" - returning %p\n", next);
    // printf(" - free list is now %p\n", s->free_list);
    s->free_list_cells--;
    return next;
  }
  tries--;

  // try gc. if it frees up at least one cell, we're good.
  fprintf(stderr, "gc...\n");
  if (surd_gc(s)) {
    goto search;
  }

  return s->nil;
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
    if (c != s->nil) {
      c->flags = TSYMBOL;
      c->_value.num = i;
      s->symbol_table[i].name = strndup(str, slen);
      s->symbol_table[i].symbol = c;
      s->symbol_table_index++;
    }
    else {
      fprintf(stderr, "error: out of memory in intern()\n");
      exit(1);
    }
  }
  else {
    newsize = sizeof(*s->symbol_table) * s->symbol_table_size * 2;
    s->symbol_table = realloc(s->symbol_table, newsize);
    if (s->symbol_table) {
      c = surd_new_cell(s);
      if (c != s->nil) {
        c->flags = TSYMBOL;
        c->_value.num = i;
        s->symbol_table[i].name = strndup(str, slen);
        s->symbol_table[i].symbol = c;
        s->symbol_table_index++;
      }
      else {
        fprintf(stderr, "error: out of memory in intern()\n");
        exit(1);
      }
    }
    else {
      fprintf(stderr, "error: out of memory in intern()\n");
      exit(1);
    }
  }
  return c;
}

void
surd_install_primitive(surd_t *s, char *name, 
                       cell_t *(*func)(surd_t *, cell_t *), int arity)
{
  cell_t *sym = surd_intern(s, name);
  cell_t *prim = surd_new_cell(s);
  if (prim != s->nil) {
    prim->flags = TPRIMITIVE;
    prim->_value.primitive.arity = arity;
    prim->_value.primitive.func = func;
    s->top_env = _env_insert(s, s->top_env, sym, prim);
  }
  else {
    fprintf(stderr, "error: out of memory in surd_install_primitive\n");
    exit(1);
  }
}

cell_t *
surd_cons(surd_t *s, cell_t *car, cell_t *cdr)
{
  cell_t *new = surd_new_cell(s);
  if (new != s->nil) {
    new->flags = TCONS;
    new->_value.cons.car = car;
    new->_value.cons.cdr = cdr;
    return new;
  }
  else {
    fprintf(stderr, "error: out of memory in surd_cons\n");
    exit(1);
  }
}

int
surd_list_length(surd_t *s, cell_t *c)
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
  if (fix != s->nil) {
    surd_num_init(s, fix, i * sign);
    return fix;
  }

  fprintf(stderr, "error: Couldn't read fixnum\n");
  exit(1);
}


static void
_eat_comment(FILE *in)
{
  int c;
  do {
    c = fgetc(in);
  } while(c != '\r' && c != '\n' && c != EOF);
}

static cell_t *
_read_symbol(surd_t *s, FILE *in)
{
 
  cell_t *sym;
  char buffer[128];
  int c, i = 0;

  c = fgetc(in);

  if (_is_initial(c)) {
    buffer[i++] = (char)c;
    buffer[i] = 0;
  }
  else {
    fprintf(stderr, "error: not a symbol: invalid first char '%c'\n", (char)c);
    exit(1);
  }

  for (;;) {
    c = fgetc(in);
    if (_is_delim(c)) {
      ungetc(c, in);
      goto done;
    }
    else if (_is_sym_char(c)) {
      buffer[i++] = (char)c;
      buffer[i] = 0;
    }
    else {
      fprintf(stderr, "error: '%c' not a symbol character\n", (char)c);
      exit(1);
    }
  }
 done:
  sym = surd_intern(s, buffer);
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
    next_next = surd_cons(s, tmp, s->nil);
    next->_value.cons.cdr = next_next;
    next = next_next;
  }

 done:
  c = fgetc(in);
  if (c != ')') {
    fprintf(stderr, "error: expected ')', but found '%c'\n", c);
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
      ungetc(l, in);
      if (isdigit(l)) {
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
      // might not be a list.
      if (ISCONS(tmp)) {
        surd_display(s, out, CAR(tmp));
        tmp = CDR(tmp);
      }
      else {
        surd_display(s, out, tmp);
        break;
      }
      sep = 1;
    }
    fprintf(out, ")");
  }
  else if (ISCLOSURE(exp)) {
    fprintf(out, "<#closure: %p>", exp);
  }
  else if (ISPRIM(exp)) {
    fprintf(out, "<#primitive: %p>", exp);
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
surd_eval(surd_t *s, cell_t *exp, cell_t *env, int top)
{
  cell_t *car, *tmp;

  //  surd_display(s, stdout, exp);
  if (ISFIXNUM(exp) || ISCLOSURE(exp) || ISPRIM(exp) || exp == s->nil) {
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
        fprintf(stderr, "error: attempted to take the car of nil\n");
        // dont' blow up, just return nil
        return s->nil;
      }
    }
    else if (car == surd_intern(s, "if")) {
      return _eval_if(s, exp, env);
    }
    else if (car == surd_intern(s, "lambda")) {
      if (surd_list_length(s, exp) > 2) {
        return surd_make_closure(s, exp, env);
      }
      else {
        fprintf(stderr, "error: lambda requires at least 2 arguments\n");
      }
    }
    else if (car == surd_intern(s, "def")) {
      if (top) {
        return _eval_def(s, exp, env);
      } else {
        fprintf(stderr, "error: def cannot be called from non-toplevel "
                "expression\n");
        exit(1);
      }
    }
    else {
      // apply
      tmp = surd_eval(s, car, env, 0);
      if (ISPRIM(tmp) || ISCLOSURE(tmp)) {
        return surd_apply(s, tmp, _eval_list(s, CDR(exp), env));
      }
      else {
        fprintf(stderr, "error:attempt to apply that which is not applyable\n");
        exit(1);
      }
    }
  }

  return s->nil;
}

cell_t *
surd_apply(surd_t *s, cell_t *closure, cell_t *args)
{
  cell_t *nenv, *code;
  int carity;
  if (closure == NULL && closure == s->nil) {
    fprintf(stderr, "error: attempt to apply a null value\n");
    exit(1);
  }
  if (ISPRIM(closure)) {
    carity = surd_list_length(s, args);
    if (carity == closure->_value.primitive.arity ||
        closure->_value.primitive.arity == -1) {
      return (closure->_value.primitive.func)(s, args);
    }
    else {
      fprintf(stderr, "arity error: arity mismatch, "
              "expected %d args, got %d\n", closure->_value.primitive.arity,
              carity);
      exit(1);
    }
  }
  else if (ISCLOSURE(closure)) {
    code = closure->_value.cons.car;
    nenv = closure->_value.cons.cdr;
    nenv = _env_extend(s, nenv, surd_car(s, surd_cdr(s, code)), args);
    return surd_eval(s, surd_car(s, surd_cdr(s, surd_cdr(s, code))), nenv, 0);
  }
  return s->nil;
}

/**
   One pass procecedures
 */

static int
marked(cell_t *c)
{
  return c->flags & (1 << MARK_BIT);
}

static int
type(cell_t *c)
{
  return c->flags & ((1 << TYPE_BITS+1) - 1)
}

static cell_t *
address(cell_t *c)
{

}

static void
possibly_mark(cell_t *c)
{
  if (!(type(c) & TATOMIC)) {
    mark(c);
  }
}

static void
mark(cell_t *c)
{
  cell->flags |= cell->flags | 1<<MARK_BIT;
}

static void
unmark(cell_t *c)
{
  if (marked(c)) {
    cell->flags ^= 1<<MARK_BIT;
  }
}


int
surd_gc(surd_t *s)
{
  return 0;
}
