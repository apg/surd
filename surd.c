#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gc.h>
#include "surd.h"

typedef enum {
  TNIL=0x0,
  TFIXNUM=0x1,
  TSYMBOL=0x2,
  TCONS=0x4,
  TSTRING=0x8,
  TCLOSURE=0x10, // env goes in cdr, code in car
  TPRIMITIVE=0x20,
  TFOREIGN=0x40,
  TMACRO=0x80,
  TERROR=0x100,
} type_t;

#define TYPE_BITS 16
#define TATOMIC (TFIXNUM | TSYMBOL | TNIL)
#define TYPE(c) (c->flags & ((1 << (TYPE_BITS+1)) - 1))

struct cell {
  unsigned int flags;
  union {
    int num;
    struct cons {
      cell_t *car;
      cell_t *cdr;
    } cons;
    struct str {
      size_t length;
      char *buffer;
    } str;
    struct primitive {
      int arity;
      int num;
    } primitive;
    struct foreign {
      int arity;
      cell_t *(*cfunc)(surd_t *, cell_t *args);
    } foreign;
  } _value;
};

struct internpool {
  char *buffer;
  size_t buffer_capacity;
  size_t buffer_length;
  size_t *offsets;
  size_t *lengths;
  size_t offset_capacity;
  size_t count;
};

static void
internpool_init(struct internpool *pool)
{
  pool->buffer = NULL;
  pool->buffer_capacity = 0;
  pool->buffer_length = 0;
  pool->offsets = NULL;
  pool->lengths = NULL;
  pool->offset_capacity = 0;
  pool->count = 0;
}

static size_t
internpool_intern(struct internpool *pool, const char *str)
{
#define INITIAL_BUFFER_CAPACITY 2048
#define INITIAL_OFFSET_CAPACITY 256
  if (!str) return -1;
  size_t len = strlen(str);

  /** at some point, we could binary search on this and speed things up.
   */
  for (size_t i = 0; i < pool->count; i++) {
    if (pool->lengths[i] == len) {
      const char *s = pool->buffer + pool->offsets[i];
      if (strcmp(s, str) == 0) {
        return i;
      }
    }
  }

  len++; /* add 1 for the nul, to store */

  if (pool->buffer_length + len > pool->buffer_capacity) {
    size_t new_cap = pool->buffer_capacity ?
      pool->buffer_capacity * 2: INITIAL_BUFFER_CAPACITY;
    while (pool->buffer_capacity + len > new_cap) {
      new_cap *= 2;
    }
    pool->buffer = GC_realloc(pool->buffer, new_cap);
    pool->buffer_capacity = new_cap;
  }

  size_t offset = pool->buffer_length;
  memcpy(pool->buffer + pool->buffer_length, str, len);
  pool->buffer_length += len;

  if (pool->count >= pool->offset_capacity) {
    size_t new_cap = pool->offset_capacity ?
      pool->offset_capacity * 2: INITIAL_OFFSET_CAPACITY;
    pool->offsets = GC_realloc(pool->offsets, new_cap * sizeof(size_t));
    pool->lengths = GC_realloc(pool->lengths, new_cap * sizeof(size_t));
    pool->offset_capacity = new_cap;
  }

  size_t position = pool->count++;
  pool->lengths[position] = len-1; /* don't include the nul */
  pool->offsets[position] = offset;
  return position;
}

static char *
internpool_tostring(struct internpool *pool, size_t offset)
{
  if (offset >= pool->count) {
    fprintf(stderr, "invalid symbol: %zu\n", offset);
    exit(1);
  }

  size_t buffer_offset = pool->offsets[offset];
  return pool->buffer + buffer_offset;
}


struct surd {
  /* Symbol table: symbols use cells as well as external memory
     created on the fly via malloc
  */
  struct internpool *interns;

  /* initial eval environment */
  cell_t *env;

  /* Top level environment */
  cell_t *top_env;
  cell_t *t;
  cell_t *nil;
  cell_t *eof;

