#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdint.h>
#include <gc.h>
#include "surd.h"

#include "profile.h"

#define TAG_BITS 0x7
#define FIXNUM_TAG 0x1
#define SYMBOL_TAG 0x6
#define NIL_VALUE 0x0
#define TRUE_VALUE 0x2
#define EOF_VALUE 0x4

typedef enum {
  TCONS=0x1,
  TSTRING=0x2,
  TCLOSURE=0x4,
  TPRIMITIVE=0x8,
  TFOREIGN=0x10,
  TPORT=0x20,
  TBOX=0x40,
} type_t;

struct cell {
  unsigned int flags;
  union {
    surd_value box;
    struct cons {
      surd_value car;
      surd_value cdr;
    } cons;
    struct str {
      size_t length;
      char *buffer;
    } str;
    struct closure {
      frame_t *env;
      surd_value code;
    } closure;
    struct primitive {
      int arity;
      int num;
    } primitive;
    struct foreign {
      int arity;
      surd_value (*cfunc)(surd_t *, surd_value);
    } foreign;
    struct port *port;
  } _value;
};

cell_t *
make_cell(surd_t *s)
{ PROFILE_START();

  _RETURN GC_malloc(sizeof(cell_t));
}

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
  size_t *names; /* symbol IDs */
  surd_value *values;
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

static surd_value
make_fixnum(int64_t value)
{
  return (surd_value)(((uint64_t)value << 1) | FIXNUM_TAG);
}

static int64_t
fixnum_of(surd_value fixnum)
{
  return (int64_t)fixnum >> 1;
}

static surd_value
make_symbol(size_t sym_id)
{
  return (surd_value)((sym_id << 3) | SYMBOL_TAG);
}

static size_t
symbol_id_of(surd_value sym)
{
  return sym >> 3;
}

static int
is_ptr(surd_value v)
{
  return ((v & TAG_BITS) == 0 && v != 0);
}

static cell_t *
cell_of(surd_value v)
{
  return (cell_t *)v;
}

static surd_value
value_of(cell_t *c)
{
  return (surd_value)c;
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

  surd_value IF;
  surd_value QUOTE;
  surd_value LAM;
  surd_value DEF;
  surd_value BEG;
  surd_value TRUE;

  int load_depth;
};

enum SURD_PRIMITIVES {
  PRIM_CONS,
  PRIM_FIRST,
  PRIM_REST,
  PRIM_NTH,
  PRIM_CONSP,
  PRIM_NILP,
  PRIM_EOFP,
  PRIM_BOXP,
  PRIM_FIXNUMP,
  PRIM_SYMBOLP,
  PRIM_STRINGP,
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
  PRIM_CLOSE,
  PRIM_GETBYTE,
  PRIM_PUTBYTE,
  PRIM_PUTSTR,
  PRIM_LOAD,
  PRIM_STRLEN,
  PRIM_BOX,
  PRIM_UNBOX,
  PRIM_SETBOX,
};

#define ISNIL(v) ((v) == 0)
#define ISTRUE(v) ((v) == TRUE_VALUE)
#define ISEOF(v) ((v) == EOF_VALUE)
#define ISFIXNUM(v) ((v) & 1)
#define ISSYM(v) (((v) & TAG_BITS) == SYMBOL_TAG)

static int ISBOX(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TBOX; }
static int ISSTR(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TSTRING; }
static int ISCONS(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TCONS; }
static int ISCLOSURE(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TCLOSURE; }
static int ISPRIM(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TPRIMITIVE; }
static int ISFOREIGN(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TFOREIGN; }
static int ISPORT(surd_value v) { return is_ptr(v) && cell_of(v)->flags & TPORT; }

#define CAR(c) cell_of(c)->_value.cons.car
#define CDR(c) cell_of(c)->_value.cons.cdr

static void
die(const char *msg)
{
  fprintf(stderr, "error: %s\n", msg);
  exit(1);
}


surd_value
surd_car(surd_t *s, surd_value c)
{ PROFILE_START();

  if (ISCONS(c)) { _RETURN CAR(c); }
  die("can't take car of non-cons");
  _RETURN NIL_VALUE;
}

surd_value
surd_cdr(surd_t *s, surd_value c)
{ PROFILE_START();

  if (ISCONS(c)) { _RETURN CDR(c); }
  die("can't take cdr of non-cons");
  _RETURN NIL_VALUE;
}

static void
_setcar(surd_t *s, surd_value cons, surd_value car)
{ PROFILE_START();

  if (ISCONS(cons)) {
    cell_of(cons)->_value.cons.car = car;
    _RETURN;
  }
  die("can't setcar of non-cons");
}

