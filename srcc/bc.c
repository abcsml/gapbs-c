#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
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

typedef float ScoreT;
typedef double CountT;

static void PBFS(const Graph *g, NodeID source, CountT *path_counts,
                 Bitmap *succ, Vector *depth_index, SlidingQueue *queue) {
  int64_t num_nodes = GraphNumNodes(g);
  NodeID *depths = (NodeID *)malloc(sizeof(NodeID) * num_nodes);
  if (depths == NULL) {
    printf("Failed to allocate depths array\n");
    exit(EXIT_FAILURE);
  }
  for (NodeID n = 0; n < num_nodes; ++n) {
    depths[n] = -1;
    path_counts[n] = 0.0;
  }
  depths[source] = 0;
  path_counts[source] = 1.0;
  BitmapReset(succ);
  VectorClear(depth_index);
  SlidingQueueReset(queue);
  SlidingQueuePushBack(queue, source);
  SlidingQueueSlideWindow(queue);
  size_t offset = SlidingQueueCurrentBeginOffset(queue);
  VectorPushBack(depth_index, &offset);
  NodeID *g_out_start = GraphNumNodes(g) > 0 ? GraphOutNeighborsBegin(g, 0) : NULL;
  int depth = 0;
  while (!SlidingQueueEmpty(queue)) {
    depth++;
    size_t begin_offset = SlidingQueueCurrentBeginOffset(queue);
    size_t end_offset = SlidingQueueCurrentEndOffset(queue);
    NodeID *begin = SlidingQueuePointerAt(queue, begin_offset);
    NodeID *end = SlidingQueuePointerAt(queue, end_offset);
    for (NodeID *it = begin; it < end; ++it) {
      NodeID u = *it;
      NodeID *nbr_begin = GraphOutNeighborsBegin(g, u);
      NodeID *nbr_end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = nbr_begin; nbr < nbr_end; ++nbr) {
        NodeID v = *nbr;
        if (depths[v] == -1) {
          depths[v] = depth;
          SlidingQueuePushBack(queue, v);
        }
        if (depths[v] == depth) {
          if (g_out_start != NULL) {
            size_t pos = (size_t)(nbr - g_out_start);
            BitmapSetBit(succ, pos);
          }
          path_counts[v] += path_counts[u];
        }
      }
    }
    SlidingQueueSlideWindow(queue);
    size_t next_offset = SlidingQueueCurrentBeginOffset(queue);
    VectorPushBack(depth_index, &next_offset);
  }
  free(depths);
}

static ScoreT *Brandes(const Graph *g, SourcePicker *sp,
                       NodeID num_iters, bool logging_enabled) {
  int64_t num_nodes = GraphNumNodes(g);
  if (num_nodes == 0)
    return NULL;
  ScoreT *scores = (ScoreT *)calloc((size_t)num_nodes, sizeof(ScoreT));
  CountT *path_counts = (CountT *)malloc(sizeof(CountT) * num_nodes);
  ScoreT *deltas = (ScoreT *)malloc(sizeof(ScoreT) * num_nodes);
  if (scores == NULL || path_counts == NULL || deltas == NULL) {
    printf("Allocation failure in Brandes\n");
    exit(EXIT_FAILURE);
  }
  Bitmap succ;
  BitmapInit(&succ, (size_t)GraphNumEdgesDirected(g));
  Vector depth_index;
  VectorInit(&depth_index, sizeof(size_t));
  SlidingQueue queue;
  SlidingQueueInit(&queue, (size_t)num_nodes);
  Timer t;
  NodeID *g_out_start = GraphOutNeighborsBegin(g, 0);
  for (NodeID iter = 0; iter < num_iters; ++iter) {
    NodeID source = SourcePickerPickNext(sp);
    if (logging_enabled)
      PrintStepLabel("Source", source);
    TimerStart(&t);
    PBFS(g, source, path_counts, &succ, &depth_index, &queue);
    TimerStop(&t);
    if (logging_enabled)
      PrintStepTime("b", TimerSeconds(&t), -1);
    for (NodeID n = 0; n < num_nodes; ++n)
      deltas[n] = 0;
    TimerStart(&t);
    int depth_count = (int)VectorSize(&depth_index);
    for (int d = depth_count - 2; d >= 0; --d) {
      size_t start = VECTOR_AT(&depth_index, size_t, d);
      size_t end = VECTOR_AT(&depth_index, size_t, d + 1);
      NodeID *begin = SlidingQueuePointerAt(&queue, start);
      NodeID *finish = SlidingQueuePointerAt(&queue, end);
      for (NodeID *it = begin; it < finish; ++it) {
        NodeID u = *it;
        ScoreT delta_u = 0;
        NodeID *nbr_begin = GraphOutNeighborsBegin(g, u);
        NodeID *nbr_end = GraphOutNeighborsEnd(g, u);
        for (NodeID *nbr = nbr_begin; nbr < nbr_end; ++nbr) {
          NodeID v = *nbr;
          size_t pos = (size_t)(nbr - g_out_start);
          if (BitmapGetBit(&succ, pos) && path_counts[v] != 0) {
            delta_u += (ScoreT)((path_counts[u] / path_counts[v]) *
                                (1.0 + deltas[v]));
          }
        }
        deltas[u] = delta_u;
        scores[u] += delta_u;
      }
    }
    TimerStop(&t);
    if (logging_enabled)
      PrintStepTime("p", TimerSeconds(&t), -1);
  }
  ScoreT biggest_score = 0;
  for (NodeID n = 0; n < num_nodes; ++n)
    if (scores[n] > biggest_score)
      biggest_score = scores[n];
  if (biggest_score != 0) {
    for (NodeID n = 0; n < num_nodes; ++n)
      scores[n] = scores[n] / biggest_score;
  }
  SlidingQueueFree(&queue);
  VectorFree(&depth_index);
  BitmapFree(&succ);
  free(path_counts);
  free(deltas);
  return scores;
}

