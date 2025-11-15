#ifndef VECTOR_H_
#define VECTOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  unsigned char *data;
  size_t size;
  size_t capacity;
  size_t elem_size;
  bool owns_data;
} Vector;

static inline void VectorInit(Vector *vec, size_t elem_size) {
  vec->data = NULL;
  vec->size = 0;
  vec->capacity = 0;
  vec->elem_size = elem_size;
  vec->owns_data = true;
}

static inline void VectorFree(Vector *vec) {
  if (vec->owns_data) {
    free(vec->data);
  }
  vec->data = NULL;
  vec->size = 0;
  vec->capacity = 0;
  vec->owns_data = true;
}

static inline void VectorReserve(Vector *vec, size_t new_capacity) {
  if (new_capacity <= vec->capacity)
    return;
  if (!vec->owns_data) {
    unsigned char *new_copy = (unsigned char *)malloc(vec->size * vec->elem_size);
    if (new_copy == NULL)
      abort();
    memcpy(new_copy, vec->data, vec->size * vec->elem_size);
    vec->data = new_copy;
    vec->owns_data = true;
  }
  unsigned char *new_data = (unsigned char *)realloc(vec->data, new_capacity * vec->elem_size);
  if (new_data == NULL) {
    abort();
  }
  vec->data = new_data;
  vec->capacity = new_capacity;
}

static inline void VectorResize(Vector *vec, size_t new_size) {
  if (new_size > vec->capacity) {
    size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity;
    while (new_capacity < new_size)
      new_capacity *= 2;
    VectorReserve(vec, new_capacity);
  }
  if (new_size > vec->size) {
    size_t delta = new_size - vec->size;
    memset(vec->data + vec->size * vec->elem_size, 0, delta * vec->elem_size);
  }
  vec->size = new_size;
}

static inline void VectorClear(Vector *vec) {
  vec->size = 0;
}

static inline int VectorEmpty(const Vector *vec) {
  return vec->size == 0;
}

static inline int VectorIsEmpty(const Vector *vec) {
  return vec->size == 0;
}

static inline size_t VectorSize(const Vector *vec) {
  return vec->size;
}

static inline void *VectorData(const Vector *vec) {
  return vec->data;
}

static inline void *VectorBegin(const Vector *vec) {
  return vec->data;
}

static inline void *VectorEnd(const Vector *vec) {
  return vec->data + vec->size * vec->elem_size;
}

static inline void VectorPushBack(Vector *vec, const void *value) {
  if (vec->size == vec->capacity) {
    size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;
    VectorReserve(vec, new_capacity);
  }
  memcpy(vec->data + vec->size * vec->elem_size, value, vec->elem_size);
  vec->size += 1;
}

static inline void VectorFill(Vector *vec, const void *value) {
  for (size_t i = 0; i < vec->size; ++i) {
    memcpy(vec->data + i * vec->elem_size, value, vec->elem_size);
  }
}

static inline void VectorSwap(Vector *a, Vector *b) {
  Vector tmp = *a;
  *a = *b;
  *b = tmp;
}

static inline void VectorLeak(Vector *vec) {
  vec->data = NULL;
  vec->size = 0;
  vec->capacity = 0;
  vec->owns_data = false;
}

#define VECTOR_AT(vec, type, idx) (((type *)((vec)->data))[idx])

#endif  // VECTOR_H_