static void
_setcdr(surd_t *s, surd_value cons, surd_value cdr)
{ PROFILE_START();

  if (ISCONS(cons)) {
    cell_of(cons)->_value.cons.cdr = cdr;
    _RETURN;
  }
  die("can't setcdr of non-cons");
}

frame_t *
surd_env(surd_t *s)
{ PROFILE_START();

  _RETURN s->env;
}

static surd_value
_env_lookup(surd_t *s, frame_t *env, surd_value sym)
{ PROFILE_START();

  frame_t *envs[] = { env, s->top_env };

  if (!ISSYM(sym)) {
    die("attempt to lookup non-symbol");
    _RETURN NIL_VALUE;
  }

  size_t sym_id = symbol_id_of(sym);
  for (size_t i = 0; i < 2; i++) {
    frame_t *tmp = envs[i];
    while (tmp != NULL) {
      for (size_t j = 0; j < tmp->length; j++) {
        if (sym_id == tmp->names[j]) {
          _RETURN tmp->values[j];
        }
      }
      tmp = tmp->parent;
    }
  }

  die("symbol not found");
  return 0;
}

static void
_env_insert(surd_t *s, frame_t *env, surd_value sym, surd_value value)
{ PROFILE_START();
#define INITIAL_FRAME_CAPACITY 4

  if (env == NULL) {
    die("env is null!");
  }

  if (env->length >= env->capacity) {
    size_t new_cap = env->capacity ?
      env->capacity * 2: INITIAL_FRAME_CAPACITY;
    env->names = GC_realloc(env->names, sizeof(size_t) * new_cap);
    env->values = GC_realloc(env->values, sizeof(surd_value) * new_cap);
    env->capacity = new_cap;
  }

  env->names[env->length] = symbol_id_of(sym);
  env->values[env->length] = value;
  env->length++;
  _RETURN;
}