  cell_t *IF;
  cell_t *QUOTE;
  cell_t *LAM;
  cell_t *DEF;
  cell_t *TRUE;
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

#define ISNIL(c) (c == (cell_t *)0)
#define ISFIXNUM(c) (c != NULL && !ISNIL(c) && c->flags & TFIXNUM)
#define ISSYM(c) (c != NULL && !ISNIL(c) && c->flags & TSYMBOL)
#define ISSTR(c) (c != NULL && !ISNIL(c) && c->flags & TSTRING)
#define ISCONS(c) (c != NULL && !ISNIL(c) && c->flags & TCONS)
#define ISCLOSURE(c) (c != NULL && !ISNIL(c) && c->flags & TCLOSURE)
#define ISPRIM(c) (c != NULL && !ISNIL(c) && c->flags & TPRIMITIVE)
#define ISFOREIGN(c) (c != NULL && !ISNIL(c) && c->flags & TFOREIGN)

#define CAR(c) (c->_value.cons.car)
#define CDR(c) (c->_value.cons.cdr)

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

static void
_setcar(surd_t *s, cell_t *cons, cell_t *car)
{
  if (cons != s->nil && cons->flags == TCONS) {
    cons->_value.cons.car = car;
  }
  else {
    fprintf(stderr, "error: setcar: not a cons\n");
    exit(1);
  }
}

static void
_setcdr(surd_t *s, cell_t *cons, cell_t *cdr)
{
  if (cons != s->nil && cons->flags == TCONS) {
    cons->_value.cons.cdr = cdr;
  }
  else {
    fprintf(stderr, "error: setcdr: not a cons\n");
    exit(1);
  }
}

cell_t *
surd_env(surd_t *s)
{
  return s->env;
}

static cell_t *
_env_lookup(surd_t *s, cell_t *env, cell_t *sym)
{
  cell_t *envs[] = { env, s->top_env };
  cell_t *tmp;
  int i;

  if (!ISSYM(sym)) {
    fprintf(stderr, "error: attempt to lookup non symbol\n");
    fflush(stderr);
    exit(1);
  }

  for (i = 0; i < 2; i++) {
    tmp = envs[i];
    while (tmp != s->nil && tmp != NULL) {
      if (ISCONS(tmp)) {
        if (ISCONS(CAR(tmp))) {
          cell_t *tmp2 = CAR(CAR(tmp));
          if (tmp2->_value.num == sym->_value.num) {
            return CDR(CAR(tmp));
          }
        }
      }
      tmp = CDR(tmp);
    }
  }

  char *str = internpool_tostring(s->interns, sym->_value.num);
  fprintf(stderr, "error: symbol '%s' not found\n", str);
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
    _setcdr(s, next, next_next);
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

surd_t *
surd_init(void)
{
  surd_t *s = GC_malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  struct internpool *interns = GC_malloc(sizeof(*interns));
  internpool_init(interns);

  s->interns = interns;

  s->nil = (cell_t *)0;
  s->t = (cell_t *)1;
  s->eof = (cell_t *)2;
  s->env = s->nil;
  s->top_env = s->nil;

  s->IF = surd_intern(s, "if");
  s->LAM = surd_intern(s, "lam");
  s->DEF = surd_intern(s, "def");
  s->QUOTE = surd_intern(s, "quote");
  s->TRUE = surd_intern(s, "true");

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

  /* for (size_t i = 0; i < s->interns->buffer_length; i++) { */
  /*   if (s->interns->buffer[i] == 0) { */
  /*     putchar('\n'); */
  /*   } */
  /*   else { */
  /*     putchar(s->interns->buffer[i]); */
  /*   } */
  /* } */

  /* printf("------------------------\n"); */

  /* for (size_t i = 0; i < s->interns->count; i++) { */
  /*   char *sss = internpool_tostring(s->interns, i); */
  /*   printf("%s ==? %s\n", s->interns->buffer + s->interns->offsets[i], sss); */
  /* } */

  /* printf("------------------------\n"); */

#undef INSTALL_PRIMITIVE

  return s;
}

cell_t *
surd_new_cell(surd_t *s)
{
  return GC_malloc(sizeof(cell_t));
}

void
surd_destroy(surd_t *s)
{
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
  cell_t *c = surd_new_cell(s);
  size_t offset = internpool_intern(s->interns, str);
  c->flags = TSYMBOL;
  c->_value.num = offset;
  return c;
}

cell_t *
surd_symbol_equal(surd_t *s, const cell_t *left, const cell_t *right)
{
  if (ISSYM(left) && ISSYM(right)) {
    char *ssl = internpool_tostring(s->interns, left->_value.num);
    char *ssr = internpool_tostring(s->interns, right->_value.num);
    if (left->_value.num == right->_value.num) {
      return s->t;
    }
  }

  return s->nil;
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
        c = CDR(c);
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

typedef int port_getc(void *);
typedef int port_ungetc(void *, int);
typedef int port_putc(void *, int);
typedef int port_close(void *);
typedef int64_t port_tell(void *, int *line, int *col);

struct port {
  port_getc *pgetc;
  port_ungetc *pungetc;
  port_putc *pputc;
  port_close *pclose;
  port_tell *ptell;
  void *underlying;
};

static int getc_(struct port *p) { return p->pgetc(p->underlying); }
static int putc_(struct port *p, int ch) { return p->pputc(p->underlying, ch); }
static int ungetc_(struct port *p, int ch) {
  return p->pungetc(p->underlying, ch);
}
static int close_(struct port *p) { return p->pclose(p->underlying); }
static int tell_(struct port *p, int *line, int *col) {
  return p->ptell(p->underlying, line, col);
}

static int file_getc(void *data) { return fgetc((FILE *)data); }
static int file_putc(void *data, int ch) { return fputc(ch, (FILE *)data); }
static int file_ungetc(void *data, int ch) { return ungetc(ch, (FILE *)data); }
static int file_close(void *data) { return fclose((FILE *)data); }
static int64_t file_tell(void *data, int *line, int *col) {
  if (line != NULL) { *line = -1; }
  if (col != NULL) { *col = -1; }
  return ftell((FILE *)data);
}

static int
eatwhile(struct port *in, const char *skip)
{
  int ch;
  do {
    ch = getc_(in);
    /* update port's line, col */
  } while (ch && strchr(skip, ch) && ch != EOF);
  return ch;
}

static int
skipuntil(struct port *in, const char *stop)
{
  int ch;
  do {
    ch = getc_(in);
    /* update port's line, col */
  } while (ch && !strchr(stop, ch) && ch != EOF);
  return ch;
}

/* return 0 error, 1 for int, 2 for float */
static int
trynumber(const char *buf, int bufi, long int *iout, double *flout)
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
    *iout = intres;
    return 1;
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

    *flout = flores;
    return 2;
  }
  return 0;
}

static cell_t *readlist_(surd_t *s, struct port *in);
static cell_t *readstring_(surd_t *s, struct port *in);

static cell_t *
read_(surd_t *s, struct port *in)
{
  for (;;) {
    int c = eatwhile(in, READ_WHITESPACE);
    switch (c) {
    case EOF:
      return NULL;
    case ';':
      c = skipuntil(in, "\n");
      ungetc_(in, c);
      continue;
    case ')':
      fprintf(stderr, "read closing brace without open");
      exit(1);
    case '\'': {// quote
      cell_t *sym = surd_intern(s, "quote");
      cell_t *tmp = read_(s, in);
      cell_t *tmp2 = surd_cons(s, tmp, s->nil);
      return surd_cons(s, sym, tmp2);
    }
    case '"': return readstring_(s, in);
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
        c = getc_(in);
      } while (c && c != EOF && !strchr(READ_DELIMS, c));
      buf[bufi] = '\0';
      ungetc_(in, c);
      if (bufi == 1 && strchr("-+", buf[0])) { return surd_intern(s, buf); }
      long int intres = 0;
      double flores = 0.0;
      switch (trynumber(buf, bufi, &intres, &flores)) {
      case 0: /* not a number, symbol */
        return surd_intern(s, buf);
      case 1: /* int */
        {
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, intres);
          return tmp;
        }
      case 2: /* float */
        fprintf(stderr, "error: unsupported float");
        exit(1);
      default:
        fprintf(stderr, "error: trynumber returned bad\n");
        exit(1);
      }
    }
    } /* end switch */
  }

  fprintf(stderr, "read_ should never reach this\n");
  exit(1);
  return NULL;
