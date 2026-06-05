#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <gc.h>
#include "surd.h"

#include "profile.h"

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
    struct closure {
      frame_t *env;
      cell_t *code;
    } closure;
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

struct frame {
  size_t *names; /* symbol offsets */
  cell_t **values;
  size_t length;
  size_t capacity;
  struct frame *parent;
};

static void
frame_init(frame_t *f)
{
  f->names = NULL;
  f->values = NULL;
  f->capacity = 0;
  f->length = 0;
  f->parent = NULL;
}


static size_t
internpool_intern(struct internpool *pool, const char *str)
{ PROFILE_START();

#define INITIAL_BUFFER_CAPACITY 2048
#define INITIAL_OFFSET_CAPACITY 256
  if (!str) { _RETURN -1; }
  size_t len = strlen(str);

  /** at some point, we could binary search on this and speed things up.
   */
  for (size_t i = 0; i < pool->count; i++) {
    if (pool->lengths[i] == len) {
      const char *s = pool->buffer + pool->offsets[i];
      if (strcmp(s, str) == 0) {
        _RETURN i;
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
  _RETURN position;
}

static char *
internpool_tostring(struct internpool *pool, size_t offset)
{ PROFILE_START();

  if (offset >= pool->count) {
    fprintf(stderr, "invalid symbol: %zu\n", offset);
    exit(1);
  }

  size_t buffer_offset = pool->offsets[offset];
  _RETURN pool->buffer + buffer_offset;
}



struct surd {
  /* Symbol table: symbols use cells as well as external memory
     created on the fly via malloc
  */
  struct internpool *interns;

  /* initial eval environment */
  frame_t *env;

  /* Top level environment */
  frame_t *top_env;
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
  PRIM_PRIMITIVEP,
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

static void
die(const char *msg)
{
  fprintf(stderr, "error: %s\n", msg);
  exit(1);
}


cell_t *
surd_car(surd_t *s, cell_t *c)
{ PROFILE_START();

  if (ISCONS(c)) { _RETURN CAR(c); }
  die("can't take car of non-cons");
  _RETURN s->nil;
}

cell_t *
surd_cdr(surd_t *s, cell_t *c)
{ PROFILE_START();

  if (ISCONS(c)) { _RETURN CDR(c); }
  die("can't take cdr of non-cons");
  _RETURN s->nil;
}

static void
_setcar(surd_t *s, cell_t *cons, cell_t *car)
{ PROFILE_START();

  if (cons != s->nil && cons->flags == TCONS) {
    cons->_value.cons.car = car;
    _RETURN;
  }
  die("can't setcar of non-cons");
}

static void
_setcdr(surd_t *s, cell_t *cons, cell_t *cdr)
{ PROFILE_START();

  if (cons != s->nil && cons->flags == TCONS) {
    cons->_value.cons.cdr = cdr;
    _RETURN;
  }
  die("can't setcdr of non-cons");
}

cell_t *
surd_env(surd_t *s)
{ PROFILE_START();

  _RETURN s->env;
}

static cell_t *
_env_lookup(surd_t *s, frame_t *env, cell_t *sym)
{ PROFILE_START();

  frame_t *envs[] = { env, s->top_env };

  if (!ISSYM(sym)) {
    die("attempt to lookup non-symbol");
    _RETURN s->nil;
  }

  for (size_t i = 0; i < 2; i++) {
    frame_t *tmp = envs[i];
    while (tmp != NULL) {
      for (size_t j = 0; j < tmp->length; j++) {
        if (sym->_value.num == (int)tmp->names[j]) {
          _RETURN tmp->values[j];
        }
      }
      tmp = tmp->parent;
    }
  }

  die("symbol not found");
  return s->nil;
}

static void
_env_insert(surd_t *s, frame_t *env, cell_t *sym, cell_t *value)
{ PROFILE_START();
#define INITIAL_FRAME_CAPACITY 4

  if (env == NULL) {
    die("env is null!");
  }

  if (env->length >= env->capacity) {
    size_t new_cap = env->capacity ?
      env->capacity * 2: INITIAL_FRAME_CAPACITY;
    env->names = GC_realloc(env->names, sizeof(env->names) * new_cap);
    env->values = GC_realloc(env->values, sizeof(env->names) * new_cap);
    env->capacity = new_cap;
  }

  env->names[env->length] = sym->_value.num;
  env->values[env->length] = value;
  env->length++;
  _RETURN;
}

static frame_t *
_env_extend(surd_t *s, frame_t *env, cell_t *params, cell_t *args)
{ PROFILE_START();

  cell_t *sym, *val;

  frame_t *result = GC_malloc(sizeof(*result));
  frame_init(result);

  result->parent = env;

  for (;;) {
    if (params == s->nil && args == s->nil) {
      break;
    }
    else if (params == s->nil && args != s->nil) {
      die("too many arguments");
      _RETURN NULL;
    }
    else if (args == s->nil && params != s->nil) {
      die("too few arguments");
      _RETURN NULL;
    }
    else {
      sym = surd_car(s, params);
      val = surd_car(s, args);
      _env_insert(s, result, sym, val);
      params = surd_cdr(s, params);
      args = surd_cdr(s, args);
    }
  }

  _RETURN result;
}

surd_t *
surd_init(void)
{ PROFILE_START();

  surd_t *s = GC_malloc(sizeof(*s));
  if (s == NULL) {
    _RETURN NULL;
  }

  struct internpool *interns = GC_malloc(sizeof(*interns));
  internpool_init(interns);

  s->interns = interns;

  s->nil = (cell_t *)0;
  s->t = (cell_t *)1;
  s->eof = (cell_t *)2;

  s->env = NULL;

  frame_t *top_env = GC_malloc(sizeof(*top_env));
  frame_init(top_env);
  s->top_env = top_env;

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
    _env_insert(s, s->top_env, sym, prim); \
  } \
  else { \
    die("out of memory installing primitive"); \
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
  INSTALL_PRIMITIVE("primitive?", PRIM_PRIMITIVEP, 1);
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

  _RETURN s;
}

cell_t *
surd_new_cell(surd_t *s)
{ PROFILE_START();

  _RETURN GC_malloc(sizeof(cell_t));
}

void
surd_destroy(surd_t *s)
{ PROFILE_START();

  s->env = NULL;
}

void
surd_num_init(surd_t *s, cell_t *c, int value)
{ PROFILE_START();

  c->flags = TFIXNUM;
  c->_value.num = value;
}

cell_t *
surd_intern(surd_t *s, const char *str)
{ PROFILE_START();

  cell_t *c = surd_new_cell(s);
  size_t offset = internpool_intern(s->interns, str);
  c->flags = TSYMBOL;
  c->_value.num = offset;
  _RETURN c;
}

int
surd_symbol_equal(surd_t *s, const cell_t *left, const cell_t *right)
{ PROFILE_START();

  if (ISSYM(left) && ISSYM(right)) {
    if (left->_value.num == right->_value.num) {
      _RETURN 1;
    }
  }

  _RETURN 0;
}

int surd_is_true(surd_t *s, const cell_t *t) { return s->t == t; }
int surd_is_nil(surd_t *s, const cell_t *t) { return s->nil == t; }
int surd_is_eof(surd_t *s, const cell_t *t) { return s->eof == t; }
int surd_is_symbol(surd_t *s, const cell_t *t) { return ISSYM(t); }
int surd_is_fixnum(surd_t *s, const cell_t *t) { return ISFIXNUM(t); }
int surd_is_string(surd_t *s, const cell_t *t) { return ISSTR(t); }
int surd_is_cons(surd_t *s, const cell_t *t) { return ISCONS(t); }
int surd_is_closure(surd_t *s, const cell_t *t) { return ISCLOSURE(t); }
int surd_is_primitive(surd_t *s, const cell_t *t) { return ISPRIM(t); }
int surd_is_foreign(surd_t *s, const cell_t *t) { return ISFOREIGN(t); }

int
surd_as_int(surd_t *s, const cell_t *t, int *result)
{ PROFILE_START();

  /* possibly being able to treat a symbol as an int is wrong */
  if (result != NULL || ISFIXNUM(t) || !ISSYM(t)) {
    *result = t->_value.num;
    _RETURN 1;
  }
  _RETURN 0;
}


void
surd_install_foreign(surd_t *s, const char *name,
                     cell_t *(*func)(surd_t *, cell_t *), int arity)
{ PROFILE_START();

  cell_t *sym = surd_intern(s, name);
  cell_t *prim = surd_new_cell(s);
  if (prim != s->nil) {
    prim->flags = TFOREIGN;
    prim->_value.foreign.arity = arity;
    prim->_value.foreign.cfunc = func;
    _env_insert(s, s->top_env, sym, prim);
    _RETURN;
  }
  die("out of memory in surd_install_foreign");
}

cell_t *
surd_cons(surd_t *s, cell_t *car, cell_t *cdr)
{ PROFILE_START();

  cell_t *new = surd_new_cell(s);
  if (new != s->nil) {
    new->flags = TCONS;
    new->_value.cons.car = car;
    new->_value.cons.cdr = cdr;
    _RETURN new;
  }

  die("out of memory in surd_cons");
  _RETURN s->nil;
}

int
surd_list_length(surd_t *s, cell_t *c)
{ PROFILE_START();

  int len = -1;
  if (c == s->nil) {
    _RETURN 0;
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
  _RETURN len;
}

cell_t *
surd_make_closure(surd_t *s, cell_t *code, frame_t *env)
{ PROFILE_START();

  cell_t *new = surd_new_cell(s);
  if (new != s->nil) {
    new->flags = TCLOSURE;
    new->_value.closure.env = env;
    new->_value.closure.code = code;
    _RETURN new;
  }

  die("out of memory in surd_make_closure");
  _RETURN s->nil;
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
{ PROFILE_START();

  int ch;
  do {
    ch = getc_(in);
    /* update port's line, col */
  } while (ch && strchr(skip, ch) && ch != EOF);
  _RETURN ch;
}

static int
skipuntil(struct port *in, const char *stop)
{ PROFILE_START();

  int ch;
  do {
    ch = getc_(in);
    /* update port's line, col */
  } while (ch && !strchr(stop, ch) && ch != EOF);
  _RETURN ch;
}

/* _RETURN 0 error, 1 for int, 2 for float */
static int
trynumber(const char *buf, int bufi, long int *iout, double *flout)
{ PROFILE_START();

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
    _RETURN 1;
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
    _RETURN 2;
  }
  _RETURN 0;
}

static cell_t *readlist_(surd_t *s, struct port *in);
static cell_t *readstring_(surd_t *s, struct port *in);

static cell_t *
read_(surd_t *s, struct port *in)
{ PROFILE_START();

  for (;;) {
    int c = eatwhile(in, READ_WHITESPACE);
    switch (c) {
    case EOF:
      _RETURN NULL;
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
      _RETURN surd_cons(s, sym, tmp2);
    }
    case '"': _RETURN readstring_(s, in);
    case '(':
      _RETURN readlist_(s, in);
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
      if (bufi == 1 && strchr("-+", buf[0])) { _RETURN surd_intern(s, buf); }
      long int intres = 0;
      double flores = 0.0;
      switch (trynumber(buf, bufi, &intres, &flores)) {
      case 0: /* not a number, symbol */
        _RETURN surd_intern(s, buf);
      case 1: /* int */
        {
          cell_t *tmp = surd_new_cell(s);
          surd_num_init(s, tmp, intres);
          _RETURN tmp;
        }
      case 2: /* float */
        fprintf(stderr, "error: unsupported float");
        exit(1);
      default:
        fprintf(stderr, "error: trynumber _RETURNed bad\n");
        exit(1);
      }
    }
    } /* end switch */
  }

  fprintf(stderr, "read_ should never reach this\n");
  exit(1);
  _RETURN NULL;
#undef SYMBUF_LEN
}

static cell_t *
readlist_(surd_t *s, struct port *in)
{ PROFILE_START();

  int c = eatwhile(in, READ_WHITESPACE);
  if (c == ')') { _RETURN s->nil; }
  if (c == EOF) { _RETURN s->eof; }
  ungetc_(in, c);
  cell_t *obj = read_(s, in);
  cell_t *list = readlist_(s, in);
  _RETURN surd_cons(s, obj, list);
}

static cell_t *
readstring_(surd_t *s, struct port *in)
{ PROFILE_START();

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
  _RETURN str;

#undef STRLEN
}

#undef READ_DELIMS
#undef READ_WHITESPACE


cell_t *
surd_read(surd_t *s, FILE *in)
{ PROFILE_START();

  cell_t *tmp;
  struct port p;
  p.pgetc = file_getc;
  p.pungetc = file_ungetc;
  p.pputc = file_putc;
  p.pclose = file_close;
  p.ptell = file_tell;
  p.underlying = in;
  tmp = read_(s, &p);
  _RETURN tmp;
}

void
surd_display(surd_t *s, FILE *out, cell_t *exp)
{ PROFILE_START();

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
{ PROFILE_START();

  /* XXX: not correct, but we'll take it for now. */
  surd_display(s, out, exp);
}

/* static cell_t * */
/* surd_compile(void) */
/* { */
/*   _RETURN NULL; */
/* } */

static cell_t *
apply_prim(surd_t *s, cell_t *prim, cell_t *args)
{ PROFILE_START();

  int carity = surd_list_length(s, args);
  if (carity == prim->_value.primitive.arity ||
      prim->_value.primitive.arity == -1) {

    switch (prim->_value.primitive.num) {
    case PRIM_CONS:
      _RETURN surd_cons(s, CAR(args), CAR(CDR(args)));
    case PRIM_FIRST: {
      cell_t *arg1 = CAR(args);
      if (ISCONS(arg1)) {
        _RETURN CAR(arg1);
      }
      die("cons required for primitive first");
      _RETURN s->nil;
    }
    case PRIM_REST: {
      cell_t *arg1 =CAR(args);
      if (ISCONS(arg1)) {
        _RETURN CDR(arg1);
      }
      die("cons required for primitive rest");
      _RETURN s->nil;
    }
    case PRIM_NTH: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISCONS(arg2) && ISFIXNUM(arg1)) {
        cell_t *current = arg2;
        for (int i = arg1->_value.num; i > 0; i--) {
          if (current == s->nil) {
            die("nth ran out of conses");
            _RETURN s->nil;
          }
          current = CDR(current);
        }
        if (ISCONS(current)) { _RETURN CAR(current); }
        die("nth ran out of conses");
        _RETURN s->nil;
      }
      die("int and cons required for primitive nth");
      _RETURN s->nil;
    }
    case PRIM_CONSP:
      _RETURN ISCONS(CAR(args)) ? s->t: s->nil;
    case PRIM_NILP:
      _RETURN ISNIL(CAR(args)) ? s->t : s->nil;
    case PRIM_EOFP:
      _RETURN (CAR(args) == s->eof) ? s->t : s->nil;
    case PRIM_FIXNUMP:
      _RETURN ISFIXNUM(CAR(args)) ? s->t : s->nil;
    case PRIM_SYMBOLP:
      _RETURN ISSYM(CAR(args)) ? s->t : s->nil;
    case PRIM_PROCEDUREP: {
      cell_t *arg1 = CAR(args);
      _RETURN (ISPRIM(arg1) || ISCLOSURE(arg1) || ISFOREIGN(arg1)) ?
        s->t : s->nil;
    }
    case PRIM_CLOSUREP:
      _RETURN ISCLOSURE(CAR(args)) ? s->t : s->nil;
    case PRIM_PRIMITIVEP:
      _RETURN ISPRIM(CAR(args)) ? s->t : s->nil;
    case PRIM_FOREIGNP:
      _RETURN ISFOREIGN(CAR(args)) ? s->t : s->nil;
    case PRIM_PLUS: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        cell_t *tmp = surd_new_cell(s);
        surd_num_init(s, tmp, arg1->_value.num + arg2->_value.num);
        _RETURN tmp;
      }
      die("attempt to add a non fixnum");
      _RETURN s->nil;
    }
    case PRIM_MINUS: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        cell_t *tmp = surd_new_cell(s);
        surd_num_init(s, tmp, arg1->_value.num - arg2->_value.num);
        _RETURN tmp;
      }
      die("attempt to subtract a non fixnum");
      _RETURN s->nil;
    }
    case PRIM_MULT: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        cell_t *tmp = surd_new_cell(s);
        surd_num_init(s, tmp, arg1->_value.num * arg2->_value.num);
        _RETURN tmp;
      }
      die("attempt to multiply a non fixnum");
      _RETURN s->nil;
    }
    case PRIM_DIV: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (arg2->_value.num == 0) {
          die("attempt to divide by 0");
          _RETURN s->nil;
        }
        cell_t *tmp = surd_new_cell(s);
        surd_num_init(s, tmp, arg1->_value.num / arg2->_value.num);
        _RETURN tmp;
      }
      die("attempt to divide a non fixnum");
      _RETURN s->nil;
    }
    case PRIM_MOD: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (arg2->_value.num == 0) {
          die("attempt to divide by 0");
          _RETURN s->nil;
        }
        cell_t *tmp = surd_new_cell(s);
        surd_num_init(s, tmp, arg1->_value.num % arg2->_value.num);
        _RETURN tmp;
      }
      die("attempt to mod by non fixnum");
      _RETURN s->nil;
    }
    case PRIM_LT: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (arg1->_value.num < arg2->_value.num) {
          _RETURN s->t;
        }
        _RETURN s->nil;
      }
      die("attempt to compare non fixnums");
      _RETURN s->nil;
    }
    case PRIM_EQ: {
      cell_t *arg1 = CAR(args);
      cell_t *arg2 = CAR(CDR(args));
      if (arg1 == arg2) {
        _RETURN s->t;
      }
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (arg1->_value.num == arg2->_value.num) {
          _RETURN s->t;
        }
        _RETURN s->nil;
      }
      if (ISSYM(arg2) && ISSYM(arg1)) {
        if (arg1->_value.num == arg2->_value.num) {
          _RETURN s->t;
        }
        _RETURN s->nil;
      }
      _RETURN s->nil;
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
  die("arity mismatch");
  _RETURN s->nil;
}

