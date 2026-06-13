#include <stdio.h>
#include <stdlib.h>
#include "../surd.h"
#include "tests.h"


void
test_read_nil(surd_t *s)
{
  FILE *in;
  surd_value c;

  in = fopen("code/reader_nil.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(surd_is_nil(s, c), "read didn't return nil :(");
    fclose(in);
  }
}


void
test_read_fixnums(surd_t *s)
{
  int64_t actual[] = {
    1, 2, 100, -390, -9, 1, 2, 123456789
  };
  int nums_to_check = 8;
  int i = 0;
  FILE *in;
  surd_value c;
  surd_value current;

  in = fopen("code/reader_fixnums.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(!surd_is_nil(s, c), "read returned nil :(");
    while (!surd_is_nil(s, c)) {
      IS(i < nums_to_check, "read more numbers than expected");
      current = surd_car(s, c);
      IS(surd_is_fixnum(s, current), "not a fixnum -- predicate");
      int64_t value = 0;
      IS(surd_as_int(s, current, &value), "couldn't get fixnum value");
      ISEQ(value, actual[i], "fixnums: value not the same as actual ");
      i++;
      c = surd_cdr(s, c);
    }
    fclose(in);
  }
}

void
test_read_strings(surd_t *s)
{
  int strs_to_check = 1;
  int i = 0;
  FILE *in;
  surd_value c;
  surd_value current;

  in = fopen("code/reader_strings.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(!surd_is_nil(s, c), "read returned nil :(");
    while (!surd_is_nil(s, c)) {
      IS(i < strs_to_check, "read more strings than expected");
      current = surd_car(s, c);
      IS(!surd_is_string(s, current), "not a string");
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
    "*bar*",
    "*baz?",
    "baz?",
    "genius",
    "?genius?",
    "gul...f"
  };
  int syms_to_check = 10;
  int i = 0;
  FILE *in;
  surd_value c;
  surd_value current, sym;

  in = fopen("code/reader_symbols.surd", "r");
  if (in) {
    c = surd_read(s, in);
    IS(!surd_is_nil(s, c), "read returned nil :(");
    while (!surd_is_nil(s, c)) {
      IS(i < syms_to_check, "read more symbols than expected");
      current = surd_car(s, c);
      sym = surd_intern(s, actual[i]);
      IS(surd_is_symbol(s, current), "not a symbol");
      IS(surd_symbol_equal(s, current, sym), "symbols are not equal");
      i++;
      c = surd_cdr(s, c);
    }
    fclose(in);
  }
}



int
main(int argc, char *argv[])
{
  surd_t *s;

  // read null
  s = surd_init();
  test_read_nil(s);
  surd_destroy(s);

  // read symbols
  s = surd_init();
  test_read_symbols(s);
  surd_destroy(s);

  // read integers
  s = surd_init();
  test_read_fixnums(s);
  surd_destroy(s);

  report_results(argv[0]);

  return 0;
}
