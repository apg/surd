#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "surd.h"

static void
repl(surd_t *s)
{
  int lc;
  cell_t *cell;
  for (lc = 0;; lc++) {
    printf("surd: %d> ", lc);
    cell = surd_read(s, stdin);
    if (cell) {
      cell = surd_eval(s, cell, s->env, 1);
      printf("\n  #res:%d => ", lc);
      surd_display(s, stdout, cell);
      printf("\n");
    } 
    else {
      exit(0);
    }
  }
}

static void 
batch(surd_t *s, FILE *in)
{
  cell_t *tmp;
  while ((tmp = surd_read(s, in)) != NULL) {
    surd_eval(s, tmp, s->env, 1);
  }
}

int
main(int argc, char *argv[]) 
{
  FILE *in;
  surd_t surd;
  surd_init(&surd, 800, 2000);

  if (argc > 1) {
    in = fopen(argv[1], "r");
    if (in != NULL) {
      batch(&surd, in);
      fclose(in);
    }
    else {
      fprintf(stderr, "file (%s) could not be opened.\n", argv[1]);
      exit(EXIT_FAILURE);
    }
  }
  else {
    repl(&surd);
  }

  return 0;
}
