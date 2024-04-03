#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gc.h>
#include "surd.h"

#define _PRE_INTERNED_SYMBOLS_SIZE 4
static const char *_symbols_to_intern[] = {
  "quote", "if", "lam", "def"
};

enum SURD_PRIMITIVES {
  PRIM_CONS,
  PRIM_FIRST,
  PRIM_REST,
  PRIM_NTH,
  PRIM_CONSP,
  PRIM_NILP,
  PRIM_EOFP,
  PRIM_FIXNUMP,
  PRIM_SYMBOLP,
  PRIM_PROCEDUREP,
  PRIM_CLOSUREP,
  PRIM_FOREIGNP,
  PRIM_PLUS,
  PRIM_MINUS,
  PRIM_MULT,
  PRIM_DIV,
  PRIM_MOD,
  PRIM_LT,
  PRIM_EQ,
  PRIM_READ,
  PRIM_WRITE,
  PRIM_OPEN,
  PRIM_GETBYTE,
  PRIM_PUTBYTE
};

cell_t *
surd_car(surd_t *s, cell_t *c)
{
  if (ISCONS(c)) { return CAR(c); }
  /* TODO: error here! */
  return s->nil;
}

cell_t *
surd_cdr(surd_t *s, cell_t *c)
{
  if (ISCONS(c)) { return CDR(c); }
  /* TODO: error here! */
  return s->nil;
}

static int
_symbol_position(surd_t *s, const char *sym)
{
  for (int i = 0; i < s->symbol_table_index; i++) {
    if (strcmp(s->symbol_table[i].name, sym) == 0) {
      return i;
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
  c = surd_cons(s, sym, value);
  result = surd_cons(s, c, env);
  return result;
}

static cell_t *
_env_extend(surd_t *s, cell_t *env, cell_t *params, cell_t *args)
{
  cell_t *sym, *val, *result;

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
      env = _env_insert(s, env, sym, val);
      params = surd_cdr(s, params);
      args = surd_cdr(s, args);
    }
  }

  return result;
}

