#ifndef _SURD_H
#define _SURD_H

#include <stdio.h>

typedef struct cell cell_t;
typedef struct cons cons_t;
typedef struct surd surd_t;
typedef struct symtab_entry symtab_entry_t;

typedef enum {
  TFIXNUM=2,
  TSYMBOL=4,
  TCONS=8,
  TCLOSURE=16, // env goes in cdr, code in car
  TPRIMITIVE=32
} type_t;

struct cell {
  int flags;
  union {
    int num;
    struct cons {
      cell_t *car;
      cell_t *cdr;
    } cons;
  } _value;
};

struct symtab_entry {
  char *name;
  cell_t *symbol;
};

struct surd {
  cell_t *heap;
  int heap_size; // number of cells
  symtab_entry_t *symbol_table;
  int symbol_table_size; // number of allocated symbols
  int symbol_table_index;
  cell_t *env;
  cell_t *nil;
};

void surd_init(surd_t *, int hs, int ss);
void surd_free(surd_t *);

cell_t *surd_new_cell(surd_t *); 
cell_t *surd_cons(surd_t *, cell_t *car, cell_t *cdr);
cell_t *surd_make_closure(surd_t *, cell_t *code, cell_t *env);
cell_t *surd_eval(surd_t *, cell_t *exp, cell_t *env);
cell_t *surd_apply(surd_t *, cell_t *closure, cell_t *args);

int surd_gc(surd_t *);

void surd_num_init(surd_t *, cell_t *c, int value);
cell_t *surd_intern(surd_t *, char *value);

cell_t *surd_read(surd_t *, FILE *in);
void surd_display(surd_t *, FILE *out, cell_t *exp);
void surd_write(surd_t *, FILE *out, cell_t *exp);

#define ISFIXNUM(c) (c->flags & TFIXNUM)
#define ISSYM(c) (c->flags & TSYMBOL)
#define ISCONS(c) (c->flags & TCONS)
#define ISCLOSURE(c) (c->flags & TCLOSURE)
#define ISPRIM(c) (c->flags & TPRIMITIVE)

#define CAR(c) (c->_value.cons.car)
#define CDR(c) (c->_value.cons.cdr)

#endif
