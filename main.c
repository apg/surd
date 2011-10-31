#include <stdio.h>
#include <stdlib.h>
#include "surd.h"

int
main(int argc, char *argv[]) 
{
  surd_t surd;
  surd_init(&surd, 100, 100);
  cell_t *cell = surd_read(&surd, stdin);

  //surd_display(&surd, stdout, cell);
  printf("- Read: ");
  surd_display(&surd, stdout, cell);
  printf("\n");

  cell = surd_eval(&surd, cell, (&surd)->env);
  printf("\n- Evaluated: ");
  surd_display(&surd, stdout, cell);
  printf("\n");

  //  surd_display(&surd, stdout, surd_eval(&surd, cell, (&surd)->env));

  return 0;
}