static void PrintTopScores(const Graph *g, const ScoreT *scores) {
  int64_t num_nodes = GraphNumNodes(g);
  Vector score_pairs;
  VectorInit(&score_pairs, sizeof(KeyValuePair));
  for (NodeID n = 0; n < num_nodes; ++n) {
    KeyValuePair pair;
    pair.key = n;
    pair.value = scores != NULL ? scores[n] : 0;
    VectorPushBack(&score_pairs, &pair);
  }
  Vector top_k;
  ComputeTopK((const KeyValuePair *)VectorData(&score_pairs),
              VectorSize(&score_pairs), 5, &top_k);
  ValueKeyPair *top = (ValueKeyPair *)VectorData(&top_k);
  for (size_t i = 0; i < VectorSize(&top_k); ++i)
    printf("%d:%f\n", top[i].key, top[i].value);
  VectorFree(&top_k);
  VectorFree(&score_pairs);
}

static bool BCVerifier(const Graph *g, SourcePicker *sp, NodeID num_iters,
                       const ScoreT *scores_to_test) {
  int64_t num_nodes = GraphNumNodes(g);
  ScoreT *scores = (ScoreT *)calloc((size_t)num_nodes, sizeof(ScoreT));
  if (scores == NULL)
    return false;
  for (NodeID iter = 0; iter < num_iters; ++iter) {
    NodeID source = SourcePickerPickNext(sp);
    NodeID *depths = (NodeID *)malloc(sizeof(NodeID) * num_nodes);
    CountT *path_counts = (CountT *)calloc((size_t)num_nodes, sizeof(CountT));
    ScoreT *deltas = (ScoreT *)calloc((size_t)num_nodes, sizeof(ScoreT));
    if (depths == NULL || path_counts == NULL || deltas == NULL) {
      printf("Allocation failure in verifier\n");
      exit(EXIT_FAILURE);
    }
    for (NodeID n = 0; n < num_nodes; ++n)
      depths[n] = -1;
    depths[source] = 0;
    path_counts[source] = 1.0;
    Vector to_visit;
    VectorInit(&to_visit, sizeof(NodeID));
    VectorReserve(&to_visit, (size_t)num_nodes);
    VectorPushBack(&to_visit, &source);
    size_t idx = 0;
    while (idx < VectorSize(&to_visit)) {
      NodeID u = VECTOR_AT(&to_visit, NodeID, idx);
      NodeID *nbr_begin = GraphOutNeighborsBegin(g, u);
      NodeID *nbr_end = GraphOutNeighborsEnd(g, u);
      for (NodeID *nbr = nbr_begin; nbr < nbr_end; ++nbr) {
        NodeID v = *nbr;
        if (depths[v] == -1) {
          depths[v] = depths[u] + 1;
          VectorPushBack(&to_visit, &v);
        }
        if (depths[v] == depths[u] + 1)
          path_counts[v] += path_counts[u];
      }
      idx++;
    }
    NodeID max_depth = 0;
    for (NodeID n = 0; n < num_nodes; ++n)
      if (depths[n] > max_depth)
        max_depth = depths[n];
    size_t depth_levels = (size_t)(max_depth + 1);
    Vector *verts_at_depth = (Vector *)malloc(sizeof(Vector) * depth_levels);
    for (size_t d = 0; d < depth_levels; ++d)
      VectorInit(&verts_at_depth[d], sizeof(NodeID));
    for (NodeID n = 0; n < num_nodes; ++n) {
      if (depths[n] != -1 && depths[n] < (NodeID)depth_levels)
        VectorPushBack(&verts_at_depth[depths[n]], &n);
    }
    for (int depth = (int)max_depth; depth >= 0; --depth) {
      Vector *level = &verts_at_depth[depth];
      for (size_t i = 0; i < VectorSize(level); ++i) {
        NodeID u = VECTOR_AT(level, NodeID, i);
        ScoreT delta_u = 0;
        NodeID *nbr_begin = GraphOutNeighborsBegin(g, u);
        NodeID *nbr_end = GraphOutNeighborsEnd(g, u);
        for (NodeID *nbr = nbr_begin; nbr < nbr_end; ++nbr) {
          NodeID v = *nbr;
          if (depths[v] == depths[u] + 1 && path_counts[v] != 0)
            delta_u += (ScoreT)((path_counts[u] / path_counts[v]) *
                                (1.0 + deltas[v]));
        }
        deltas[u] = delta_u;
        scores[u] += delta_u;
      }
    }
    for (size_t d = 0; d < depth_levels; ++d)
      VectorFree(&verts_at_depth[d]);
    free(verts_at_depth);
    VectorFree(&to_visit);
    free(depths);
    free(path_counts);
    free(deltas);
  }
  ScoreT biggest_score = 0;
  for (NodeID n = 0; n < num_nodes; ++n)
    if (scores[n] > biggest_score)
      biggest_score = scores[n];
  if (biggest_score != 0) {
    for (NodeID n = 0; n < num_nodes; ++n)
      scores[n] = scores[n] / biggest_score;
  }
  bool all_ok = true;
  for (NodeID n = 0; n < num_nodes; ++n) {
    ScoreT delta = scores_to_test != NULL ?
        (ScoreT)fabs(scores_to_test[n] - scores[n]) : scores[n];
    if (delta > 1e-4f) {
      printf("%d: %f != %f (%f)\n", n,
             scores[n], scores_to_test != NULL ? scores_to_test[n] : 0, delta);
      all_ok = false;
    }
  }
  free(scores);
  return all_ok;
}

