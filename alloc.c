#include <gc.h>

#include "surd.h"

cell_t *
surd_new_cell(surd_t *s)
{
  return GC_malloc(sizeof(cell_t));
}
