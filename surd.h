#ifdef _SURD_H
#define _SURD_H

#include <stdio.h>

typedef struct cell cell_t;
typedef struct cons cons_t;
typedef struct surd surd_t;

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

struct surd {
  cell_t *heap;
  int heap_size; // number of cells
  char **symbol_table;
  int symbol_table_size; // number of allocated symbols
  int symbol_table_index;
  cell_t *env;
  cell_t *nil;
};

void surd_init(surd_t *, int hs, int ss);
void surd_free(surd_t *);

cell_t *surd_new_cell(surd_t *); 
cell_t *surd_cons(surd_t *, cell_t *car, cell_t *cdr);

cell_t *surd_eval(surd_t *, cell_t *exp);
cell_t *surd_apply(surd_t *, cell_t *closure, cell_t *args);

int surd_gc(surd_t *);

void surd_num_init(surd_t *, cell_t *c, int value);
void surd_intern(surd_t *, char *value);

cell_t *surd_read(surd_t *, FILE *in);
cell_t *surd_write(surd_t *, FILE *out, cell_t *exp);

#endif
