#include <stdio.h>
#include <stdlib.h>
#include "surd.h"

int
main(int argc, char *argv[]) 
{
  surd_t surd;
  surd_init(&surd, 100, 100);
  cell_t *cell;
  int lc;

  //surd_display(&surd, stdout, cell);
  for (lc = 0;; lc++) {
    printf("surd: %d> ", lc);
    cell = surd_read(&surd, stdin);
    cell = surd_eval(&surd, cell, (&surd)->env, 1);
    printf("result %d: ", lc);
    surd_display(&surd, stdout, cell);
    printf("\n");
  }

  return 0;
}
