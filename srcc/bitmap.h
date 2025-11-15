#ifndef BITMAP_H_
#define BITMAP_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform_atomics.h"

typedef struct {
  uint64_t *start;
  size_t num_words;
} Bitmap;

static inline void BitmapInit(Bitmap *bitmap, size_t size) {
  bitmap->num_words = (size + 63) / 64;
  bitmap->start = (uint64_t *)calloc(bitmap->num_words, sizeof(uint64_t));
}

static inline void BitmapFree(Bitmap *bitmap) {
  free(bitmap->start);
  bitmap->start = NULL;
  bitmap->num_words = 0;
}

static inline void BitmapReset(Bitmap *bitmap) {
  memset(bitmap->start, 0, bitmap->num_words * sizeof(uint64_t));
}

static inline void BitmapSetBit(Bitmap *bitmap, size_t pos) {
  size_t word = pos / 64;
  size_t bit = pos & 63;
  bitmap->start[word] |= ((uint64_t)1ull << bit);
}

static inline void BitmapSetBitAtomic(Bitmap *bitmap, size_t pos) {
  size_t word = pos / 64;
  size_t bit = pos & 63;
  uint64_t mask = (uint64_t)1ull << bit;
  uint64_t old_val;
  uint64_t new_val;
  do {
    old_val = bitmap->start[word];
    new_val = old_val | mask;
  } while (!compare_and_swap_uint64(&bitmap->start[word], old_val, new_val));
}

static inline bool BitmapGetBit(const Bitmap *bitmap, size_t pos) {
  size_t word = pos / 64;
  size_t bit = pos & 63;
  return ((bitmap->start[word] >> bit) & 1ull) != 0ull;
}

static inline void BitmapSwap(Bitmap *a, Bitmap *b) {
  uint64_t *tmp_start = a->start;
  size_t tmp_words = a->num_words;
  a->start = b->start;
  a->num_words = b->num_words;
  b->start = tmp_start;
  b->num_words = tmp_words;
}

#endif  // BITMAP_H_
