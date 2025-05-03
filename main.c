#include <time.h>
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
      cell = surd_eval(s, cell, surd_env(s), 1);
      printf("\n  #res:%d => ", lc);
      surd_display(s, stdout, cell);
      printf("\n");
    }
    else {
      exit(0);
    }
  }
}

int
main(int argc, char *argv[])
{
  struct timespec start, end;

  FILE *in;
  surd_t *surd = surd_init();

  if (argc > 1) {
    in = fopen(argv[1], "r");
    if (in != NULL) {
      clock_gettime(CLOCK_MONOTONIC, &start);
      surd_load(surd, in);

      clock_gettime(CLOCK_MONOTONIC, &end);
      long sec = end.tv_sec - start.tv_sec;
      long nsec = end.tv_nsec - start.tv_nsec;
      if (nsec < 0) {
        --sec;
        nsec += 1000000000;
      }
      fprintf(stderr, "ellapsed load time: %ld.%09ld seconds\n", sec, nsec);
      fclose(in);
    }
    else {
      fprintf(stderr, "file (%s) could not be opened.\n", argv[1]);
      exit(EXIT_FAILURE);
    }
  }
  else {
    repl(surd);
  }

  return 0;
}
