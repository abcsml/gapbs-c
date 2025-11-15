#ifndef PLATFORM_ATOMICS_H_
#define PLATFORM_ATOMICS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if defined(_OPENMP) || defined(__GNUC__)
#define ATOMIC_USE_INTRINSICS 1
#endif

static inline int32_t fetch_and_add_int32(volatile int32_t *x, int32_t inc) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_fetch_and_add(x, inc);
#else
  int32_t orig = *x;
  *x += inc;
  return orig;
#endif
}

static inline int64_t fetch_and_add_int64(volatile int64_t *x, int64_t inc) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_fetch_and_add(x, inc);
#else
  int64_t orig = *x;
  *x += inc;
  return orig;
#endif
}

static inline uint32_t fetch_and_add_uint32(volatile uint32_t *x, uint32_t inc) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_fetch_and_add(x, inc);
#else
  uint32_t orig = *x;
  *x += inc;
  return orig;
#endif
}

static inline uint64_t fetch_and_add_uint64(volatile uint64_t *x, uint64_t inc) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_fetch_and_add(x, inc);
#else
  uint64_t orig = *x;
  *x += inc;
  return orig;
#endif
}

static inline size_t fetch_and_add_size(volatile size_t *x, size_t inc) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_fetch_and_add(x, inc);
#else
  size_t orig = *x;
  *x += inc;
  return orig;
#endif
}

static inline bool compare_and_swap_int32(volatile int32_t *x, int32_t old_val, int32_t new_val) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_bool_compare_and_swap(x, old_val, new_val);
#else
  if (*x == old_val) {
    *x = new_val;
    return true;
  }
  return false;
#endif
}

static inline bool compare_and_swap_int64(volatile int64_t *x, int64_t old_val, int64_t new_val) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_bool_compare_and_swap(x, old_val, new_val);
#else
  if (*x == old_val) {
    *x = new_val;
    return true;
  }
  return false;
#endif
}

static inline bool compare_and_swap_uint32(volatile uint32_t *x, uint32_t old_val, uint32_t new_val) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_bool_compare_and_swap(x, old_val, new_val);
#else
  if (*x == old_val) {
    *x = new_val;
    return true;
  }
  return false;
#endif
}

static inline bool compare_and_swap_uint64(volatile uint64_t *x, uint64_t old_val, uint64_t new_val) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_bool_compare_and_swap(x, old_val, new_val);
#else
  if (*x == old_val) {
    *x = new_val;
    return true;
  }
  return false;
#endif
}

static inline bool compare_and_swap_size(volatile size_t *x, size_t old_val, size_t new_val) {
#ifdef ATOMIC_USE_INTRINSICS
  return __sync_bool_compare_and_swap(x, old_val, new_val);
#else
  if (*x == old_val) {
    *x = new_val;
    return true;
  }
  return false;
#endif
}

#undef ATOMIC_USE_INTRINSICS

#endif  // PLATFORM_ATOMICS_H_
