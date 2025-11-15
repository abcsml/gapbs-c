#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "benchmark.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

static const WeightT kDistInf = (WeightT)((~(uint64_t)0) >> 2);

typedef struct {
  Vector *bins;
  size_t size;
} BinArray;

typedef struct {
  WeightT dist;
  NodeID node;
} HeapNode;

typedef struct {
  HeapNode *data;
  size_t size;
  size_t capacity;
} MinHeap;

static void HeapInit(MinHeap *heap) {
  heap->data = NULL;
  heap->size = 0;
  heap->capacity = 0;
}

static void HeapFree(MinHeap *heap) {
  free(heap->data);
  heap->data = NULL;
  heap->size = 0;
  heap->capacity = 0;
}

static void HeapSwap(HeapNode *a, HeapNode *b) {
  HeapNode tmp = *a;
  *a = *b;
  *b = tmp;
}

static void HeapPush(MinHeap *heap, WeightT dist, NodeID node) {
  if (heap->size == heap->capacity) {
    heap->capacity = heap->capacity == 0 ? 16 : heap->capacity * 2;
    heap->data = (HeapNode *)realloc(heap->data, heap->capacity * sizeof(HeapNode));
  }
  size_t idx = heap->size++;
  heap->data[idx].dist = dist;
  heap->data[idx].node = node;
  while (idx > 0) {
    size_t parent = (idx - 1) / 2;
    if (heap->data[parent].dist <= heap->data[idx].dist)
      break;
    HeapSwap(&heap->data[parent], &heap->data[idx]);
    idx = parent;
  }
}

static int HeapEmpty(const MinHeap *heap) {
  return heap->size == 0;
}

static HeapNode HeapPop(MinHeap *heap) {
  HeapNode root = heap->data[0];
  heap->data[0] = heap->data[--heap->size];
  size_t idx = 0;
  while (true) {
    size_t left = 2 * idx + 1;
    size_t right = 2 * idx + 2;
    size_t smallest = idx;
    if (left < heap->size && heap->data[left].dist < heap->data[smallest].dist)
      smallest = left;
    if (right < heap->size && heap->data[right].dist < heap->data[smallest].dist)
      smallest = right;
    if (smallest == idx)
      break;
    HeapSwap(&heap->data[idx], &heap->data[smallest]);
    idx = smallest;
  }
  return root;
}

static void EnsureBin(BinArray *arr, size_t idx) {
  if (idx < arr->size)
    return;
  size_t old = arr->size;
  size_t new_size = idx + 1;
  arr->bins = (Vector *)realloc(arr->bins, new_size * sizeof(Vector));
  if (arr->bins == NULL)
    exit(EXIT_FAILURE);
  for (size_t i = old; i < new_size; ++i)
    VectorInit(&arr->bins[i], sizeof(NodeID));
  arr->size = new_size;
}

static WeightT *DeltaStep(const WGraph *g, NodeID source, WeightT delta,
                          bool logging_enabled) {
  int64_t num_nodes = GraphNumNodesW(g);
  if (num_nodes == 0)
    return NULL;
  if (delta <= 0)
    delta = 1;
  WeightT *dist = (WeightT *)malloc(sizeof(WeightT) * (size_t)num_nodes);
  if (dist == NULL)
    exit(EXIT_FAILURE);
  for (NodeID n = 0; n < num_nodes; ++n)
    dist[n] = kDistInf;
  dist[source] = 0;
  BinArray bins = {NULL, 0};
  EnsureBin(&bins, 0);
  VectorPushBack(&bins.bins[0], &source);
  size_t current_bin = 0;
  while (current_bin < bins.size) {
    if (VectorIsEmpty(&bins.bins[current_bin])) {
      current_bin++;
      continue;
    }
    Vector bag;
    VectorInit(&bag, sizeof(NodeID));
    size_t bin_size = VectorSize(&bins.bins[current_bin]);
    VectorResize(&bag, bin_size);
    memcpy(VectorData(&bag), VectorData(&bins.bins[current_bin]),
           bin_size * sizeof(NodeID));
    VectorClear(&bins.bins[current_bin]);
    size_t idx = 0;
    while (idx < VectorSize(&bag)) {
      NodeID u = VECTOR_AT(&bag, NodeID, idx);
      idx++;
      if ((WeightT)(current_bin * delta) > dist[u])
        continue;
      NodeWeight *begin = WGraphOutNeighborsBegin(g, u);
      NodeWeight *end = WGraphOutNeighborsEnd(g, u);
      for (NodeWeight *nbr = begin; nbr < end; ++nbr) {
        NodeID v = nbr->v;
        WeightT new_dist = dist[u] + nbr->w;
        if (new_dist < dist[v]) {
          dist[v] = new_dist;
          size_t dest_bin = (size_t)(new_dist / delta);
          EnsureBin(&bins, dest_bin);
          if (dest_bin == current_bin) {
            VectorPushBack(&bag, &v);
          } else {
            VectorPushBack(&bins.bins[dest_bin], &v);
          }
        }
      }
    }
    VectorFree(&bag);
  }
  for (size_t i = 0; i < bins.size; ++i)
    VectorFree(&bins.bins[i]);
  free(bins.bins);
  if (logging_enabled)
    printf("Delta-step completed\n");
  return dist;
}