static frame_t *
_env_extend(surd_t *s, frame_t *env, surd_value params, surd_value args)
{ PROFILE_START();

  surd_value sym, val;

  frame_t *result = GC_malloc(sizeof(*result));
  frame_init(result);

  result->parent = env;

  for (;;) {
    if (ISNIL(params) && ISNIL(args)) {
      break;
    }
    else if (ISNIL(params) && !ISNIL(args)) {
      die("too many arguments");
      _RETURN NULL;
    }
    else if (ISNIL(args) && !ISNIL(params)) {
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

static cell_t *make_port_cell(surd_t *s, FILE *f, int noclose);

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

  s->env = NULL;

  frame_t *top_env = GC_malloc(sizeof(*top_env));
  frame_init(top_env);
  s->top_env = top_env;

  s->IF = surd_intern(s, "if");
  s->LAM = surd_intern(s, "lam");
  s->DEF = surd_intern(s, "def");
  s->QUOTE = surd_intern(s, "quote");
  s->BEG = surd_intern(s, "beg");
  s->TRUE = surd_intern(s, "true");

  surd_value sym, prim;

#define INSTALL_PRIMITIVE(NAME, NUM, ARITY) \
  sym = surd_intern(s, NAME); \
  prim = value_of(make_cell(s)); \
  if (!ISNIL(prim)) { \
    cell_of(prim)->flags = TPRIMITIVE; \
    cell_of(prim)->_value.primitive.arity = ARITY; \
    cell_of(prim)->_value.primitive.num = NUM; \
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
  INSTALL_PRIMITIVE("box?", PRIM_BOXP, 1);
  INSTALL_PRIMITIVE("fixnum?", PRIM_FIXNUMP, 1);
  INSTALL_PRIMITIVE("symbol?", PRIM_SYMBOLP, 1);
  INSTALL_PRIMITIVE("string?", PRIM_STRINGP, 1);
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
  INSTALL_PRIMITIVE("close", PRIM_CLOSE, 1);
  INSTALL_PRIMITIVE("get-byte", PRIM_GETBYTE, 1);
  INSTALL_PRIMITIVE("put-byte", PRIM_PUTBYTE, 2);
  INSTALL_PRIMITIVE("put-str", PRIM_PUTSTR, 2);
  INSTALL_PRIMITIVE("load", PRIM_LOAD, 1);
  INSTALL_PRIMITIVE("strlen", PRIM_STRLEN, 1);
  INSTALL_PRIMITIVE("box", PRIM_BOX, 1);
  INSTALL_PRIMITIVE("unbox", PRIM_UNBOX, 1);
  INSTALL_PRIMITIVE("set-box!", PRIM_SETBOX, 2);

#undef INSTALL_PRIMITIVE

#define INSTALL_STD_PORT(NAME, FD, MODE) \
  sym = surd_intern(s, NAME); \
  _env_insert(s, s->top_env, sym, surd_make_port(s, fdopen(FD, MODE)));

  INSTALL_STD_PORT("stdin",  STDIN_FILENO,  "r");
  INSTALL_STD_PORT("stdout", STDOUT_FILENO, "w");
  INSTALL_STD_PORT("stderr", STDERR_FILENO, "w");

#undef INSTALL_STD_PORT

  s->load_depth = 0;

  _RETURN s;
}

void
surd_destroy(surd_t *s)
{ PROFILE_START();

  s->env = NULL;
}

surd_value
surd_make_port(surd_t *s, FILE *p)
{ PROFILE_START();

  _RETURN value_of(make_port_cell(s, p, 0));
}

surd_value
surd_fixnum(surd_t *s, int64_t value)
{ PROFILE_START();
  _RETURN make_fixnum(value);
}

surd_value
surd_nil(surd_t *s)
{ PROFILE_START();
  _RETURN NIL_VALUE;
}

surd_value
surd_intern(surd_t *s, const char *str)
{ PROFILE_START();

  size_t offset = internpool_intern(s->interns, str);
  _RETURN make_symbol(offset);
}

int
surd_symbol_equal(surd_t *s, surd_value left, surd_value right)
{ PROFILE_START();

  if (ISSYM(left) && ISSYM(right)) {
    if (symbol_id_of(left) == symbol_id_of(right)) {
      _RETURN 1;
    }
  }

  _RETURN 0;
}

int surd_is_true(surd_t *s, surd_value t) { return ISTRUE(t); }
int surd_is_nil(surd_t *s, surd_value t) { return ISNIL(t); }
int surd_is_eof(surd_t *s, surd_value t) { return ISEOF(t); }
int surd_is_symbol(surd_t *s, surd_value t) { return ISSYM(t); }
int surd_is_box(surd_t *s, surd_value t) { return ISBOX(t); }
int surd_is_fixnum(surd_t *s, surd_value t) { return ISFIXNUM(t); }
int surd_is_string(surd_t *s, surd_value t) { return ISSTR(t); }
int surd_is_cons(surd_t *s, surd_value t) { return ISCONS(t); }
int surd_is_closure(surd_t *s, surd_value t) { return ISCLOSURE(t); }
int surd_is_primitive(surd_t *s, surd_value t) { return ISPRIM(t); }
int surd_is_foreign(surd_t *s, surd_value t) { return ISFOREIGN(t); }

int
surd_as_int(surd_t *s, surd_value t, int64_t *result)
{ PROFILE_START();

  if (ISFIXNUM(t)) {
    *result = fixnum_of(t);
    _RETURN 1;
  }
  _RETURN 0;
}


void
surd_install_foreign(surd_t *s, const char *name,
                     surd_value (*func)(surd_t *, surd_value), int arity)
{ PROFILE_START();

  surd_value sym = surd_intern(s, name);
  surd_value prim = value_of(make_cell(s));
  if (!ISNIL(prim)) {
    cell_of(prim)->flags = TFOREIGN;
    cell_of(prim)->_value.foreign.arity = arity;
    cell_of(prim)->_value.foreign.cfunc = func;
    _env_insert(s, s->top_env, sym, prim);
    _RETURN;
  }
  die("out of memory in surd_install_foreign");
}

surd_value
surd_cons(surd_t *s, surd_value car, surd_value cdr)
{ PROFILE_START();

  cell_t *new = make_cell(s);
  if (new != NULL) {
    new->flags = TCONS;
    new->_value.cons.car = car;
    new->_value.cons.cdr = cdr;
    _RETURN value_of(new);
  }

  die("out of memory in surd_cons");
  _RETURN NIL_VALUE;
}

int
surd_list_length(surd_t *s, surd_value c)
{ PROFILE_START();

  int len = -1;
  if (ISNIL(c)) {
    _RETURN 0;
  }
  else if (ISCONS(c)) {
    len = 0;
    while (!ISNIL(c)) {
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

surd_value
surd_make_closure(surd_t *s, surd_value code, frame_t *env)
{ PROFILE_START();

  cell_t *new = make_cell(s);
  if (new != NULL) {
    new->flags = TCLOSURE;
    new->_value.closure.env = env;
    new->_value.closure.code = code;
    _RETURN value_of(new);
  }

  die("out of memory in surd_make_closure");
  _RETURN NIL_VALUE;
}

surd_value surd_box(surd_t *s, surd_value v)
{ PROFILE_START();
  cell_t *new = make_cell(s);
  if (new != NULL) {
    new->flags = TBOX;
    new->_value.box = v;
    _RETURN value_of(new);
  }
  die("out of memory in box");
  _RETURN NIL_VALUE;
}

surd_value surd_unbox(surd_t *s, surd_value b)
{ PROFILE_START();
  if (ISBOX(b)) {
    _RETURN cell_of(b)->_value.box;
  }
  die("not a box");
  _RETURN NIL_VALUE;
}

surd_value surd_setbox(surd_t *s, surd_value b, surd_value v)
{ PROFILE_START();
  if (ISBOX(b)) {
    cell_of(b)->_value.box = v;
    return b;
  }
  die("set-box!: not a box");
  _RETURN NIL_VALUE;
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
static int file_noclose(void *data) { (void)data; return 0; }
static int64_t file_tell(void *data, int *line, int *col) {
  if (line != NULL) { *line = -1; }
  if (col != NULL) { *col = -1; }
  return ftell((FILE *)data);
}

static cell_t *
make_port_cell(surd_t *s, FILE *f, int noclose)
{ PROFILE_START();

  struct port *p = GC_malloc(sizeof(*p));
  p->pgetc = file_getc;
  p->pungetc = file_ungetc;
  p->pputc = file_putc;
  p->pclose = noclose ? file_noclose : file_close;
  p->ptell = file_tell;
  p->underlying = f;
  cell_t *c = make_cell(s);
  c->flags = TPORT;
  c->_value.port = p;
  _RETURN c;
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

static surd_value readlist_(surd_t *s, struct port *in);
static surd_value readstring_(surd_t *s, struct port *in);
static surd_value readchar_(surd_t *s, struct port *in);

static surd_value
read_(surd_t *s, struct port *in)
{ PROFILE_START();

  for (;;) {
    int c = eatwhile(in, READ_WHITESPACE);
    switch (c) {
    case EOF:
      _RETURN NIL_VALUE;  /* return nil to signal EOF of read */
    case ';':
      c = skipuntil(in, "\n");
      ungetc_(in, c);
      continue;
    case ')':
      fprintf(stderr, "read closing brace without open");
      exit(1);
    case '\'': {// quote
      surd_value sym = surd_intern(s, "quote");
      surd_value tmp = read_(s, in);
      surd_value tmp2 = surd_cons(s, tmp, 0);
      _RETURN surd_cons(s, sym, tmp2);
    }
    case '"': _RETURN readstring_(s, in);
    case '#': _RETURN readchar_(s, in);
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
      case 0: {
        /* not a number, symbol */
        surd_value sym = surd_intern(s, buf);

        /* is it a known value? */
        if (sym == s->TRUE) {
          return TRUE_VALUE;
        }
        _RETURN sym;
      }
      case 1: /* int */
        _RETURN surd_fixnum(s, intres);
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
  _RETURN NIL_VALUE;
#undef SYMBUF_LEN
}

static surd_value
readlist_(surd_t *s, struct port *in)
{ PROFILE_START();

  int c = eatwhile(in, READ_WHITESPACE);
  if (c == ')') { _RETURN NIL_VALUE; }
  if (c == EOF) { _RETURN EOF_VALUE; }
  ungetc_(in, c);
  surd_value obj = read_(s, in);
  surd_value list = readlist_(s, in);
  _RETURN surd_cons(s, obj, list);
}

static surd_value
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

  cell_t *str = make_cell(s);
  str->flags = TSTRING;
  str->_value.str.buffer = strdup(buf);
  str->_value.str.length = bufi;
  _RETURN value_of(str);

#undef STRLEN
}

static surd_value
readchar_(surd_t *s, struct port *in)
{ PROFILE_START();

  int c2 = getc_(in);
  if (c2 != '\\') {
    fprintf(stderr, "read: unexpected #%c\n", c2);
    exit(1);
  }
  /* read character name or single character */
  char buf[16];
  int bufi = 0;
  int c3 = getc_(in);
  if (c3 == EOF) { _RETURN EOF_VALUE; }
  buf[bufi++] = c3;
  /* is this a named character? */
  c3 = getc_(in);
  while (c3 != EOF && !strchr(READ_DELIMS, c3)) {
    if (bufi < 15) buf[bufi++] = c3;
    c3 = getc_(in);
  }
  ungetc_(in, c3);
  buf[bufi] = '\0';
  int chval;
  if (bufi == 1) {
    chval = (unsigned char)buf[0];
  } else if (strcmp(buf, "space") == 0)     { chval = ' '; }
  else if (strcmp(buf, "newline") == 0)      { chval = '\n'; }
  else if (strcmp(buf, "tab") == 0)          { chval = '\t'; }
  else if (strcmp(buf, "return") == 0)       { chval = '\r'; }
  else if (strcmp(buf, "nul") == 0)          { chval = '\0'; }
  else if (strcmp(buf, "backspace") == 0)    { chval = '\b'; }
  else if (strcmp(buf, "delete") == 0)       { chval = 127; }
  else if (strcmp(buf, "escape") == 0)       { chval = 27; }
  else if (strcmp(buf, "page") == 0)         { chval = '\f'; }
  else {
    fprintf(stderr, "read: unknown character name: #\\%s\n", buf);
    exit(1);
  }
  _RETURN surd_fixnum(s, chval);
}

#undef READ_DELIMS
#undef READ_WHITESPACE

surd_value
surd_read(surd_t *s, FILE *in)
{ PROFILE_START();

  surd_value tmp;
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

static void
write_cell_(surd_t *s, FILE *out, surd_value exp, int quote_strings)
{ PROFILE_START();

  int sep = 0;
  surd_value tmp;

  if (ISNIL(exp)) {
    fprintf(out, "()");
  }
  else if (ISFIXNUM(exp)) {
    fprintf(out, "%" PRId64, fixnum_of(exp));
  }
  else if (ISSYM(exp)) {
    fprintf(out, "%s", internpool_tostring(s->interns, symbol_id_of(exp)));
  }
  else if (ISSTR(exp)) {
    cell_t *c = cell_of(exp);
    if (quote_strings) {
      fprintf(out, "\"");
      for (size_t i = 0; i < c->_value.str.length; i++) {
        char ch = c->_value.str.buffer[i];
        if (ch == '"') { fprintf(out, "\\\""); }
        else if (ch == '\\') { fprintf(out, "\\\\"); }
        else if (ch == '\n') { fprintf(out, "\\n"); }
        else if (ch == '\t') { fprintf(out, "\\t"); }
        else { fprintf(out, "%c", ch); }
      }
      fprintf(out, "\"");
    } else {
      for (size_t i = 0; i < c->_value.str.length; i++) {
        fprintf(out, "%c", c->_value.str.buffer[i]);
      }
    }
  }
  else if (ISCONS(exp)) {
    fprintf(out, "(");
    tmp = exp;
    while (!ISNIL(tmp)) {
      if (sep) { fprintf(out, " "); }
      if (ISCONS(tmp)) {
        write_cell_(s, out, CAR(tmp), quote_strings);
        tmp = CDR(tmp);
      }
      else {
        write_cell_(s, out, tmp, quote_strings);
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
  else if (ISPORT(exp)) {
    fprintf(out, "<#port: %p>", (void *)cell_of(exp)->_value.port->underlying);
  }
  else {
    fprintf(out, "umm...");
  }
}

void
surd_display(surd_t *s, FILE *out, surd_value exp)
{ PROFILE_START();
  write_cell_(s, out, exp, 0);
}

void
surd_write(surd_t *s, FILE *out, surd_value exp)
{ PROFILE_START();
  write_cell_(s, out, exp, 1);
}

/* static cell_t * */
/* surd_compile(void) */
/* { */
/*   _RETURN NULL; */
/* } */

static surd_value
apply_prim(surd_t *s, surd_value prim, surd_value args)
{ PROFILE_START();

#define MAX_LOAD_DEPTH 10

  int carity = surd_list_length(s, args);
  cell_t *prim_cell = cell_of(prim);
  if (carity == prim_cell->_value.primitive.arity ||
      prim_cell->_value.primitive.arity == -1) {

    switch (prim_cell->_value.primitive.num) {
    case PRIM_CONS:
      _RETURN surd_cons(s, CAR(args), CAR(CDR(args)));
    case PRIM_FIRST: {
      surd_value arg1 = CAR(args);
      if (ISCONS(arg1)) {
        _RETURN CAR(arg1);
      }
      if (ISSTR(arg1)) {
        cell_t *c = cell_of(arg1);
        if (c->_value.str.length == 0) {
          _RETURN NIL_VALUE;
        }
        _RETURN surd_fixnum(s, (unsigned char)c->_value.str.buffer[0]);
      }
      die("cons or string required for primitive first");
      _RETURN NIL_VALUE;
    }
    case PRIM_REST: {
      surd_value arg1 = CAR(args);
      if (ISCONS(arg1)) {
        _RETURN CDR(arg1);
      }
      if (ISSTR(arg1)) {
        cell_t *c = cell_of(arg1);
        if (c->_value.str.length == 0) {
          _RETURN NIL_VALUE;
        }
        cell_t *str = make_cell(s);
        str->flags = TSTRING;
        str->_value.str.buffer = strdup(c->_value.str.buffer + 1);
        str->_value.str.length = c->_value.str.length - 1;
        _RETURN value_of(str);
      }
      die("cons or string required for primitive rest");
      _RETURN NIL_VALUE;
    }
    case PRIM_NTH: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISCONS(arg2) && ISFIXNUM(arg1)) {
        surd_value current = arg2;
        for (int64_t i = fixnum_of(arg1); i > 0; i--) {
          if (ISNIL(current)) {
            die("nth ran out of conses");
            _RETURN NIL_VALUE;
          }
          current = CDR(current);
        }
        if (ISCONS(current)) { _RETURN CAR(current); }
        die("nth ran out of conses");
        _RETURN NIL_VALUE;
      }
      die("int and cons required for primitive nth");
      _RETURN NIL_VALUE;
    }
    case PRIM_CONSP:
      _RETURN ISCONS(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_NILP:
      _RETURN ISNIL(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_EOFP:
      _RETURN (CAR(args) == EOF_VALUE) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_FIXNUMP:
      _RETURN ISFIXNUM(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_SYMBOLP:
      _RETURN ISSYM(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_STRINGP:
      _RETURN ISSTR(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_PROCEDUREP: {
      surd_value arg1 = CAR(args);
      _RETURN (ISPRIM(arg1) || ISCLOSURE(arg1) || ISFOREIGN(arg1)) ?
        TRUE_VALUE : NIL_VALUE;
    }
    case PRIM_CLOSUREP:
      _RETURN ISCLOSURE(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_PRIMITIVEP:
      _RETURN ISPRIM(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_FOREIGNP:
      _RETURN ISFOREIGN(CAR(args)) ? TRUE_VALUE : NIL_VALUE;
    case PRIM_PLUS: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        _RETURN surd_fixnum(s, fixnum_of(arg1) + fixnum_of(arg2));
      }
      die("attempt to add a non fixnum");
      _RETURN NIL_VALUE;
    }
    case PRIM_MINUS: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        _RETURN surd_fixnum(s, fixnum_of(arg1) - fixnum_of(arg2));
      }
      die("attempt to subtract a non fixnum");
      _RETURN NIL_VALUE;
    }
    case PRIM_MULT: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        _RETURN surd_fixnum(s, fixnum_of(arg1) * fixnum_of(arg2));
      }
      die("attempt to multiply a non fixnum");
      _RETURN NIL_VALUE;
    }
    case PRIM_DIV: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        int64_t divisor = fixnum_of(arg2);
        if (divisor == 0) {
          die("attempt to divide by 0");
          _RETURN NIL_VALUE;
        }
        _RETURN surd_fixnum(s, fixnum_of(arg1) / divisor);
      }
      die("attempt to divide a non fixnum");
      _RETURN NIL_VALUE;
    }
    case PRIM_MOD: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        int64_t divisor = fixnum_of(arg2);
        if (divisor == 0) {
          die("attempt to divide by 0");
          _RETURN NIL_VALUE;
        }
        _RETURN surd_fixnum(s, fixnum_of(arg1) % divisor);
      }
      die("attempt to mod by non fixnum");
      _RETURN NIL_VALUE;
    }
    case PRIM_LT: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (fixnum_of(arg1) < fixnum_of(arg2)) {
          _RETURN TRUE_VALUE;
        }
        _RETURN NIL_VALUE;
      }
      die("attempt to compare non fixnums");
      _RETURN NIL_VALUE;
    }
    case PRIM_EQ: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (arg1 == arg2) {
        _RETURN TRUE_VALUE;
      }
      if (ISFIXNUM(arg2) && ISFIXNUM(arg1)) {
        if (fixnum_of(arg1) == fixnum_of(arg2)) {
          _RETURN TRUE_VALUE;
        }
        _RETURN NIL_VALUE;
      }
      if (ISSYM(arg2) && ISSYM(arg1)) {
        if (symbol_id_of(arg1) == symbol_id_of(arg2)) {
          _RETURN TRUE_VALUE;
        }
        _RETURN NIL_VALUE;
      }
      _RETURN NIL_VALUE;
    }
    case PRIM_READ: {
      surd_value arg1 = CAR(args);
      if (!ISPORT(arg1)) { die("read requires a port"); }
      surd_value result = read_(s, cell_of(arg1)->_value.port);
      _RETURN result ? result : EOF_VALUE;
    }
    case PRIM_WRITE: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (!ISPORT(arg2)) { die("write requires a port as second argument"); }
      surd_write(s, (FILE *)cell_of(arg2)->_value.port->underlying, arg1);
      _RETURN arg1;
    }
    case PRIM_OPEN: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (!ISSTR(arg1) || !ISSTR(arg2)) { die("open requires two strings"); }
      cell_t *c1 = cell_of(arg1);
      cell_t *c2 = cell_of(arg2);
      FILE *f = fopen(c1->_value.str.buffer, c2->_value.str.buffer);
      if (!f) { die("open failed"); }
      _RETURN value_of(make_port_cell(s, f, 0));
    }
    case PRIM_CLOSE: {
      surd_value arg1 = CAR(args);
      if (!ISPORT(arg1)) { die("close requires a port"); }
      close_(cell_of(arg1)->_value.port);
      _RETURN arg1;
    }
    case PRIM_GETBYTE: {
      surd_value arg1 = CAR(args);
      if (!ISPORT(arg1)) { die("get-byte requires a port"); }
      int ch = getc_(cell_of(arg1)->_value.port);
      if (ch == EOF) { _RETURN EOF_VALUE; }
      _RETURN surd_fixnum(s, ch);
    }
    case PRIM_PUTBYTE: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (!ISFIXNUM(arg1) || !ISPORT(arg2)) { die("put-byte requires fixnum and port"); }
      putc_(cell_of(arg2)->_value.port, fixnum_of(arg1));
      _RETURN arg1;
    }
    case PRIM_PUTSTR: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (!ISSTR(arg1) || !ISPORT(arg2)) { die("put-str requires string and port"); }
      cell_t *c1 = cell_of(arg1);
      for (size_t i = 0; i < c1->_value.str.length; i++) {
        putc_(cell_of(arg2)->_value.port, c1->_value.str.buffer[i]);
      }
      _RETURN arg1;
    }
    case PRIM_LOAD: {
      surd_value arg1 = CAR(args);
      if (!ISSTR(arg1)) { die("load requires a string filename"); }
      if (s->load_depth >= 10) { die("load depth exceeded (max 10)"); }
      FILE *f = fopen(cell_of(arg1)->_value.str.buffer, "r");
      if (!f) { die("could not open file for load"); }
      s->load_depth++;
      surd_value result = surd_load(s, f);
      s->load_depth--;
      fclose(f);
      _RETURN result ? result : NIL_VALUE;
    }
    case PRIM_STRLEN: {
      surd_value arg1 = CAR(args);
      if (ISSTR(arg1)) {
        _RETURN surd_fixnum(s, cell_of(arg1)->_value.str.length);
      }
      die("strlen requires a string");
      _RETURN NIL_VALUE;
    }
    case PRIM_BOX: {
      surd_value arg1 = CAR(args);
      _RETURN surd_box(s, arg1);
    }
    case PRIM_UNBOX: {
      surd_value arg1 = CAR(args);
      if (ISBOX(arg1)) {
        _RETURN cell_of(arg1)->_value.box;
      }
      die("unbox requires a box");
      _RETURN NIL_VALUE;
    }
    case PRIM_SETBOX: {
      surd_value arg1 = CAR(args);
      surd_value arg2 = CAR(CDR(args));
      if (ISBOX(arg1)) {
        _RETURN surd_setbox(s, arg1, arg2);
      }
      fprintf(stderr, "IT's a %d\n", cell_of(arg1)->flags);
      fflush(stderr);
      die("strlen requires a string");
      _RETURN NIL_VALUE;
    }

    default:
      fprintf(stderr,"unknown primitive\n");
      exit(1);
    }
  }
  die("arity mismatch");
  _RETURN NIL_VALUE;

#undef MAX_LOAD_DEPTH
}

static surd_value
apply_foreign(surd_t *s, surd_value foreign, surd_value args)
{ PROFILE_START();

  die("not implemented");
  _RETURN NIL_VALUE;
}


static surd_value
eval_loop(surd_t *s, surd_value exp, frame_t *env, int top)
{ PROFILE_START();

  for (;;) {
  recur:
    if (ISFIXNUM(exp) || ISCLOSURE(exp) || ISPRIM(exp) || ISSTR(exp) || ISNIL(exp) || ISTRUE(exp) || ISEOF(exp)) {
      _RETURN exp;
    }
    else if (ISSYM(exp)) {
      _RETURN _env_lookup(s, env, exp);
    }

    if (!ISCONS(exp)) {
      die("don't know how to evaluate this");
      _RETURN NIL_VALUE;
    }

    surd_value car = CAR(exp);
    if (surd_symbol_equal(s, car, s->QUOTE)) {
      surd_value tmp = CDR(exp);
      if (ISCONS(tmp)) {
        _RETURN CAR(tmp);
      }
      die("in quote: attempted to take the car of nil");
      _RETURN NIL_VALUE;
    }
    else if (surd_symbol_equal(s, car, s->IF)) {
      surd_value condition = surd_car(s, surd_cdr(s, exp));
      surd_value test = eval_loop(s, condition, env, 0);
      if (ISNIL(test)) { /* alternate */
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
        _RETURN NIL_VALUE;
      }
      _RETURN surd_make_closure(s, exp, env);
    }
    else if (surd_symbol_equal(s, car, s->DEF)) {
      if (!top) {
        die("def cannot be called from non-toplevel expression");
        _RETURN NIL_VALUE;
      }

      surd_value symbol = surd_car(s, surd_cdr(s, exp));
      surd_value value = surd_car(s, surd_cdr(s, surd_cdr(s, exp)));

      if (ISSYM(symbol)) {
        surd_value evaled = eval_loop(s, value, env, 0);
        _env_insert(s, s->top_env, symbol, evaled);
        _RETURN evaled;
      }
      else {
        die("def expected symbol as a second argument");
        _RETURN NIL_VALUE;
      }
    }
    else if (surd_symbol_equal(s, car, s->BEG)) {
      surd_value rest = CDR(exp);
      surd_value result = 0;
      while (!ISNIL(rest)) {
        result = eval_loop(s, CAR(rest), env, 0);
        rest = CDR(rest);
      }
      _RETURN result;
    }
    else {
      // apply
      surd_value op = eval_loop(s, car, env, 0);
      surd_value args = 0;
      if (!ISNIL(CDR(exp))) {
        surd_value list = CDR(exp);
        surd_value tmp = eval_loop(s, CAR(list), env, 0);
        surd_value next = 0;
        surd_value next_next = 0;
        surd_value evaled = 0;
        args = surd_cons(s, tmp, 0);
        next = args;
        tmp = CDR(list);
        while (!ISNIL(tmp)) {
          evaled = eval_loop(s, CAR(tmp), env, 0);
          next_next = surd_cons(s, evaled, 0);
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
      else if (ISCLOSURE(op)) {
        surd_value code = cell_of(op)->_value.closure.code;
        frame_t *cenv = cell_of(op)->_value.closure.env;
        env = _env_extend(s, cenv, surd_car(s, surd_cdr(s, code)), args);
        surd_value body_exprs = surd_cdr(s, surd_cdr(s, code));
        if (!ISNIL(CDR(body_exprs))) {
          exp = surd_cons(s, s->BEG, body_exprs);
        } else {
          exp = surd_car(s, body_exprs);
        }
        goto recur;
      }
      else {
        die("operator is not a procedure");
        _RETURN NIL_VALUE;
      }
    }
  }
  _RETURN NIL_VALUE;
}


surd_value
surd_eval(surd_t *s, surd_value exp, frame_t *env, int top)
{ PROFILE_START();

  _RETURN eval_loop(s, exp, env, top);
}

surd_value
surd_apply(surd_t *s, surd_value closure, surd_value args)
{ PROFILE_START();

  die("not implemented");
  _RETURN NIL_VALUE;
}

surd_value
surd_load(surd_t *s, FILE *in)
{ PROFILE_START();

  surd_value tmp;
  surd_value last = 0;
  while ((tmp = surd_read(s, in)), tmp != 0) {
    last = surd_eval(s, tmp, surd_env(s), 1);
  }
  _RETURN last;
}



static void
repl(surd_t *s)
{ PROFILE_START();

  int lc;
  surd_value val;
  for (lc = 0;; lc++) {
    printf("surd: %d> ", lc);
    val = surd_read(s, stdin);
    if (val) {
      val = surd_eval(s, val, surd_env(s), 1);
      printf("\n  #res:%d => ", lc);
      surd_display(s, stdout, val);
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
      surd->load_depth++;
      surd_load(surd, in);
      surd->load_depth--;
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
