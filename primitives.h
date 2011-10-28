#ifndef _SURD_PRIMITIVES_H
#define _SURD_PRIMITIVES_H

// list selection
cell_t *surd_p_first(surd_t *s, cell_t *args);
cell_t *surd_p_rest(surd_t *s, cell_t *args);
cell_t *surd_p_second(surd_t *s, cell_t *args);
cell_t *surd_p_third(surd_t *s, cell_t *args);
cell_t *surd_p_fourth(surd_t *s, cell_t *args);
cell_t *surd_p_fifth(surd_t *s, cell_t *args);
cell_t *surd_p_nth(surd_t *s, cell_t *args);

// predicates
cell_t *surd_p_consp(surd_t *s, cell_t *args);
cell_t *surd_p_nilp(surd_t *s, cell_t *args);
cell_t *surd_p_fixnump(surd_t *s, cell_t *args);
cell_t *surd_p_symbolp(surd_t *s, cell_t *args);
cell_t *surd_p_procedurep(surd_t *s, cell_t *args);
cell_t *surd_p_closurep(surd_t *s, cell_t *args);

// basic arithmetic
cell_t *surd_p_plus(surd_t *s, cell_t *args);
cell_t *surd_p_minus(surd_t *s, cell_t *args);
cell_t *surd_p_mult(surd_t *s, cell_t *args);
cell_t *surd_p_div(surd_t *s, cell_t *args);
cell_t *surd_p_mod(surd_t *s, cell_t *args);

// display
cell_t *surd_p_display(surd_t *s, cell_t *args);
#endif