#undef SYMBUF_LEN
}

static cell_t *
readlist_(surd_t *s, struct port *in)
{
  int c = eatwhile(in, READ_WHITESPACE);
  if (c == ')') { return s->nil; }
  if (c == EOF) { return s->eof; }
  ungetc_(in, c);
  cell_t *obj = read_(s, in);
  cell_t *list = readlist_(s, in);
  return surd_cons(s, obj, list);
}

static cell_t *
readstring_(surd_t *s, struct port *in)
{
#define STRLEN 256
  char buf[STRLEN];
  int c = getc_(in), c2;
  int bufi = 0;
  while (c && c != EOF && c != '"') {
    if (bufi == STRLEN - 1) {
      fprintf(stderr, "string too long\n"); /* TODO: NEED DYNAMIC ALLOCATION */
      exit(1);
    }
    if (c == '\\') {
      c2 = getc_(in);
      switch (c2) {
      case 'a': c = '\a'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case '"': c = '"'; break;
      default:
        fprintf(stderr, "error: invalid escape sequence\n");
        exit(1);
      }
    }
    buf[bufi++] = c;
    buf[bufi] = '\0';
    c = getc_(in);
  }
  buf[bufi] = '\0';
  ungetc_(in, c);

  cell_t *str = surd_new_cell(s);
  str->flags = TSTRING;
  str->_value.str.buffer = strdup(buf);
  str->_value.str.length = bufi;
  return str;

#undef STRLEN
}

