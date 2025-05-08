#ifndef _SURD_H
#define _SURD_H

// Rob Pike says I shouldn't do this...
// http://doc.cat-v.org/bell_labs/pikestyle
#include <stdio.h>

typedef struct cell cell_t;
typedef struct surd surd_t;

surd_t *surd_init(void);
void surd_destroy(surd_t *);
cell_t *surd_env(surd_t *);

void surd_install_foreign(surd_t *, const char *name,
                          cell_t *(*func)(surd_t *, cell_t *), int arity);
cell_t *surd_new_cell(surd_t *);
cell_t *surd_cons(surd_t *, cell_t *car, cell_t *cdr);
int surd_list_length(surd_t *s, cell_t *c);
cell_t *surd_car(surd_t *, cell_t *cns);
cell_t *surd_cdr(surd_t *, cell_t *cns);
cell_t *surd_make_closure(surd_t *, cell_t *code, cell_t *env);
cell_t *surd_eval(surd_t *, cell_t *exp, cell_t *env, int top);
cell_t *surd_apply(surd_t *, cell_t *closure, cell_t *args);

void surd_num_init(surd_t *, cell_t *c, int value);
cell_t *surd_intern(surd_t *, const char *value);
int surd_symbol_equal(surd_t *, const cell_t *l, const cell_t *r);

int surd_is_nil(surd_t *, const cell_t *l);
int surd_is_true(surd_t *, const cell_t *l);
int surd_is_eof(surd_t *, const cell_t *l);
int surd_is_symbol(surd_t *s, const cell_t *t);
int surd_is_fixnum(surd_t *s, const cell_t *t);
int surd_is_string(surd_t *s, const cell_t *t);
int surd_is_cons(surd_t *s, const cell_t *t);
int surd_is_closure(surd_t *s, const cell_t *t);
int surd_is_primitive(surd_t *s, const cell_t *t);
int surd_is_foreign(surd_t *s, const cell_t *t);

int surd_as_int(surd_t *s, const cell_t *t, int *result);

cell_t *surd_read(surd_t *, FILE *in);
void surd_display(surd_t *, FILE *out, cell_t *exp);
void surd_write(surd_t *, FILE *out, cell_t *exp);

cell_t *surd_load(surd_t *, FILE *in);

#endif