static void PrintSSSPStats(const WGraph *g, const WeightT *dist) {
  int64_t num_nodes = GraphNumNodesW(g);
  int64_t reached = 0;
  for (NodeID n = 0; n < num_nodes; ++n)
    if (dist[n] != kDistInf)
      reached++;
  printf("SSSP Tree reaches %" PRId64 " nodes\n", reached);
}

static bool SSSPVerifier(const WGraph *g, NodeID source,
                         const WeightT *dist_to_test) {
  int64_t num_nodes = GraphNumNodesW(g);
  WeightT *oracle = (WeightT *)malloc(sizeof(WeightT) * (size_t)num_nodes);
  if (oracle == NULL)
    exit(EXIT_FAILURE);
  for (NodeID n = 0; n < num_nodes; ++n)
    oracle[n] = kDistInf;
  oracle[source] = 0;
  MinHeap heap;
  HeapInit(&heap);
  HeapPush(&heap, 0, source);
  while (!HeapEmpty(&heap)) {
    HeapNode hn = HeapPop(&heap);
    if (hn.dist != oracle[hn.node])
      continue;
    NodeWeight *begin = WGraphOutNeighborsBegin(g, hn.node);
    NodeWeight *end = WGraphOutNeighborsEnd(g, hn.node);
    for (NodeWeight *nbr = begin; nbr < end; ++nbr) {
      WeightT new_dist = hn.dist + nbr->w;
      if (new_dist < oracle[nbr->v]) {
        oracle[nbr->v] = new_dist;
        HeapPush(&heap, new_dist, nbr->v);
      }
    }
  }
  HeapFree(&heap);
  bool ok = true;
  for (NodeID n = 0; n < num_nodes; ++n) {
    if (oracle[n] != dist_to_test[n]) {
      printf("%d: %f != %f\n", n,
             (double)dist_to_test[n], (double)oracle[n]);
      ok = false;
    }
  }
  free(oracle);
  return ok;
}

int main(int argc, char **argv) {
  CLDelta cli;
  CLDeltaInit(&cli, argc, argv, "single-source shortest-path", 1.0);
  if (!CLDeltaParseArgs(&cli))
    return -1;
  WeightedBuilder builder;
  WeightedBuilderInit(&builder, &cli.app.base);
  WGraph g;
  WeightedBuilderMakeGraph(&builder, &g);
  WSourcePicker sp;
  WSourcePickerInit(&sp, &g, (NodeID)CLAppStartVertex(&cli.app));
  WSourcePicker vsp;
  WSourcePickerInit(&vsp, &g, (NodeID)CLAppStartVertex(&cli.app));
  WGraphPrintStats(&g);
  int trials = CLAppNumTrials(&cli.app);
  double total_seconds = 0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    NodeID source = WSourcePickerPickNext(&sp);
    WeightT delta = (WeightT)CLDeltaValue(&cli);
    WeightT *dist = DeltaStep(&g, source, delta, CLAppLoggingEnabled(&cli.app));
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_seconds += seconds;
    if (CLAppDoAnalysis(&cli.app) && iter == trials - 1 && dist != NULL)
      PrintSSSPStats(&g, dist);
    if (CLAppDoVerify(&cli.app)) {
      bool ok = SSSPVerifier(&g, WSourcePickerPickNext(&vsp), dist);
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
    }
    free(dist);
  }
  if (trials > 0)
    PrintTime("Average Time", total_seconds / trials);
  WGraphFree(&g);
  return 0;
}
