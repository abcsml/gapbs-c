#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "benchmark.h"
#include "bitmap.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "sliding_queue.h"
#include "util.h"
#include "vector.h"

static inline uint64_t Rand64(uint64_t *state) {
  uint64_t x = *state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *state = x;
  return x * UINT64_C(2685821657736338717);
}

static void Link(NodeID u, NodeID v, NodeID *comp) {
  NodeID p1 = comp[u];
  NodeID p2 = comp[v];
  while (p1 != p2) {
    NodeID high = p1 > p2 ? p1 : p2;
    NodeID low = p1 + p2 - high;
    NodeID p_high = comp[high];
    if (p_high == low) {
      break;
    } else if (p_high == high) {
      comp[high] = low;
      break;
    }
    p1 = comp[p_high];
    p2 = comp[low];
  }
}

static void Compress(NodeID *comp, int64_t num_nodes) {
  for (NodeID n = 0; n < num_nodes; ++n) {
    while (comp[n] != comp[comp[n]]) {
      comp[n] = comp[comp[n]];
    }
  }
}

static NodeID SampleFrequentElement(const NodeID *comp, int64_t length,
                                    bool logging_enabled,
                                    int64_t num_samples) {
  if (length == 0)
    return -1;
  if (num_samples > length)
    num_samples = length;
  uint64_t state = kRandSeed;
  int *counts = (int *)calloc((size_t)length, sizeof(int));
  if (counts == NULL) {
    printf("Allocation failure in SampleFrequentElement\n");
    exit(EXIT_FAILURE);
  }
  for (int64_t i = 0; i < num_samples; ++i) {
    NodeID idx = (NodeID)(Rand64(&state) % (uint64_t)length);
    counts[comp[idx]]++;
  }
  NodeID most_frequent = 0;
  int best_count = 0;
  for (NodeID label = 0; label < length; ++label) {
    if (counts[label] > best_count) {
      best_count = counts[label];
      most_frequent = label;
    }
  }
  if (logging_enabled && num_samples > 0) {
    float frac = (float)best_count / (float)num_samples;
    printf("Skipping largest intermediate component (ID: %d, approx. %d%% of the graph)\n",
           most_frequent, (int)(frac * 100.0f));
  }
  free(counts);
  return most_frequent;
}

static NodeID *Afforest(const Graph *g, bool logging_enabled,
                        int32_t neighbor_rounds) {
  int64_t num_nodes = GraphNumNodes(g);
  NodeID *comp = (NodeID *)malloc(sizeof(NodeID) * (size_t)num_nodes);
  if (comp == NULL && num_nodes > 0) {
    printf("Allocation failure in Afforest\n");
    exit(EXIT_FAILURE);
  }
  for (NodeID n = 0; n < num_nodes; ++n)
    comp[n] = n;
  for (int r = 0; r < neighbor_rounds; ++r) {
    for (NodeID u = 0; u < num_nodes; ++u) {
      NodeID *neighbor = GraphOutNeighborsOffset(g, u, (size_t)r);
      NodeID *end = GraphOutNeighborsEnd(g, u);
      if (neighbor < end) {
        Link(u, *neighbor, comp);
      }
    }
    Compress(comp, num_nodes);
  }
  NodeID c = SampleFrequentElement(comp, num_nodes, logging_enabled, 1024);
  if (!GraphDirected(g)) {
    for (NodeID u = 0; u < num_nodes; ++u) {
      if (comp[u] == c)
        continue;
      NodeID *begin = GraphOutNeighborsOffset(g, u, (size_t)neighbor_rounds);
      NodeID *end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = begin; nbr < end; ++nbr)
        Link(u, *nbr, comp);
    }
  } else {
    for (NodeID u = 0; u < num_nodes; ++u) {
      if (comp[u] == c)
        continue;
      NodeID *begin = GraphOutNeighborsOffset(g, u, (size_t)neighbor_rounds);
      NodeID *end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = begin; nbr < end; ++nbr)
        Link(u, *nbr, comp);
      NodeID *in_begin = GraphInNeighborsBegin(g, u);
      NodeID *in_end = GraphInNeighborsEnd(g, u);
      for (NodeID *nbr = in_begin; nbr < in_end; ++nbr)
        Link(u, *nbr, comp);
    }
  }
  Compress(comp, num_nodes);
  return comp;
}