int main(int argc, char **argv) {
  CLIterApp cli;
  CLIterAppInit(&cli, argc, argv, "betweenness-centrality", 1);
  if (!CLIterAppParseArgs(&cli))
    return -1;
  if (CLIterAppNumIters(&cli) > 1 && CLAppStartVertex(&cli.app) != -1)
    printf("Warning: iterating from same source (-r & -i)\n");
  Builder builder;
  BuilderInit(&builder, &cli.app.base);
  Graph g;
  BuilderMakeGraph(&builder, &g);
  GraphPrintStats(&g);
  SourcePicker sp;
  SourcePickerInit(&sp, &g, (NodeID)CLAppStartVertex(&cli.app));
  SourcePicker vsp;
  SourcePickerInit(&vsp, &g, (NodeID)CLAppStartVertex(&cli.app));
  int trials = CLAppNumTrials(&cli.app);
  double total_seconds = 0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    ScoreT *scores = Brandes(&g, &sp, CLIterAppNumIters(&cli),
                             CLAppLoggingEnabled(&cli.app));
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_seconds += seconds;
    if (CLAppDoAnalysis(&cli.app) && iter == trials - 1 && scores != NULL)
      PrintTopScores(&g, scores);
    if (CLAppDoVerify(&cli.app)) {
      bool ok = BCVerifier(&g, &vsp, CLIterAppNumIters(&cli), scores);
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
    }
    free(scores);
  }
  if (trials > 0)
    PrintTime("Average Time", total_seconds / trials);
  GraphFree(&g);
  return 0;
}
