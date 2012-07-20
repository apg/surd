#include <stdio.h>
#include <stdlib.h>
#include "surd.h"

int
main(int argc, char *argv[]) 
{
  surd_t surd;
  surd_init(&surd, 1000, 1000);
  cell_t *cell;
  int lc;

  //surd_display(&surd, stdout, cell);
  for (lc = 0;; lc++) {
    printf("surd: %d> ", lc);
    cell = surd_read(&surd, stdin);
    if (cell) {
      cell = surd_eval(&surd, cell, (&surd)->env, 1);
      printf("\n  #res:%d => ", lc);
      surd_display(&surd, stdout, cell);
      printf("\n");
    } 
    else {
      exit(0);
    }
  }

  return 0;
}
