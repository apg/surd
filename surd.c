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
  for (i = 0; i < s->symbol_table_index; i++) {
    if (s->symbol_table[i].name) {
      if (strncmp(s->symbol_table[i].name, sym, strlen(s->symbol_table[i].name)) == 0) {
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
  }

  fprintf(stderr, "error: symbol %s not found\n", s->symbol_table[sym->_value.num].name);
  fflush(stderr);
  exit(1);
}

static cell_t *
_env_insert(surd_t *s, cell_t *env, cell_t *sym, cell_t *value)
{
  cell_t *c, *result;
  surd_add_root(s, env);
  surd_add_root(s, value);
  surd_add_root(s, sym);

  c = surd_cons(s, sym, value);
  surd_add_root(s, c);

  result = surd_cons(s, c, env);

  surd_rm_root(s, c);
  surd_rm_root(s, sym);
  surd_rm_root(s, value);
  surd_rm_root(s, env);
  
  return result;
}

static cell_t *
_env_extend(surd_t *s, cell_t *env, cell_t *params, cell_t *args)
{
  cell_t *sym, *val, *tmp, *result;

  /*fprintf(stderr, "~=~=~=~=~=~=~=~=~=~=~=~=~=\n");
  fprintf(stderr, "params: ");
  surd_display(s, stderr, params);
  fprintf(stderr, "\nargs: ");
  surd_display(s, stderr, args);
  fprintf(stderr, "\n");*/

  surd_add_root(s, env);
  surd_add_root(s, params);
  surd_add_root(s, args);

  result = env;

  for (;;) {
    if (params == s->nil && args == s->nil) {
      result = env;
      break;
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
      tmp = env;
      env = _env_insert(s, env, sym, val);
      surd_rm_root(s, tmp);
      surd_add_root(s, env);
      params = surd_cdr(s, params);
      args = surd_cdr(s, args);
    }
  }

  surd_rm_root(s, args);
  surd_rm_root(s, params);
  surd_rm_root(s, env);


  return result;
}

static cell_t *
_eval_list(surd_t *s, cell_t *list, cell_t *env)
{
  cell_t *first, *next=NULL, *next_next, *tmp, *evaled=NULL;
  if (list == s->nil) {
    return s->nil;
  }
  surd_add_root(s, list);
  surd_add_root(s, env);

  tmp = surd_eval(s, CAR(list), env, 0);
  first = surd_cons(s, tmp,  s->nil);

  next = first;
  tmp = CDR(list);

  while (tmp != s->nil && tmp) {
    if (evaled) {
      surd_rm_root(s, evaled);
    }

    evaled = surd_eval(s, CAR(tmp), env, 0);
    surd_add_root(s, evaled);
    next_next = surd_cons(s, evaled, s->nil);
    next->_value.cons.cdr = next_next;
    surd_rm_root(s, next);
    next = next_next;
    surd_add_root(s, next);
    tmp = CDR(tmp);
  }

  if (!tmp) {
    fprintf(stderr, "whoops: tmp was NULL in eval_list\n");
  }

  surd_rm_root(s, evaled);
  surd_rm_root(s, next);
  surd_rm_root(s, env);
  surd_rm_root(s, list);

  return first;
}

static cell_t *
_eval_if(surd_t *s, cell_t *exp, cell_t *env)
{
  cell_t *result, *condition, *consequent, *alternate, *val;

  surd_add_root(s, exp);
  surd_add_root(s, env);

  condition = surd_car(s, surd_cdr(s, exp));
  consequent = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  alternate = surd_car(s, surd_cdr(s, surd_cdr(s, surd_cdr(s, exp))));
  val = surd_eval(s, condition, env, 0);
  surd_add_root(s, val);

  if (val == s->nil) {
    if (alternate == s->nil) {
      result = s->nil;
    }
    else {
      result = surd_eval(s, alternate, env, 0);
    }
  }
  else {
    if (consequent == s->nil) {
      result = s->nil;
    } 
    else {
      result = surd_eval(s, consequent, env, 0);
    }
  }

  surd_rm_root(s, val);
  surd_rm_root(s, exp);
  surd_rm_root(s, env);
  return result;

}

static cell_t *
_eval_def(surd_t *s, cell_t *exp, cell_t *env)
{
  cell_t *symbol = surd_car(s, surd_cdr(s, exp));
  cell_t *value = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  cell_t *evaled;

  surd_add_root(s, exp);
  surd_add_root(s, env);

  // TODO: check arity!
  if (ISSYM(symbol)) {
    evaled = surd_eval(s, value, env, 0);
    surd_add_root(s, evaled);
    // TODO: should probably store boxes so we can safely replace...
    s->top_env = _env_insert(s, s->top_env, symbol, evaled);
    surd_rm_root(s, evaled);
  } 
  else {
    fprintf(stderr, "error: def expected symbol as second argument\n");
  }

  surd_rm_root(s, env);
  surd_rm_root(s, exp);

  return s->nil;
}


void 
surd_init(surd_t *s, int hs, int ss)
{
  int i;
  s->heap = malloc(sizeof(*s->heap) * hs);
  s->heap_size = hs;
  s->heap_ceil = s->heap + (hs + 1);
  s->free_list_cells = 0;

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
  s->bump = s->heap;

  s->env = s->nil;
  s->top_env = s->nil;

  s->roots = NULL;
  s->roots_index = 0;
  s->roots_size = 0;

  // link the heap into a free_list so we don't have to linear search
  s->free_list = s->nil;
  s->first_alloc = NULL;

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

  surd_install_primitive(s, ">", surd_p_gt, 2);
  surd_install_primitive(s, "<", surd_p_lt, 2);
  surd_install_primitive(s, "=", surd_p_eq, 2);
  surd_install_primitive(s, ">=", surd_p_ge, 2);
  surd_install_primitive(s, "<=", surd_p_le, 2);

  surd_install_primitive(s, "read", surd_p_read, 0);
  surd_install_primitive(s, "write", surd_p_write, -1);
  surd_install_primitive(s, "print-symbols", surd_p_symbols, -1);
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
  /* need to reclaim these--this is a memleak */
  s->env = NULL;
  s->nil = NULL;
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
  if (c == s->nil) {
    return 0;
  }
  else if (ISCONS(c)) {
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

cell_t *
surd_make_box(surd_t *s, cell_t *value)
{
  cell_t *box;
  box = surd_cons(s, value, s->nil);
  box->flags = TBOX;
  return box;
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
    MARK(fix);
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
    fprintf(stderr, "error: not a symbol: invalid first char(%d) '%c' \n", c, (char)c);
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


  /* TODO: left off here! */
  first = surd_cons(s, tmp, s->nil);
  surd_add_root(s, first);
  next = first;

  for (;;) {
    tmp = _read(s, in);
    surd_add_root(s, tmp);
    if (!tmp) { // got an end delimiter, so we're done
      goto done;
    }
    next_next = surd_cons(s, tmp, s->nil);
    next->_value.cons.cdr = next_next;
    next = next_next;
    surd_rm_root(s, tmp);
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
  cell_t *sym, *tmp, *tmp2, *ret;
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
      surd_add_root(s, sym);
      tmp = surd_read(s, in);
      surd_add_root(s, tmp);
      tmp2 = surd_cons(s, tmp, s->nil);
      surd_add_root(s, tmp2);
      ret = surd_cons(s, sym, tmp2);
      surd_rm_root(s, tmp2);
      surd_rm_root(s, tmp);
      surd_rm_root(s, sym);
      return ret;

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
      if (c == EOF) {
        return NULL;
      }
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
  cell_t *car, *tmp, *tmp2;

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
      surd_add_root(s, car);
      surd_add_root(s, env);
      tmp = surd_eval(s, car, env, 0);
      surd_add_root(s, tmp);

      if (ISPRIM(tmp) || ISCLOSURE(tmp)) {
        /*        fprintf(stderr, "Env being used to apply: \t");
                  surd_display(s, stderr, env);
                  fprintf(stderr, "\n");*/
        tmp2 = surd_apply(s, tmp, _eval_list(s, CDR(exp), env));
        surd_rm_root(s, tmp);
        surd_rm_root(s, env);
        surd_rm_root(s, car);
        return tmp2;
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
  cell_t *nenv, *code, *tmp;
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
    surd_add_root(s, nenv);
    tmp = surd_eval(s, surd_car(s, surd_cdr(s, surd_cdr(s, code))), nenv, 0);
    surd_rm_root(s, nenv);
    return tmp;
  }
  return s->nil;
}
