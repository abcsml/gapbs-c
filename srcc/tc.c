#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "benchmark.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "util.h"
#include "vector.h"

static int CompareInt64Asc(const void *a, const void *b) {
  const int64_t *ia = (const int64_t *)a;
  const int64_t *ib = (const int64_t *)b;
  if (*ia == *ib)
    return 0;
  return (*ia < *ib) ? -1 : 1;
}

static size_t OrderedCount(const Graph *g) {
  size_t total = 0;
  int64_t num_nodes = GraphNumNodes(g);
  for (NodeID u = 0; u < num_nodes; ++u) {
    NodeID *u_begin = GraphOutNeighborsBegin(g, u);
    NodeID *u_end = GraphOutNeighborsEnd(g, u);
    for (NodeID *nbr = u_begin; nbr < u_end; ++nbr) {
      NodeID v = *nbr;
      if (v > u)
        break;
      NodeID *it = GraphOutNeighborsBegin(g, v);
      NodeID *v_end = GraphOutNeighborsEnd(g, v);
      for (NodeID *w_ptr = u_begin; w_ptr < u_end; ++w_ptr) {
        NodeID w = *w_ptr;
        if (w > v)
          break;
        while (it < v_end && *it < w)
          it++;
        if (it == v_end)
          break;
        if (*it == w)
          total++;
      }
    }
  }
  return total;
}

static bool WorthRelabelling(const Graph *g) {
  int64_t num_nodes = GraphNumNodes(g);
  if (num_nodes == 0)
    return false;
  int64_t average_degree = GraphNumEdges(g) / num_nodes;
  if (average_degree < 10)
    return false;
  SourcePicker picker;
  SourcePickerInit(&picker, g, -1);
  int64_t num_samples = num_nodes < 1000 ? num_nodes : 1000;
  Vector samples;
  VectorInit(&samples, sizeof(int64_t));
  for (int64_t i = 0; i < num_samples; ++i) {
    NodeID node = SourcePickerPickNext(&picker);
    int64_t degree = GraphOutDegree(g, node);
    VectorPushBack(&samples, &degree);
  }
  int64_t sample_total = 0;
  for (int64_t i = 0; i < num_samples; ++i)
    sample_total += VECTOR_AT(&samples, int64_t, i);
  qsort(VectorData(&samples), VectorSize(&samples), sizeof(int64_t), CompareInt64Asc);
  double sample_average = (double)sample_total / num_samples;
  double sample_median = (double)VECTOR_AT(&samples, int64_t, num_samples/2);
  VectorFree(&samples);
  return sample_average / 1.3 > sample_median;
}

static size_t Hybrid(const Graph *g) {
  if (WorthRelabelling(g)) {
    Graph relabeled = BuilderRelabelByDegree(g);
    size_t total = OrderedCount(&relabeled);
    GraphFree(&relabeled);
    return total;
  }
  return OrderedCount(g);
}

static void PrintTriangleStats(const Graph *g, size_t total) {
  (void)g;
  printf("%zu triangles\n", total);
}

static bool TCVerifier(const Graph *g, size_t test_total) {
  size_t total = 0;
  Vector intersection;
  VectorInit(&intersection, sizeof(NodeID));
  for (NodeID u = 0; u < GraphNumNodes(g); ++u) {
    NodeID *u_begin = GraphOutNeighborsBegin(g, u);
    NodeID *u_end = GraphOutNeighborsEnd(g, u);
    for (NodeID *nbr = u_begin; nbr < u_end; ++nbr) {
      NodeID v = *nbr;
      NodeID *v_begin = GraphOutNeighborsBegin(g, v);
      NodeID *v_end = GraphOutNeighborsEnd(g, v);
      VectorClear(&intersection);
      NodeID *it_u = u_begin;
      NodeID *it_v = v_begin;
      while (it_u < u_end && it_v < v_end) {
        if (*it_u == *it_v) {
          NodeID val = *it_u;
          VectorPushBack(&intersection, &val);
          it_u++;
          it_v++;
        } else if (*it_u < *it_v) {
          it_u++;
        } else {
          it_v++;
        }
      }
      total += VectorSize(&intersection);
    }
  }
  VectorFree(&intersection);
  total /= 6;
  if (total != test_total)
    printf("%zu != %zu\n", total, test_total);
  return total == test_total;
}

int main(int argc, char **argv) {
  CLApp cli;
  CLAppInit(&cli, argc, argv, "triangle count");
  if (!CLAppParseArgs(&cli))
    return -1;
  Builder builder;
  BuilderInit(&builder, &cli.base);
  Graph g;
  BuilderMakeGraph(&builder, &g);
  if (GraphDirected(&g)) {
    printf("Input graph is directed but tc requires undirected\n");
    GraphFree(&g);
    return -2;
  }
  GraphPrintStats(&g);
  int trials = CLAppNumTrials(&cli);
  double total_seconds = 0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    size_t total = Hybrid(&g);
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_seconds += seconds;
    if (CLAppDoAnalysis(&cli) && iter == trials - 1)
      PrintTriangleStats(&g, total);
    if (CLAppDoVerify(&cli)) {
      bool ok = TCVerifier(&g, total);
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
    }
  }
  if (trials > 0)
    PrintTime("Average Time", total_seconds / trials);
  GraphFree(&g);
  return 0;
}
