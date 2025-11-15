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
#include "util.h"
#include "vector.h"

static NodeID *ShiloachVishkin(const Graph *g) {
  int64_t num_nodes = GraphNumNodes(g);
  NodeID *comp = (NodeID *)malloc(sizeof(NodeID) * (size_t)num_nodes);
  if (comp == NULL && num_nodes > 0) {
    printf("Allocation failure in ShiloachVishkin\n");
    exit(EXIT_FAILURE);
  }
  for (NodeID n = 0; n < num_nodes; ++n)
    comp[n] = n;
  bool change = true;
  int num_iter = 0;
  while (change) {
    change = false;
    num_iter++;
    for (NodeID u = 0; u < num_nodes; ++u) {
      NodeID *begin = GraphOutNeighborsBegin(g, u);
      NodeID *end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = begin; nbr < end; ++nbr) {
        NodeID v = *nbr;
        NodeID comp_u = comp[u];
        NodeID comp_v = comp[v];
        if (comp_u == comp_v)
          continue;
        NodeID high = comp_u > comp_v ? comp_u : comp_v;
        NodeID low = comp_u + comp_v - high;
        if (comp[high] == high) {
          comp[high] = low;
          change = true;
        }
      }
    }
    for (NodeID n = 0; n < num_nodes; ++n) {
      while (comp[n] != comp[comp[n]])
        comp[n] = comp[comp[n]];
    }
  }
  printf("Shiloach-Vishkin took %d iterations\n", num_iter);
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
  CLAppInit(&cli, argc, argv, "connected-components");
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
    NodeID *comp = ShiloachVishkin(&g);
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
