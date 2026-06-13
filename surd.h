#ifndef _SURD_H
#define _SURD_H

// Rob Pike says I shouldn't do this...
// http://doc.cat-v.org/bell_labs/pikestyle
#include <stdio.h>
#include <stdint.h>

typedef uint64_t surd_value;
typedef struct cell cell_t;
typedef struct frame frame_t;
typedef struct surd surd_t;

surd_t *surd_init(void);
void surd_destroy(surd_t *);
frame_t *surd_env(surd_t *);

void surd_install_foreign(surd_t *, const char *name,
                          surd_value (*func)(surd_t *, surd_value), int arity);
surd_value surd_box(surd_t *, surd_value v);
surd_value surd_unbox(surd_t *, surd_value b);
surd_value surd_setbox(surd_t *, surd_value b, surd_value v);

surd_value surd_cons(surd_t *, surd_value car, surd_value cdr);

int surd_list_length(surd_t *s, surd_value c);
surd_value surd_car(surd_t *, surd_value cns);
surd_value surd_cdr(surd_t *, surd_value cns);
surd_value surd_make_closure(surd_t *, surd_value code, frame_t *env);
surd_value surd_eval(surd_t *, surd_value exp, frame_t *env, int top);
surd_value surd_apply(surd_t *, surd_value closure, surd_value args);
surd_value surd_make_port(surd_t *, FILE *p);

surd_value surd_fixnum(surd_t *s, int64_t value);
surd_value surd_nil(surd_t *s);
surd_value surd_intern(surd_t *, const char *value);
int surd_symbol_equal(surd_t *, surd_value l, surd_value r);

int surd_is_nil(surd_t *, surd_value l);
int surd_is_true(surd_t *, surd_value l);
int surd_is_eof(surd_t *, surd_value l);
int surd_is_box(surd_t *s, surd_value t);
int surd_is_symbol(surd_t *s, surd_value t);
int surd_is_fixnum(surd_t *s, surd_value t);
int surd_is_string(surd_t *s, surd_value t);
int surd_is_cons(surd_t *s, surd_value t);
int surd_is_closure(surd_t *s, surd_value t);
int surd_is_primitive(surd_t *s, surd_value t);
int surd_is_foreign(surd_t *s, surd_value t);

int surd_as_int(surd_t *s, surd_value t, int64_t *result);

surd_value surd_read(surd_t *, FILE *in);
void surd_display(surd_t *, FILE *out, surd_value exp);
void surd_write(surd_t *, FILE *out, surd_value exp);

surd_value surd_load(surd_t *, FILE *in);

#endif