#undef READ_DELIMS
#undef READ_WHITESPACE


cell_t *
surd_read(surd_t *s, FILE *in)
{
  cell_t *tmp;
  struct port p;
  p.pgetc = file_getc;
  p.pungetc = file_ungetc;
  p.pputc = file_putc;
  p.pclose = file_close;
  p.ptell = file_tell;
  p.underlying = in;
  tmp = read_(s, &p);
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
    fprintf(out, "%s", internpool_tostring(s->interns, exp->_value.num));
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

/* static cell_t * */
/* surd_compile(void) */
/* { */
/*   return NULL; */
/* } */


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
      if (surd_symbol_equal(s, car, s->QUOTE) == s->t) {
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
      else if (surd_symbol_equal(s, car, s->IF) == s->t) {
        exp = _eval_if(s, exp, env);
      }
      else if (surd_symbol_equal(s, car, s->LAM) == s->t) {
        if (surd_list_length(s, exp) > 2) {
          return surd_make_closure(s, exp, env);
        }
        else {
          fprintf(stderr, "error: lam requires at least 2 arguments\n");
          exit(1);
        }
      }
      else if (surd_symbol_equal(s, car, s->DEF) == s->t) {
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
        if (arg1 == arg2) {
          return s->t;
        }
        if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
          if (arg1->_value.num == arg2->_value.num) {
            return s->t;
          }
          return s->nil;
        }
        if (ISSYM(arg2) && ISSYM(arg1)) {
          if (arg1->_value.num == arg2->_value.num) {
            return s->t;
          }
          return s->nil;
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
    cell_t *code = CAR(closure);
    cell_t *nenv = CDR(closure);
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