static cell_t *
apply_foreign(surd_t *s, cell_t *foreign, cell_t *args)
{ PROFILE_START();

  die("not implemented");
  _RETURN s->nil;
}


static cell_t *
eval_loop(surd_t *s, cell_t *exp, frame_t *env, int top)
{ PROFILE_START();

  for (;;) {
  recur:
    if (ISFIXNUM(exp) || ISCLOSURE(exp) || ISPRIM(exp) || exp == s->nil) {
      _RETURN exp;
    }
    else if (ISSYM(exp)) {
      _RETURN _env_lookup(s, env, exp);
    }

    if (!ISCONS(exp)) {
      die("don't know how to evaluate this");
      _RETURN s->nil;
    }

    cell_t *car = CAR(exp);
    if (surd_symbol_equal(s, car, s->QUOTE)) {
      cell_t *tmp = CDR(exp);
      if (ISCONS(tmp)) {
        _RETURN CAR(tmp);
      }
      die("in quote: attempted to take the car of nil");
      _RETURN s->nil;
    }
    else if (surd_symbol_equal(s, car, s->IF)) {
      cell_t *condition = surd_car(s, surd_cdr(s, exp));
      cell_t *test = eval_loop(s, condition, env, 0);
      if (test == s->nil) { /* alternate */
        exp = surd_car(s, surd_cdr(s, surd_cdr(s, surd_cdr(s, exp))));
      }
      else {
        exp = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));
      }
      goto recur;
    }
    else if (surd_symbol_equal(s, car, s->LAM)) {
      if (surd_list_length(s, exp) < 2) {
        die("lam requires at least 2 arguments");
        _RETURN s->nil;
      }
      _RETURN surd_make_closure(s, exp, env);
    }
    else if (surd_symbol_equal(s, car, s->DEF)) {
      if (!top) {
        die("def cannot be called from non-toplevel expression");
        _RETURN s->nil;
      }

      cell_t *symbol = surd_car(s, surd_cdr(s, exp));
      cell_t *value = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));

      // TODO: check arity!
      if (ISSYM(symbol)) {
        cell_t *evaled = eval_loop(s, value, env, 0);
        _env_insert(s, s->top_env, symbol, evaled);
        _RETURN evaled;
      }
      else {
        die("def execpected symbol as a second argument");
        _RETURN s->nil;
      }
    }
    else {
      // apply
      cell_t *op = eval_loop(s, car, env, 0);
      cell_t *args = s->nil;
      if (CDR(exp) != s->nil) {
        cell_t *list = CDR(exp);
        cell_t *tmp = eval_loop(s, CAR(list), env, 0);
        cell_t *next = s->nil;
        cell_t *next_next = s->nil;
        cell_t *evaled = s->nil;
        args = surd_cons(s, tmp, s->nil);
        next = args;
        tmp = CDR(list);
        while (tmp != s->nil && tmp) {
          evaled = eval_loop(s, CAR(tmp), env, 0);
          next_next = surd_cons(s, evaled, s->nil);
          _setcdr(s, next, next_next);
          next = next_next;
          tmp = CDR(tmp);
        }
      }

      if (ISPRIM(op)) {
        _RETURN apply_prim(s, op, args);
      }
      else if (ISFOREIGN(op)) {
        _RETURN apply_foreign(s, op, args);
      }

      cell_t *code = op->_value.closure.code;
      frame_t *cenv = op->_value.closure.env;
      env = _env_extend(s, cenv, surd_car(s, surd_cdr(s, code)), args);
      exp = surd_car(s, surd_cdr(s, surd_cdr(s, code)));
      goto recur;
    }
  }
  _RETURN s->nil;
}


