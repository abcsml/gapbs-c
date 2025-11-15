#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "benchmark.h"
#include "bitmap.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "reader.h"
#include "sliding_queue.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

static int64_t BUStep(const Graph *g, NodeID *parent,
                      Bitmap *front, Bitmap *next) {
  int64_t awake_count = 0;
  BitmapReset(next);
  int64_t num_nodes = GraphNumNodes(g);
  for (NodeID u = 0; u < num_nodes; ++u) {
    if (parent[u] < 0) {
      NodeID *begin = GraphInNeighborsBegin(g, u);
      NodeID *end = GraphInNeighborsEnd(g, u);
      for (NodeID *it = begin; it < end; ++it) {
        NodeID v = *it;
        if (BitmapGetBit(front, (size_t)v)) {
          parent[u] = v;
          awake_count++;
          BitmapSetBit(next, (size_t)u);
          break;
        }
      }
    }
  }
  return awake_count;
}

static int64_t TDStep(const Graph *g, NodeID *parent,
                      SlidingQueue *queue) {
  int64_t scout_count = 0;
  NodeID *begin = SlidingQueueBegin(queue);
  NodeID *end = SlidingQueueEnd(queue);
  for (NodeID *it = begin; it < end; ++it) {
    NodeID u = *it;
    NodeID *nbr_begin = GraphOutNeighborsBegin(g, u);
    NodeID *nbr_end = GraphOutNeighborsEnd(g, u);
    for (NodeID *nbr = nbr_begin; nbr < nbr_end; ++nbr) {
      NodeID v = *nbr;
      NodeID curr_val = parent[v];
      if (curr_val < 0) {
        parent[v] = u;
        SlidingQueuePushBack(queue, v);
        scout_count += -curr_val;
      }
    }
  }
  return scout_count;
}

static void QueueToBitmap(const SlidingQueue *queue, Bitmap *bm) {
  BitmapReset(bm);
  NodeID *begin = SlidingQueueBegin(queue);
  NodeID *end = SlidingQueueEnd(queue);
  for (NodeID *it = begin; it < end; ++it) {
    BitmapSetBit(bm, (size_t)(*it));
  }
}

static void BitmapToQueue(const Graph *g, const Bitmap *bm,
                          SlidingQueue *queue) {
  SlidingQueueReset(queue);
  int64_t num_nodes = GraphNumNodes(g);
  for (NodeID n = 0; n < num_nodes; ++n) {
    if (BitmapGetBit(bm, (size_t)n))
      SlidingQueuePushBack(queue, n);
  }
  SlidingQueueSlideWindow(queue);
}

static NodeID *InitParent(const Graph *g) {
  int64_t num_nodes = GraphNumNodes(g);
  NodeID *parent = (NodeID *)malloc(sizeof(NodeID) * num_nodes);
  if (parent == NULL) {
    printf("Failed to allocate parent array\n");
    exit(EXIT_FAILURE);
  }
  for (NodeID n = 0; n < num_nodes; ++n) {
    int64_t degree = GraphOutDegree(g, n);
    parent[n] = degree != 0 ? (NodeID)(-degree) : -1;
  }
  return parent;
}

static NodeID *DOBFS(const Graph *g, NodeID source, bool logging_enabled,
                     int alpha, int beta) {
  if (logging_enabled)
    PrintStepLabel("Source", source);
  Timer t;
  TimerStart(&t);
  NodeID *parent = InitParent(g);
  TimerStop(&t);
  if (logging_enabled)
    PrintStepTime("i", TimerSeconds(&t), -1);
  parent[source] = source;
  SlidingQueue queue;
  SlidingQueueInit(&queue, (size_t)GraphNumNodes(g));
  SlidingQueuePushBack(&queue, source);
  SlidingQueueSlideWindow(&queue);
  Bitmap curr;
  BitmapInit(&curr, (size_t)GraphNumNodes(g));
  BitmapReset(&curr);
  Bitmap front;
  BitmapInit(&front, (size_t)GraphNumNodes(g));
  BitmapReset(&front);
  int64_t edges_to_check = GraphNumEdgesDirected(g);
  int64_t scout_count = GraphOutDegree(g, source);
  while (!SlidingQueueEmpty(&queue)) {
    if (scout_count > edges_to_check / alpha) {
      int64_t awake_count;
      int64_t old_awake_count;
      TimerStart(&t);
      QueueToBitmap(&queue, &front);
      TimerStop(&t);
      if (logging_enabled)
        PrintStepTime("e", TimerSeconds(&t), -1);
      awake_count = (int64_t)SlidingQueueSize(&queue);
      SlidingQueueSlideWindow(&queue);
      do {
        TimerStart(&t);
        old_awake_count = awake_count;
        awake_count = BUStep(g, parent, &front, &curr);
        BitmapSwap(&front, &curr);
        TimerStop(&t);
        if (logging_enabled)
          PrintStepTime("bu", TimerSeconds(&t), awake_count);
      } while ((awake_count >= old_awake_count) ||
               (awake_count > GraphNumNodes(g) / beta));
      TimerStart(&t);
      BitmapToQueue(g, &front, &queue);
      TimerStop(&t);
      if (logging_enabled)
        PrintStepTime("c", TimerSeconds(&t), -1);
      scout_count = 1;
    } else {
      TimerStart(&t);
      edges_to_check -= scout_count;
      scout_count = TDStep(g, parent, &queue);
      SlidingQueueSlideWindow(&queue);
      TimerStop(&t);
      if (logging_enabled)
        PrintStepTime("td", TimerSeconds(&t), (int64_t)SlidingQueueSize(&queue));
    }
  }
  int64_t num_nodes = GraphNumNodes(g);
  for (NodeID n = 0; n < num_nodes; ++n) {
    if (parent[n] < -1)
      parent[n] = -1;
  }
  SlidingQueueFree(&queue);
  BitmapFree(&curr);
  BitmapFree(&front);
  return parent;
}

