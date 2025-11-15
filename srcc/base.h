#ifndef BASE_H_
#define BASE_H_

#include <stdint.h>

typedef int32_t NodeID;
typedef int32_t WeightT;

typedef struct {
  NodeID v;
  WeightT w;
} NodeWeight;

typedef struct {
  NodeID u;
  NodeID v;
} Edge;

typedef struct {
  NodeID u;
  NodeWeight v;
} WEdge;

typedef int32_t SGID;
typedef struct {
  SGID u;
  SGID v;
} SGEdge;
typedef int64_t SGOffset;

#endif  // BASE_H_