cell_t *
surd_eval(surd_t *s, cell_t *exp, frame_t *env, int top)
{ PROFILE_START();

  _RETURN eval_loop(s, exp, env, top);
}

cell_t *
surd_apply(surd_t *s, cell_t *closure, cell_t *args)
{ PROFILE_START();

  die("not implemented");
  _RETURN s->nil;
}

cell_t *
surd_load(surd_t *s, FILE *in)
{ PROFILE_START();

  cell_t *tmp;
  cell_t *last = s->nil;
  while ((tmp = surd_read(s, in)) != NULL) {
    last = surd_eval(s, tmp, surd_env(s), 1);
  }
  _RETURN last;
}



static void
repl(surd_t *s)
{ PROFILE_START();

  int lc;
  cell_t *cell;
  for (lc = 0;; lc++) {
    printf("surd: %d> ", lc);
    cell = surd_read(s, stdin);
    if (cell) {
      cell = surd_eval(s, cell, surd_env(s), 1);
      printf("\n  #res:%d => ", lc);
      surd_display(s, stdout, cell);
      printf("\n");
    }
    else {
      exit(0);
    }
  }
}

#ifndef TEST_BUILD
int
main(int argc, char *argv[])
{
  struct timespec start, end;

  FILE *in;
  surd_t *surd = surd_init();

  if (argc > 1) {
    in = fopen(argv[1], "r");
    if (in != NULL) {
      clock_gettime(CLOCK_MONOTONIC, &start);
      surd_load(surd, in);

      clock_gettime(CLOCK_MONOTONIC, &end);
      long sec = end.tv_sec - start.tv_sec;
      long nsec = end.tv_nsec - start.tv_nsec;
      if (nsec < 0) {
        --sec;
        nsec += 1000000000;
      }
      fprintf(stderr, "elapsed load time: %ld.%09ld seconds\n", sec, nsec);
      fclose(in);
    }
    else {
      fprintf(stderr, "file (%s) could not be opened.\n", argv[1]);
      exit(EXIT_FAILURE);
    }
  }
  else {
    repl(surd);
  }

  print_profiler_report();
  return 0;
}
#endif