static cell_t *
_eval_list(surd_t *s, cell_t *list, cell_t *env)
{
  cell_t *first, *next=NULL, *next_next, *tmp, *evaled=NULL;
  if (list == s->nil) {
    return s->nil;
  }

  tmp = surd_eval(s, CAR(list), env, 0);
  first = surd_cons(s, tmp,  s->nil);

  next = first;
  tmp = CDR(list);

  while (tmp != s->nil && tmp) {
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
  cell_t *result, *condition, *consequent, *alternate, *val;

  condition = surd_car(s, surd_cdr(s, exp));
  consequent = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  alternate = surd_car(s, surd_cdr(s, surd_cdr(s, surd_cdr(s, exp))));
  val = surd_eval(s, condition, env, 0);

  if (val == s->nil) {
    result = alternate == s->nil ? s->nil: alternate;
  }
  else {
    result = consequent;
  }

  return result;

}

static cell_t *
_eval_def(surd_t *s, cell_t *exp, cell_t *env)
{
  cell_t *symbol = surd_car(s, surd_cdr(s, exp));
  cell_t *value = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
  cell_t *evaled = s->nil;

  // TODO: check arity!
  if (ISSYM(symbol)) {
    evaled = surd_eval(s, value, env, 0);
    // TODO: should probably store boxes so we can safely replace...
    s->top_env = _env_insert(s, s->top_env, symbol, evaled);
  }
  else {
    fprintf(stderr, "error: def expected symbol as second argument\n");
    exit(1);
  }

  return evaled;
}


void
surd_init(surd_t *s, int hs, int ss)
{
  int i;
  s->symbol_table = GC_malloc(sizeof(*s->symbol_table) * ss);

  s->symbol_table_index = 0;
  s->symbol_table_size = ss;
  s->nil = (cell_t *)0;
  s->eof = (cell_t *)1;

  memset(s->symbol_table, 0, sizeof(*s->symbol_table) * ss);

  s->env = s->nil;
  s->top_env = s->nil;

  // intern some key symbols
  for (i = 0; i < _PRE_INTERNED_SYMBOLS_SIZE; i++) {
    surd_intern(s, _symbols_to_intern[i]);
  }

  s->t = surd_internn(s, "true", 4);

  cell_t *sym, *prim;

#define INSTALL_PRIMITIVE(NAME, NUM, ARITY) \
  sym = surd_intern(s, NAME); \
  prim = surd_new_cell(s); \
  if (prim != s->nil) { \
    prim->flags = TPRIMITIVE; \
    prim->_value.primitive.arity = ARITY; \
    prim->_value.primitive.num = NUM; \
    s->top_env = _env_insert(s, s->top_env, sym, prim); \
  } \
  else { \
    fprintf(stderr, "error: out of memory installing primitive\n"); \
    exit(1);                                                       \
  }

  INSTALL_PRIMITIVE("cons", PRIM_CONS, 2);
  INSTALL_PRIMITIVE("first", PRIM_FIRST, 1);
  INSTALL_PRIMITIVE("rest", PRIM_REST, 1);
  INSTALL_PRIMITIVE("nth", PRIM_NTH, 2);

  INSTALL_PRIMITIVE("cons?", PRIM_CONSP, 1);
  INSTALL_PRIMITIVE("nil?", PRIM_NILP, 1);
  INSTALL_PRIMITIVE("eof?", PRIM_EOFP, 1);
  INSTALL_PRIMITIVE("fixnum?", PRIM_FIXNUMP, 1);
  INSTALL_PRIMITIVE("symbol?", PRIM_SYMBOLP, 1);
  INSTALL_PRIMITIVE("procedure?", PRIM_PROCEDUREP, 1);
  INSTALL_PRIMITIVE("closure?", PRIM_CLOSUREP, 1);
  INSTALL_PRIMITIVE("foreign?", PRIM_FOREIGNP, 1);

  INSTALL_PRIMITIVE("+", PRIM_PLUS, 2);
  INSTALL_PRIMITIVE("-", PRIM_MINUS, 2);
  INSTALL_PRIMITIVE("*", PRIM_MULT, 2);
  INSTALL_PRIMITIVE("/", PRIM_DIV, 2);
  INSTALL_PRIMITIVE("%", PRIM_MOD, 2);

  INSTALL_PRIMITIVE("<", PRIM_LT, 2);
  INSTALL_PRIMITIVE("=", PRIM_EQ, 2);

  INSTALL_PRIMITIVE("read", PRIM_READ, 1);
  INSTALL_PRIMITIVE("write", PRIM_WRITE, 2);
  INSTALL_PRIMITIVE("open", PRIM_OPEN, 2);
  INSTALL_PRIMITIVE("get-byte", PRIM_GETBYTE, 1);
  INSTALL_PRIMITIVE("put-byte", PRIM_PUTBYTE, 2);

#undef INSTALL_PRIMITIVE
}

cell_t *
surd_new_cell(surd_t *s)
{
  return GC_malloc(sizeof(cell_t));
}

void
surd_destroy(surd_t *s)
{
  /* XXX: Who cares about real memory management? :) */
  if (s->symbol_table) {
    s->symbol_table = NULL;
    s->symbol_table_size = 0;
    s->symbol_table_index = 0;
  }

  s->env = NULL;
}

void
surd_num_init(surd_t *s, cell_t *c, int value)
{
  c->flags = TFIXNUM;
  c->_value.num = value;
}

cell_t *
surd_intern(surd_t *s, const char *str)
{
  return surd_internn(s, str, strlen(str));
}

cell_t *
surd_internn(surd_t *s, const char *str, size_t n)
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
surd_install_foreign(surd_t *s, const char *name,
                     cell_t *(*func)(surd_t *, cell_t *), int arity)
{
  cell_t *sym = surd_intern(s, name);
  cell_t *prim = surd_new_cell(s);
  if (prim != s->nil) {
    prim->flags = TFOREIGN;
    prim->_value.foreign.arity = arity;
    prim->_value.foreign.cfunc = func;
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

#define READ_WHITESPACE " \t\n\r"
#define READ_DELIMS " \t\r\n()[]{};#"

static int
eatwhile(FILE *in, const char *skip)
{
  int ch;
  do {
    ch = fgetc(in);
  } while (ch && strchr(skip, ch) && ch != EOF);
  return ch;
}

static int
skipuntil(FILE *in, const char *stop)
{
  int ch;
  do {
    ch = fgetc(in);
  } while (ch && !strchr(stop, ch) && ch != EOF);
  return ch;
}

static cell_t *
trynumber(surd_t *s, const char *buf, int bufi)
{
  char *endres;
  long int intres;
  double flores;
  const char *end = buf+bufi;

  /* try int first. */
  intres = strtol(buf, &endres, 0);
  if (end == endres && *end == '\0') {
    /* int was successful. Does it overflow? */
    if ((errno == ERANGE && (intres == LONG_MAX || intres == LONG_MIN))) {

      if (intres > (LONG_MAX >> 1)) {
        fprintf(stderr, "integer overflow\n");
        exit(1);
      }
      if (intres < (LONG_MIN >> 1)) {
        fprintf(stderr, "integer underflow\n");
        exit(1);
      }
      if (errno != 0 && intres == 0) {
        fprintf(stderr, "unknown integer error\n");
        exit(1);
      }
    }
    cell_t *fix = surd_new_cell(s);
    if (fix != s->nil) {
      surd_num_init(s, fix, intres);
      return fix;
    }
  }
  /* Fall through, it's not an int */

  flores = strtod(buf, &endres);
  if (end == endres && *end == '\0') {
    /* flores == 0 iff flores is not greater than or equal to 0
     * -Wfloat-equal will complain if we don't do it this way.
     */
    if (errno == ERANGE) {
      if (!(flores > 0 || flores < 0)) {
        fprintf(stderr, "double underflow\n");
        exit(1);
      }
      else {
        fprintf(stderr, "double overflow\n");
        exit(1);
      }
    }
    else {
      fprintf(stderr, "floats are not implemented yet.\n");
      exit(1);
    }
  }
  /* Fall through, it's not a float, either */

  ptrdiff_t n = end - buf;
  cell_t *sym = surd_internn(s, buf, n);
  return sym;
}

static cell_t *readlist_(surd_t *s, FILE *in);

static cell_t *
read_(surd_t *s, FILE *in)
{

  for (;;) {
    int c = eatwhile(in, READ_WHITESPACE);
    switch (c) {
    case EOF:
      return NULL;
    case ';':
      c = skipuntil(in, "\n");
      ungetc(c, in);
      continue;
    case ')':
      fprintf(stderr, "read closing brace without open");
      exit(1);
    case '\'': {// quote
      cell_t *sym = surd_internn(s, "quote", 5);
      cell_t *tmp = surd_read(s, in);
      cell_t *tmp2 = surd_cons(s, tmp, s->nil);
      return surd_cons(s, sym, tmp2);
    }
    case '(':
      return readlist_(s, in);
    default: {
#define SYMBUF_LEN 256
      char buf[SYMBUF_LEN];
      int bufi = 0;
      do {
        if (bufi == SYMBUF_LEN) {
          fprintf(stderr, "symbol too long\n");
          exit(1);
        }
        buf[bufi++] = c;
        c = fgetc(in);
      } while (c && c != EOF && !strchr(READ_DELIMS, c));
      buf[bufi] = '\0';
      ungetc(c, in);
      if (bufi == 1 && strchr("-+", buf[0])) { return surd_internn(s, buf, 1); }
      return trynumber(s, buf, bufi);
    }
    } /* end switch */
  }
  return NULL;

#undef SYMBUF_LEN
}

static cell_t *
readlist_(surd_t *s, FILE *in)
{
  int c = eatwhile(in, READ_WHITESPACE);
  if (c == ')') { return s->nil; }
  if (c == EOF) { return s->eof; }
  ungetc(c, in);
  cell_t *obj = read_(s, in);
  cell_t *list = readlist_(s, in);
  return surd_cons(s, obj, list);
}

#undef READ_DELIMS
#undef READ_WHITESPACE


cell_t *
surd_read(surd_t *s, FILE *in)
{
  cell_t *tmp;
  tmp = read_(s, in);
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
    fprintf(out, "<#closure: %p>", (void *)exp);
  }
  else if (ISPRIM(exp)) {
    fprintf(out, "<#primitive: %p>", (void *)exp);
  }
  else {
    fprintf(out, "umm...");
  }
}

void
surd_write(surd_t *s, FILE *out, cell_t *exp)
{
  /* XXX: not correct, but we'll take it for now. */
  surd_display(s, out, exp);
}

cell_t *
surd_eval(surd_t *s, cell_t *exp, cell_t *env, int top)
{
  for (;;) {
    if (ISFIXNUM(exp) || ISCLOSURE(exp) || ISPRIM(exp) || exp == s->nil) {
      return exp;
    }
    else if (ISSYM(exp)) {
      return _env_lookup(s, env, exp);
    }
    else if (ISCONS(exp)) {
      cell_t *car = CAR(exp);
      if (car == surd_intern(s, "quote")) {
        cell_t *tmp = CDR(exp);
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
        exp = _eval_if(s, exp, env);
      }
      else if (car == surd_intern(s, "lam")) {
        if (surd_list_length(s, exp) > 2) {
          return surd_make_closure(s, exp, env);
        }
        else {
          fprintf(stderr, "error: lam requires at least 2 arguments\n");
          exit(1);
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
        cell_t *tmp = surd_eval(s, car, env, 0);
        // TODO: the closure path can be optimized into the loop
        if (ISPRIM(tmp) || ISCLOSURE(tmp)) {
          cell_t *tmp2 = surd_apply(s, tmp, _eval_list(s, CDR(exp), env));
          return tmp2;
        }
        else {
          fprintf(stderr, "error:attempt to apply that which is not applyable\n");
          exit(1);
        }
      }
    }
  }
  return s->nil;
}

cell_t *
surd_apply(surd_t *s, cell_t *closure, cell_t *args)
{
  if (closure == NULL && closure == s->nil) {
    fprintf(stderr, "error: attempt to apply a null value\n");
    exit(1);
  }
  if (ISPRIM(closure)) {
    int carity = surd_list_length(s, args);
    if (carity == closure->_value.primitive.arity ||
        closure->_value.primitive.arity == -1) {

      switch (closure->_value.primitive.num) {
      case PRIM_CONS: {
        cell_t *c = surd_cons(s, CAR(args),
                              CAR(CDR(args)));
        if (c == s->nil) {
          fprintf(stderr, "error: out of memory in primitive cons\n");
          exit(1);
        }
        return c;
      }
      case PRIM_FIRST: {
        cell_t *arg1 =CAR(args);
        if (ISCONS(arg1)) {
          return CAR(arg1);
        }
        else {
          fprintf(stderr, "error: cons required for primitve first\n");
          exit(1);
        }
      }
      case PRIM_REST: {
        cell_t *arg1 =CAR(args);
        if (ISCONS(arg1)) {
          return CDR(arg1);
        }
        else {
          fprintf(stderr, "error: cons required for primitve rest\n");
          exit(1);
        }
      }
      case PRIM_NTH: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISCONS(arg2) && ISFIXNUM(arg1)) {
          cell_t *current = arg2;
          for (int i = arg1->_value.num; i > 0; i--) {
            if (current == s->nil) {
              fprintf(stderr, "error: nth ran out of cells\n");
              exit(1);
            }
            current = CDR(current);
          }
          if (current != s->nil) {
            return CAR(current);
          }
          else {
            fprintf(stderr, "error: nth ran out of cells\n");
            exit(1);
          }
        }
        else {
          fprintf(stderr, "error: int and cons required for primitve nth\n");
          exit(1);
        }
      }

      case PRIM_CONSP: {
        cell_t *arg1 = CAR(args);
        if (ISCONS(arg1)) {
          return s->t;
        }
        return s->nil;
      }

      case PRIM_NILP: {
        cell_t *arg1 = CAR(args);
        if (arg1 == s->nil) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_EOFP: {
        cell_t *arg1 = CAR(args);
        if (arg1 == s->eof) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_FIXNUMP: {
        cell_t *arg1 = CAR(args);
        if (arg1 == s->nil) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_SYMBOLP: {
        cell_t *arg1 = CAR(args);
        if (ISSYM(arg1)) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_PROCEDUREP: {
        cell_t *arg1 = CAR(args);
        if (ISPRIM(arg1) || ISCLOSURE(arg1) || ISFOREIGN(arg1)) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_CLOSUREP: {
        cell_t *arg1 = CAR(args);
        if (ISCLOSURE(arg1)) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_FOREIGNP: {
        cell_t *arg1 = CAR(args);
        if (ISFOREIGN(arg1)) {
          return s->t;
        }
        return s->nil;
      }
      case PRIM_PLUS: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, arg1->_value.num + arg2->_value.num);
          return tmp;
        }
        else {
          fprintf(stderr, "attempt to add a non fixnum\n");
          exit(1);
        }
      }
      case PRIM_MINUS: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, arg1->_value.num - arg2->_value.num);
          return tmp;
        }
        fprintf(stderr, "attempt to subtract a non fixnum\n");
        exit(1);
      }
      case PRIM_MULT: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, arg1->_value.num * arg2->_value.num);
          return tmp;
        }
        fprintf(stderr, "attempt to multiply a non fixnum\n");
        exit(1);
      }
      case PRIM_DIV: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          if (arg2->_value.num == 0) {
            fprintf(stderr, "attempt to divide by 0\n");
            exit(1);
          }
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, arg1->_value.num / arg2->_value.num);
          return tmp;
        }
        fprintf(stderr, "attempt to divide a non fixnum\n");
        exit(1);
      }
      case PRIM_MOD: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          if (arg2->_value.num == 0) {
            fprintf(stderr, "attempt to divide by 0\n");
            exit(1);
          }
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, arg1->_value.num % arg2->_value.num);
          return tmp;
        }
        fprintf(stderr, "attempt to mod a non fixnum\n");
        exit(1);
      }
      case PRIM_LT: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          if (arg1->_value.num < arg2->_value.num) {
            return s->t;
          }
          return s->nil;
        }
        fprintf(stderr, "attempt to compare a non fixnum\n");
        exit(1);
      }
      case PRIM_EQ: {
        cell_t *arg1 = CAR(args);
        cell_t *arg2 = CAR(CDR(args));
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          if (arg1->_value.num == arg2->_value.num) {
            return s->t;
          }
          return s->nil;
        }
        else if (ISSYM(arg2) && ISSYM(arg1)) {
          if (arg1->_value.num == arg2->_value.num) {
            return s->t;
          }
          return s->nil;
        }
        else if (arg1 == arg2) {
          return s->t;
        }
        return s->nil;
      }

      case PRIM_READ: {
        fprintf(stderr, "read not implemented\n");
        exit(1);
      }

      case PRIM_WRITE: {
        fprintf(stderr, "write not implemented\n");
        exit(1);
      }

      case PRIM_OPEN: {
        fprintf(stderr, "open not implemented\n");
        exit(1);
      }

      case PRIM_GETBYTE: {
        fprintf(stderr, "get-byte not implemented\n");
        exit(1);
      }

      case PRIM_PUTBYTE: {
        fprintf(stderr, "put-byte not implemented\n");
        exit(1);
      }

      default:
        fprintf(stderr,"unknown primitive\n");
        exit(1);
      }
    }
    else {
      fprintf(stderr, "arity error: arity mismatch, "
              "expected %d args, got %d\n", closure->_value.primitive.arity,
              carity);
      exit(1);
    }
  }
  else if (ISCLOSURE(closure)) {
    cell_t *code = closure->_value.cons.car;
    cell_t *nenv = closure->_value.cons.cdr;
    nenv = _env_extend(s, nenv, surd_car(s, surd_cdr(s, code)), args);
    cell_t *tmp = surd_eval(s, surd_car(s, surd_cdr(s, surd_cdr(s, code))), nenv, 1);
    return tmp;
  }
  return s->nil;
}

cell_t *
surd_load(surd_t *s, FILE *in)
{
  cell_t *tmp;
  cell_t *last = s->nil;
  while ((tmp = surd_read(s, in)) != NULL) {
    last = surd_eval(s, tmp, s->env, 1);
  }
  return last;
}
