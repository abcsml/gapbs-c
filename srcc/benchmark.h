#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "command_line.h"
#include "graph.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

typedef struct {
  NodeID key;
  double value;
} KeyValuePair;

typedef struct {
  double value;
  NodeID key;
} ValueKeyPair;

typedef struct {
  const Graph *graph;
  NodeID given_source;
  uint64_t rng_state;
} SourcePicker;

typedef struct {
  const WGraph *graph;
  NodeID given_source;
  uint64_t rng_state;
} WSourcePicker;

static inline uint64_t PickerRand(uint64_t *state) {
  uint64_t x = *state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *state = x;
  return x * UINT64_C(2685821657736338717);
}

static inline void SourcePickerInit(SourcePicker *picker,
                                    const Graph *graph,
                                    NodeID given_source) {
  picker->graph = graph;
  picker->given_source = given_source;
  picker->rng_state = (uint64_t)kRandSeed;
}

static inline NodeID SourcePickerPickNext(SourcePicker *picker) {
  if (picker->given_source != -1)
    return picker->given_source;
  NodeID source;
  int64_t num_nodes = GraphNumNodes(picker->graph);
  if (num_nodes <= 0)
    return -1;
  do {
    source = (NodeID)(PickerRand(&picker->rng_state) % (uint64_t)num_nodes);
  } while (GraphOutDegree(picker->graph, source) == 0);
  return source;
}

static inline void WSourcePickerInit(WSourcePicker *picker,
                                     const WGraph *graph,
                                     NodeID given_source) {
  picker->graph = graph;
  picker->given_source = given_source;
  picker->rng_state = (uint64_t)kRandSeed;
}

static inline NodeID WSourcePickerPickNext(WSourcePicker *picker) {
  if (picker->given_source != -1)
    return picker->given_source;
  NodeID source;
  int64_t num_nodes = GraphNumNodesW(picker->graph);
  if (num_nodes <= 0)
    return -1;
  do {
    source = (NodeID)(PickerRand(&picker->rng_state) % (uint64_t)num_nodes);
  } while (WGraphOutDegree(picker->graph, source) == 0);
  return source;
}

static inline void TopKInsert(Vector *topk, const ValueKeyPair *candidate, size_t k) {
  size_t size = VectorSize(topk);
  ValueKeyPair *data = (ValueKeyPair *)VectorData(topk);
  size_t pos = 0;
  while (pos < size && data[pos].value > candidate->value)
    pos++;
  if (size < k) {
    VectorResize(topk, size + 1);
    data = (ValueKeyPair *)VectorData(topk);
    for (size_t i = size; i > pos; --i)
      data[i] = data[i - 1];
    data[pos] = *candidate;
  } else if (k > 0 && pos < k) {
    for (size_t i = k - 1; i > pos; --i)
      data[i] = data[i - 1];
    data[pos] = *candidate;
  }
  if (VectorSize(topk) > k)
    VectorResize(topk, k);
}

static inline void ComputeTopK(const KeyValuePair *pairs, size_t count,
                               size_t k, Vector *result) {
  VectorInit(result, sizeof(ValueKeyPair));
  VectorResize(result, 0);
  for (size_t i = 0; i < count; ++i) {
    ValueKeyPair candidate;
    candidate.value = pairs[i].value;
    candidate.key = pairs[i].key;
    size_t current = VectorSize(result);
    if (current < k) {
      TopKInsert(result, &candidate, k);
    } else if (current > 0) {
      ValueKeyPair *data = (ValueKeyPair *)VectorData(result);
      if (candidate.value > data[current-1].value)
        TopKInsert(result, &candidate, k);
    }
  }
}

static inline int VerifyUnimplemented(void) {
  printf("** verify unimplemented **\n");
  return 0;
}

#endif  // BENCHMARK_H_
