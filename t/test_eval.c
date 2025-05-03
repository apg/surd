#include <stdio.h>
#include <stdlib.h>
#include "../surd.h"
#include "tests.h"


void
test_eval_fact(surd_t *s)
{
  int actual[] = {
    1, 2, 120, 3628800
  };
  int nums_to_check = 4;
  int i = 0;
  FILE *in;
  cell_t *c;
  cell_t *current;

  in = fopen("code/eval_fact.surd", "r");
  if (in) {
    c = surd_load(s, in);
    IS(!surd_is_nil(s, c), "eval returned nil?");
    while (!surd_is_nil(s, c)) {
      IS(i < nums_to_check, "read more numbers than expected");
      current = surd_car(s, c);
      IS(surd_is_fixnum(s, current), "not a fixnum -- predicate");
      int value = 0;
      IS(surd_as_int(s, current, &value), "not a fixnum -- as_int");
      ISEQ(value, actual[i], "value not the same as actual");
      i++;
      c = surd_cdr(s, c);
    }
    fclose(in);
  }
}

int
main(int argc, char *argv[])
{
  surd_t *s = surd_init();
  test_eval_fact(s);
  surd_destroy(s);

  report_results(argv[0]);

  return 0;
}
