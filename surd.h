#ifndef _SURD_H
#define _SURD_H

// Rob Pike says I shouldn't do this... 
// http://doc.cat-v.org/bell_labs/pikestyle
#include <stdio.h>

typedef struct cell cell_t;
typedef struct cons cons_t;
typedef struct surd surd_t;
typedef struct symtab_entry symtab_entry_t;

typedef enum {
  TNIL=0x1,
  TFIXNUM=0x2,
  TSYMBOL=0x4,
  TCONS=0x8,
  TCLOSURE=0x10, // env goes in cdr, code in car
  TPRIMITIVE=0x20
} type_t;

#define TYPE_BITS 16

#define TATOMIC TFIXNUM | TSYMBOL | TNIL
#define MARK_BIT 31

struct cell {
  unsinged int flags;
  cell_t *hist;
  union {
    int num;
    struct cons {
      cell_t *car;
      cell_t *cdr;
    } cons;
    struct primitive {
      int arity;
      cell_t *(*func)(surd_t *, cell_t *);
    } primitive;
  } _value;
};

struct symtab_entry {
  char *name;
  cell_t *symbol;
};

struct surd {
  cell_t *heap;
  int heap_size; // number of cells
  cell_t *free_list;
  int free_list_cells; // cells free
  symtab_entry_t *symbol_table;
  int symbol_table_size; // number of allocated symbols
  int symbol_table_index;
  cell_t *env;
  cell_t *top_env;
  cell_t *nil;
};

void surd_init(surd_t *, int hs, int ss);
void surd_destroy(surd_t *);

void surd_install_primitive(surd_t *, char *name,
                            cell_t *(*func)(surd_t *, cell_t *), int arity);
cell_t *surd_new_cell(surd_t *); 
cell_t *surd_cons(surd_t *, cell_t *car, cell_t *cdr);
int surd_list_length(surd_t *s, cell_t *c);
cell_t *surd_car(surd_t *, cell_t *cns);
cell_t *surd_cdr(surd_t *, cell_t *cns);
cell_t *surd_make_closure(surd_t *, cell_t *code, cell_t *env);
cell_t *surd_eval(surd_t *, cell_t *exp, cell_t *env, int top);
cell_t *surd_apply(surd_t *, cell_t *closure, cell_t *args);

int surd_gc(surd_t *);

void surd_num_init(surd_t *, cell_t *c, int value);
cell_t *surd_intern(surd_t *, char *value);

cell_t *surd_read(surd_t *, FILE *in);
void surd_display(surd_t *, FILE *out, cell_t *exp);
void surd_write(surd_t *, FILE *out, cell_t *exp);

#define ISFIXNUM(c) (c != NULL && c->flags & TFIXNUM)
#define ISSYM(c) (c != NULL && c->flags & TSYMBOL)
#define ISCONS(c) (c != NULL && c->flags & TCONS)
#define ISCLOSURE(c) (c != NULL && c->flags & TCLOSURE)
#define ISPRIM(c) (c != NULL && c->flags & TPRIMITIVE)

#define CAR(c) (c->_value.cons.car)
#define CDR(c) (c->_value.cons.cdr)

#endif
