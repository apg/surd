#ifndef _QUICK_TESTS_H
#define _QUICK_TESTS_H

#include <stdio.h>

static int FAILURES = 0;
static int SUCCESSES = 0;

#define IS(x, z) do{if ((x)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISEQ(x, y, z) do{if ((x) == (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISNE(x, y, z) do{if ((x) != (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISGT(x, y, z) do{if ((x) > (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISGE(x, y, z) do{if ((x) >= (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISLT(x, y, z) do{if ((x) < (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

#define ISLE(x, y, z) do{if ((x) <= (y)) { SUCCESSES++;} \
    else { FAILURES++; fprintf(stderr, "  FAILURE: %s failed", z); } \
  } while(0);

void
report_results(char *name) 
{
  fprintf(stderr, "%s\n  Results: %d / %d, %d failures.\n",
          name, SUCCESSES, SUCCESSES + FAILURES, FAILURES);
  if (FAILURES) {
    exit(1);
  }
  else {
    exit(0);
  }
}

#endif
