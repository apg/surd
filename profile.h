#ifndef SIMPLE_PROFILER_H
#define SIMPLE_PROFILER_H

#ifdef PROFILE

#include <time.h>

#define MAX_PROFILED_FUNCTIONS 1000

typedef struct {
  const char *name;
  long calls;
  double total_time_sec;
} __Profile_Entry;

static __Profile_Entry __Profiler_Data[MAX_PROFILED_FUNCTIONS];
static int __Profiler_Count = 0;

static int
__find_or_add_entry(const char *name) {
  for (int i = 0; i < __Profiler_Count; i++) {
    if (strcmp(__Profiler_Data[i].name, name) == 0) return i;
  }
  if (__Profiler_Count < MAX_PROFILED_FUNCTIONS) {
    __Profiler_Data[__Profiler_Count].name = name;
    return __Profiler_Count++;
  }
  return -1; // Too many functions
}

#define PROFILE_START() \
  struct timespec __start, __end;               \
  double __elapsed = 0.0;                       \
  int __idx = __find_or_add_entry(__func__);      \
  clock_gettime(CLOCK_MONOTONIC, &__start);

#define _RETURN \
    clock_gettime(CLOCK_MONOTONIC, &__end); \
    __elapsed = (__end.tv_sec - __start.tv_sec) + \
                       (__end.tv_nsec - __start.tv_nsec) / 1e9; \
    if (__idx >= 0) { \
        __Profiler_Data[__idx].calls++; \
        __Profiler_Data[__idx].total_time_sec += __elapsed; \
    } \
    return

static void print_profiler_report(void) {
    printf("\n=== Profile Report ===\n");
    for (int i = 0; i < __Profiler_Count; i++) {
        printf("%s: %ld call(s), %.6f sec total\n",
               __Profiler_Data[i].name,
               __Profiler_Data[i].calls,
               __Profiler_Data[i].total_time_sec);
    }
    printf("======================\n");
}

#else

#define PROFILE_START()
#define _RETURN return
static void print_profiler_report(void) {}

#endif

#endif // SIMPLE_PROFILER_H
