#include <stdio.h>
#include <stdlib.h>
#include "../surd.h"
#include "tests.h"


void
test_read_nil(surd_t *s)
{
  FILE *in;
  cell_t *c;

  in = fopen("code/reader_nil.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(c == s->nil, "read didn't return nil :(");
    fclose(in);
  }
}


void
test_read_fixnums(surd_t *s)
{
  int actual[] = {
    1, 2, 100, -390, -9, 1, 2, 123456789
  };
  int nums_to_check = 8;
  int i = 0;
  FILE *in;
  cell_t *c;
  cell_t *current;

  in = fopen("code/reader_fixnums.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(c != s->nil, "read returned nil :(");
    while (c != s->nil) {
      IS(i < nums_to_check, "read more numbers than expected");
      current = surd_car(s, c);
      IS(ISFIXNUM(current), "not a fixnum");
      ISEQ(current->_value.num, actual[i], "value not the same as actual");
      i++;
      c = surd_cdr(s, c);
    }
    fclose(in);
  }
}


void
test_read_symbols(surd_t *s)
{
  char *actual[] = {
    "+",
    "-",
    ".+",
    "*foo",
    "*bar",
    "*baz?",
    "baz?",
    "genius",
    "?genius?",
    "gul...f"
  };
  int syms_to_check = 10;
  int i = 0;
  FILE *in;
  cell_t *c;
  cell_t *current, *sym;

  in = fopen("code/reader_symbols.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(c != s->nil, "read returned nil :(");
    while (c != s->nil) {
      IS(i < syms_to_check, "read more symbols than expected");
      current = surd_car(s, c);
      sym = surd_intern(s, actual[i]);
      IS(ISSYM(current), "not a symbol");
      ISEQ(current->_value.num, sym->_value.num, "value not the same as actual");
      i++;
      c = surd_cdr(s, c);
    }
    fclose(in);
  }
}



int
main(int argc, char *argv[])
{
  surd_t s;
  // read null
  surd_init(&s, 100, 100);
  test_read_nil(&s);
  surd_destroy(&s);

  // read symbols
  surd_init(&s, 100, 100);
  test_read_symbols(&s);
  surd_destroy(&s);

  // read integers
  surd_init(&s, 100, 100);
  test_read_fixnums(&s);
  surd_destroy(&s);

  report_results(argv[0]);

  return 0;
}
