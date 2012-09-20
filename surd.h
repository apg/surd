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
  TNIL=0x0,
  TFIXNUM=0x1,
  TSYMBOL=0x2,
  TCONS=0x4,
  TSTRING=0x8,
  TCLOSURE=0x10, // env goes in cdr, code in car
  TPRIMITIVE=0x20,
  TMACRO=0x40
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
  /* Symbol table: symbols use cells as well as external memory
     created on the fly via malloc 
  */
  symtab_entry_t *symbol_table;

  /* size of symbol table */
  int symbol_table_size; 

  /* index of the next free symbol */
  int symbol_table_index;

  /* initial eval environment */
  cell_t *env; 

  /* Top level environment */
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
cell_t *surd_eval(surd_t *, cell_t *exp, cell_t *env);
cell_t *surd_apply(surd_t *, cell_t *closure, cell_t *args);

void surd_num_init(surd_t *, cell_t *c, int value);
cell_t *surd_intern(surd_t *, char *value);

cell_t *surd_read(surd_t *, FILE *in);
void surd_display(surd_t *, FILE *out, cell_t *exp);
void surd_write(surd_t *, FILE *out, cell_t *exp);

#define ISNIL(c) (c == (cell_t *)0)
#define ISFIXNUM(c) (c != NULL && !ISNIL(c) && c->flags & TFIXNUM)
#define ISSYM(c) (c != NULL && !ISNIL(c) && c->flags & TSYMBOL)
#define ISCONS(c) (c != NULL && !ISNIL(c) && c->flags & TCONS)
#define ISCLOSURE(c) (c != NULL && !ISNIL(c) && c->flags & TCLOSURE)
#define ISPRIM(c) (c != NULL && !ISNIL(c) && c->flags & TPRIMITIVE)

#define CAR(c) (c->_value.cons.car)
#define CDR(c) (c->_value.cons.cdr)

#endif
