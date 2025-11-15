#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <time.h>

typedef struct {
  struct timespec start_time;
  struct timespec end_time;
} Timer;

static inline void TimerStart(Timer *timer) {
  clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
  timer->end_time = timer->start_time;
}

static inline void TimerStop(Timer *timer) {
  clock_gettime(CLOCK_MONOTONIC, &timer->end_time);
}

static inline double TimerSeconds(const Timer *timer) {
  double start_sec = (double)timer->start_time.tv_sec +
                     (double)timer->start_time.tv_nsec / 1e9;
  double end_sec = (double)timer->end_time.tv_sec +
                   (double)timer->end_time.tv_nsec / 1e9;
  return end_sec - start_sec;
}

static inline double TimerMillisecs(const Timer *timer) {
  return TimerSeconds(timer) * 1e3;
}

static inline double TimerMicrosecs(const Timer *timer) {
  return TimerSeconds(timer) * 1e6;
}

#define TIME_OP(timer_ptr, op) \
  do {                         \
    TimerStart((timer_ptr));   \
    (op);                      \
    TimerStop((timer_ptr));    \
  } while (0)

#endif  // TIMER_H_