static void PrintCompStats(const Graph *g, const NodeID *comp) {
  int64_t num_nodes = GraphNumNodes(g);
  if (num_nodes == 0) {
    printf("Graph is empty\n");
    return;
  }
  NodeID *counts = (NodeID *)calloc((size_t)num_nodes, sizeof(NodeID));
  if (counts == NULL)
    return;
  for (NodeID n = 0; n < num_nodes; ++n)
    counts[comp[n]] += 1;
  Vector count_vector;
  VectorInit(&count_vector, sizeof(KeyValuePair));
  for (NodeID label = 0; label < num_nodes; ++label) {
    if (counts[label] > 0) {
      KeyValuePair kvp;
      kvp.key = label;
      kvp.value = (double)counts[label];
      VectorPushBack(&count_vector, &kvp);
    }
  }
  Vector top_k;
  ComputeTopK((const KeyValuePair *)VectorData(&count_vector),
              VectorSize(&count_vector), 5, &top_k);
  size_t actual_k = VectorSize(&top_k);
  printf("%zu biggest clusters\n", actual_k);
  ValueKeyPair *top = (ValueKeyPair *)VectorData(&top_k);
  for (size_t i = 0; i < actual_k; ++i)
    printf("%d:%d\n", top[i].key, (int)top[i].value);
  size_t num_components = 0;
  for (NodeID label = 0; label < num_nodes; ++label)
    if (counts[label] > 0)
      num_components++;
  printf("There are %zu components\n", num_components);
  VectorFree(&top_k);
  VectorFree(&count_vector);
  free(counts);
}

static bool CCVerifier(const Graph *g, const NodeID *comp) {
  int64_t num_nodes = GraphNumNodes(g);
  if (num_nodes == 0)
    return true;
  Bitmap visited;
  BitmapInit(&visited, (size_t)num_nodes);
  BitmapReset(&visited);
  char *label_chosen = (char *)calloc((size_t)num_nodes, sizeof(char));
  if (label_chosen == NULL)
    return false;
  Vector frontier;
  VectorInit(&frontier, sizeof(NodeID));
  for (NodeID n = 0; n < num_nodes; ++n) {
    NodeID label = comp[n];
    if (label_chosen[label])
      continue;
    label_chosen[label] = 1;
    VectorClear(&frontier);
    VectorPushBack(&frontier, &n);
    BitmapSetBit(&visited, (size_t)n);
    size_t idx = 0;
    while (idx < VectorSize(&frontier)) {
      NodeID u = VECTOR_AT(&frontier, NodeID, idx);
      NodeID *out_begin = GraphOutNeighborsBegin(g, u);
      NodeID *out_end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = out_begin; nbr < out_end; ++nbr) {
        NodeID v = *nbr;
        if (comp[v] != label) {
          VectorFree(&frontier);
          free(label_chosen);
          BitmapFree(&visited);
          return false;
        }
        if (!BitmapGetBit(&visited, (size_t)v)) {
          BitmapSetBit(&visited, (size_t)v);
          VectorPushBack(&frontier, &v);
        }
      }
      if (GraphDirected(g)) {
        NodeID *in_begin = GraphInNeighborsBegin(g, u);
        NodeID *in_end = GraphInNeighborsEnd(g, u);
        for (NodeID *nbr = in_begin; nbr < in_end; ++nbr) {
          NodeID v = *nbr;
          if (comp[v] != label) {
            VectorFree(&frontier);
            free(label_chosen);
            BitmapFree(&visited);
            return false;
          }
          if (!BitmapGetBit(&visited, (size_t)v)) {
            BitmapSetBit(&visited, (size_t)v);
            VectorPushBack(&frontier, &v);
          }
        }
      }
      idx++;
    }
  }
  for (NodeID n = 0; n < num_nodes; ++n) {
    if (!BitmapGetBit(&visited, (size_t)n)) {
      VectorFree(&frontier);
      free(label_chosen);
      BitmapFree(&visited);
      return false;
    }
  }
  VectorFree(&frontier);
  free(label_chosen);
  BitmapFree(&visited);
  return true;
}

int main(int argc, char **argv) {
  CLApp cli;
  CLAppInit(&cli, argc, argv, "connected-components-afforest");
  if (!CLAppParseArgs(&cli))
    return -1;
  Builder builder;
  BuilderInit(&builder, &cli.base);
  Graph g;
  BuilderMakeGraph(&builder, &g);
  GraphPrintStats(&g);
  int trials = CLAppNumTrials(&cli);
  double total_seconds = 0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    NodeID *comp = Afforest(&g, CLAppLoggingEnabled(&cli), 2);
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_seconds += seconds;
    if (CLAppDoAnalysis(&cli) && iter == trials - 1 && comp != NULL)
      PrintCompStats(&g, comp);
    if (CLAppDoVerify(&cli)) {
      bool ok = CCVerifier(&g, comp);
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
    }
    free(comp);
  }
  if (trials > 0)
    PrintTime("Average Time", total_seconds / trials);
  GraphFree(&g);
  return 0;
}
