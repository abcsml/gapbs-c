#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

#define MT19937_N 624
#define MT19937_M 397
#define MT19937_MATRIX_A 0x9908B0DFU
#define MT19937_UPPER_MASK 0x80000000U
#define MT19937_LOWER_MASK 0x7FFFFFFFU

typedef struct {
  uint32_t mt[MT19937_N];
  int index;
} MT19937;

static inline void MT19937Seed(MT19937 *state, uint32_t seed) {
  state->mt[0] = seed;
  for (int i = 1; i < MT19937_N; ++i) {
    state->mt[i] = (uint32_t)(1812433253U * (state->mt[i - 1] ^ (state->mt[i - 1] >> 30)) + i);
  }
  state->index = MT19937_N;
}

static inline void MT19937Twist(MT19937 *state) {
  for (int i = 0; i < MT19937_N; ++i) {
    uint32_t x = (state->mt[i] & MT19937_UPPER_MASK) +
                 (state->mt[(i + 1) % MT19937_N] & MT19937_LOWER_MASK);
    uint32_t xA = x >> 1;
    if (x & 0x1U)
      xA ^= MT19937_MATRIX_A;
    state->mt[i] = state->mt[(i + MT19937_M) % MT19937_N] ^ xA;
  }
  state->index = 0;
}

static inline uint32_t MT19937Next(MT19937 *state) {
  if (state->index >= MT19937_N)
    MT19937Twist(state);
  uint32_t y = state->mt[state->index++];
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9D2C5680U;
  y ^= (y << 15) & 0xEFC60000U;
  y ^= (y >> 18);
  return y;
}

static inline uint32_t RandBounded(MT19937 *rng, uint32_t max_inclusive) {
  uint64_t range = (uint64_t)max_inclusive + 1;
  if (range == 0)
    return MT19937Next(rng);
  uint32_t limit = UINT32_MAX - (uint32_t)(UINT32_MAX % range);
  uint32_t r;
  do {
    r = MT19937Next(rng);
  } while (r > limit);
  return (uint32_t)(r % range);
}

typedef struct {
  int scale;
  int degree;
  int64_t num_nodes;
  int64_t num_edges;
} Generator;

static inline void GeneratorInit(Generator *gen, int scale, int degree) {
  gen->scale = scale;
  gen->degree = degree;
  gen->num_nodes = (int64_t)1 << scale;
  gen->num_edges = gen->num_nodes * degree;
}

static inline void GeneratorPermuteIDs(Generator *gen, Vector *edges) {
  if (gen->num_nodes == 0)
    return;
  Vector permutation;
  VectorInit(&permutation, sizeof(NodeID));
  VectorResize(&permutation, (size_t)gen->num_nodes);
  NodeID *perm = (NodeID *)VectorData(&permutation);
  for (NodeID n = 0; n < gen->num_nodes; ++n)
    perm[n] = n;
  MT19937 rng;
  MT19937Seed(&rng, (uint32_t)kRandSeed);
  for (int64_t i = gen->num_nodes - 1; i > 0; --i) {
    uint32_t j = RandBounded(&rng, (uint32_t)i);
    NodeID tmp = perm[i];
    perm[i] = perm[j];
    perm[j] = tmp;
  }
  Edge *elist = (Edge *)VectorData(edges);
  size_t edge_count = VectorSize(edges);
  for (size_t e = 0; e < edge_count; ++e) {
    elist[e].u = perm[elist[e].u];
    elist[e].v = perm[elist[e].v];
  }
  VectorFree(&permutation);
}

static inline void GeneratorMakeUniformEL(Generator *gen, Vector *edges) {
  VectorInit(edges, sizeof(Edge));
  VectorResize(edges, (size_t)gen->num_edges);
  const int64_t block_size = 1 << 18;
  Edge *elist = (Edge *)VectorData(edges);
  for (int64_t block = 0; block < gen->num_edges; block += block_size) {
    MT19937 rng;
    MT19937Seed(&rng, (uint32_t)(kRandSeed + block / block_size));
    int64_t block_end = block + block_size;
    if (block_end > gen->num_edges)
      block_end = gen->num_edges;
    for (int64_t e = block; e < block_end; ++e) {
      elist[e].u = (NodeID)RandBounded(&rng, (uint32_t)(gen->num_nodes - 1));
      elist[e].v = (NodeID)RandBounded(&rng, (uint32_t)(gen->num_nodes - 1));
    }
  }
}

static inline void GeneratorMakeRMatEL(Generator *gen, Vector *edges) {
  const uint32_t max = UINT32_MAX;
  const uint32_t A = (uint32_t)(0.57 * max);
  const uint32_t B = (uint32_t)(0.19 * max);
  const uint32_t C = (uint32_t)(0.19 * max);
  VectorInit(edges, sizeof(Edge));
  VectorResize(edges, (size_t)gen->num_edges);
  const int64_t block_size = 1 << 18;
  Edge *elist = (Edge *)VectorData(edges);
  for (int64_t block = 0; block < gen->num_edges; block += block_size) {
    MT19937 rng;
    MT19937Seed(&rng, (uint32_t)(kRandSeed + block / block_size));
    int64_t block_end = block + block_size;
    if (block_end > gen->num_edges)
      block_end = gen->num_edges;
    for (int64_t e = block; e < block_end; ++e) {
      NodeID src = 0;
      NodeID dst = 0;
      for (int depth = 0; depth < gen->scale; ++depth) {
        uint32_t rand_point = MT19937Next(&rng);
        src <<= 1;
        dst <<= 1;
        if (rand_point < A + B) {
          if (rand_point > A)
            dst++;
        } else {
          src++;
          if (rand_point > A + B + C)
            dst++;
        }
      }
      elist[e].u = src;
      elist[e].v = dst;
    }
  }
  GeneratorPermuteIDs(gen, edges);
}

static inline void GeneratorGenerateEL(Generator *gen, int uniform, Vector *edges) {
  Timer t;
  TimerStart(&t);
  if (uniform)
    GeneratorMakeUniformEL(gen, edges);
  else
    GeneratorMakeRMatEL(gen, edges);
  TimerStop(&t);
  PrintTime("Generate Time", TimerSeconds(&t));
}

static inline void GeneratorInsertWeights(Vector *weighted_edges) {
  size_t edge_count = VectorSize(weighted_edges);
  if (edge_count == 0)
    return;
  const int64_t block_size = 1 << 18;
  WEdge *edges = (WEdge *)VectorData(weighted_edges);
  for (int64_t block = 0; block < (int64_t)edge_count; block += block_size) {
    MT19937 rng;
    MT19937Seed(&rng, (uint32_t)(kRandSeed + block / block_size));
    int64_t block_end = block + block_size;
    if (block_end > (int64_t)edge_count)
      block_end = (int64_t)edge_count;
    for (int64_t e = block; e < block_end; ++e) {
      WeightT weight = (WeightT)(RandBounded(&rng, 254) + 1);
      edges[e].v.w = weight;
    }
  }
}

#endif  // GENERATOR_H_