static void PrintBFSStats(const Graph *g, const NodeID *bfs_tree) {
  int64_t tree_size = 0;
  int64_t n_edges = 0;
  int64_t num_nodes = GraphNumNodes(g);
  for (NodeID n = 0; n < num_nodes; ++n) {
    if (bfs_tree[n] >= 0) {
      n_edges += GraphOutDegree(g, n);
      tree_size++;
    }
  }
  printf("BFS Tree has %" PRId64 " nodes and %" PRId64 " edges\n",
         tree_size, n_edges);
}

static bool BFSVerifier(const Graph *g, NodeID source,
                        const NodeID *parent) {
  int64_t num_nodes = GraphNumNodes(g);
  int *depth = (int *)malloc(sizeof(int) * num_nodes);
  if (depth == NULL) {
    printf("Failed to allocate depth array\n");
    exit(EXIT_FAILURE);
  }
  for (NodeID n = 0; n < num_nodes; ++n)
    depth[n] = -1;
  depth[source] = 0;
  Vector to_visit;
  VectorInit(&to_visit, sizeof(NodeID));
  VectorReserve(&to_visit, (size_t)num_nodes);
  VectorPushBack(&to_visit, &source);
  size_t idx = 0;
  while (idx < VectorSize(&to_visit)) {
    NodeID u = VECTOR_AT(&to_visit, NodeID, idx);
    NodeID *begin = GraphOutNeighborsBegin(g, u);
    NodeID *end = GraphOutNeighborsEnd(g, u);
    for (NodeID *it = begin; it < end; ++it) {
      NodeID v = *it;
      if (depth[v] == -1) {
        depth[v] = depth[u] + 1;
        VectorPushBack(&to_visit, &v);
      }
    }
    idx++;
  }
  bool ok = true;
  for (NodeID u = 0; u < num_nodes; ++u) {
    if ((depth[u] != -1) && (parent[u] != -1)) {
      if (u == source) {
        if (!((parent[u] == u) && (depth[u] == 0))) {
          printf("Source wrong\n");
          ok = false;
          break;
        }
        continue;
      }
      bool parent_found = false;
      NodeID *begin = GraphInNeighborsBegin(g, u);
      NodeID *end = GraphInNeighborsEnd(g, u);
      for (NodeID *it = begin; it < end; ++it) {
        if (*it == parent[u]) {
          if (depth[*it] != depth[u] - 1) {
            printf("Wrong depths for %d & %d\n", u, *it);
            ok = false;
          }
          parent_found = true;
          break;
        }
      }
      if (!parent_found) {
        printf("Couldn't find edge from %d to %d\n", parent[u], u);
        ok = false;
        break;
      }
    } else if (depth[u] != parent[u]) {
      printf("Reachability mismatch\n");
      ok = false;
      break;
    }
  }
  VectorFree(&to_visit);
  free(depth);
  return ok;
}

int main(int argc, char **argv) {
  CLApp cli;
  CLAppInit(&cli, argc, argv, "breadth-first search");
  if (!CLAppParseArgs(&cli))
    return -1;
  Builder builder;
  BuilderInit(&builder, &cli.base);
  Graph g;
  BuilderMakeGraph(&builder, &g);
  GraphPrintStats(&g);
  SourcePicker picker;
  SourcePickerInit(&picker, &g, (NodeID)CLAppStartVertex(&cli));
  SourcePicker verifier_picker;
  SourcePickerInit(&verifier_picker, &g, (NodeID)CLAppStartVertex(&cli));
  int trials = CLAppNumTrials(&cli);
  double total_time = 0.0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    NodeID source = SourcePickerPickNext(&picker);
    NodeID *parent = DOBFS(&g, source, CLAppLoggingEnabled(&cli), 15, 18);
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_time += seconds;
    if (CLAppDoAnalysis(&cli) && iter == trials - 1)
      PrintBFSStats(&g, parent);
    if (CLAppDoVerify(&cli)) {
      Timer verify_timer;
      TimerStart(&verify_timer);
      NodeID verify_source = SourcePickerPickNext(&verifier_picker);
      bool ok = BFSVerifier(&g, verify_source, parent);
      TimerStop(&verify_timer);
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
      PrintTime("Verification Time", TimerSeconds(&verify_timer));
    }
    free(parent);
  }
  if (trials > 0)
    PrintTime("Average Time", total_time / trials);
  GraphFree(&g);
  return 0;
}
