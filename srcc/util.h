#ifndef UTIL_H_
#define UTIL_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "timer.h"

static const int64_t kRandSeed = 27491095;

static inline void PrintLabel(const char *label, const char *val) {
  char display[256];
  snprintf(display, sizeof(display), "%s:", label);
  printf("%-21s%7s\n", display, val);
}

static inline void PrintTime(const char *label, double seconds) {
  char display[256];
  snprintf(display, sizeof(display), "%s:", label);
  printf("%-21s%3.5lf\n", display, seconds);
}

static inline void PrintStepLabel(const char *label, int64_t count) {
  char display[256];
  snprintf(display, sizeof(display), "%s:", label);
  printf("%-14s%14" PRId64 "\n", display, count);
}

static inline void PrintStepTime(const char *label, double seconds, int64_t count) {
  char display[256];
  snprintf(display, sizeof(display), "%s", label);
  if (count != -1) {
    printf("%5s%11" PRId64 "  %10.5lf\n", display, count, seconds);
  } else {
    printf("%5s%23.5lf\n", display, seconds);
  }
}

static inline void PrintStepIndex(int step, double seconds, int64_t count) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", step);
  PrintStepTime(buffer, seconds, count);
}

#define TIME_PRINT(label, op)   \
  do {                          \
    Timer t_;                   \
    TimerStart(&t_);            \
    (op);                       \
    TimerStop(&t_);             \
    PrintTime((label), TimerSeconds(&t_)); \
  } while (0)

#endif  // UTIL_H_
