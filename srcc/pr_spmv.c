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

typedef float ScoreT;
static const float kDamp = 0.85f;

static ScoreT *PageRankPull(const Graph *g, int max_iters, double epsilon,
                            bool logging_enabled) {
  int64_t num_nodes = GraphNumNodes(g);
  if (num_nodes == 0)
    return NULL;
  ScoreT init_score = 1.0f / (ScoreT)num_nodes;
  ScoreT base_score = (1.0f - kDamp) / (ScoreT)num_nodes;
  ScoreT *scores = (ScoreT *)malloc(sizeof(ScoreT) * (size_t)num_nodes);
  ScoreT *next_scores = (ScoreT *)malloc(sizeof(ScoreT) * (size_t)num_nodes);
  ScoreT *outgoing = (ScoreT *)malloc(sizeof(ScoreT) * (size_t)num_nodes);
  if (scores == NULL || next_scores == NULL || outgoing == NULL)
    exit(EXIT_FAILURE);
  for (NodeID n = 0; n < num_nodes; ++n)
    scores[n] = init_score;
  for (int iter = 0; iter < max_iters; ++iter) {
    for (NodeID n = 0; n < num_nodes; ++n) {
      int64_t degree = GraphOutDegree(g, n);
      outgoing[n] = degree > 0 ? scores[n] / degree : 0;
    }
    double error = 0;
    for (NodeID u = 0; u < num_nodes; ++u) {
      ScoreT incoming_total = 0;
      NodeID *begin = GraphInNeighborsBegin(g, u);
      NodeID *end = GraphInNeighborsEnd(g, u);
      for (NodeID *nbr = begin; nbr < end; ++nbr)
        incoming_total += outgoing[*nbr];
      next_scores[u] = base_score + kDamp * incoming_total;
      error += fabs(next_scores[u] - scores[u]);
    }
    if (logging_enabled)
      PrintStepTime("iter", error, iter);
    ScoreT *tmp = scores;
    scores = next_scores;
    next_scores = tmp;
    if (error < epsilon)
      break;
  }
  free(next_scores);
  free(outgoing);
  return scores;
}

static void PrintTopScores(const Graph *g, const ScoreT *scores) {
  if (scores == NULL)
    return;
  int64_t num_nodes = GraphNumNodes(g);
  Vector score_pairs;
  VectorInit(&score_pairs, sizeof(KeyValuePair));
  for (NodeID n = 0; n < num_nodes; ++n) {
    KeyValuePair kvp;
    kvp.key = n;
    kvp.value = scores[n];
    VectorPushBack(&score_pairs, &kvp);
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

static bool PRVerifier(const Graph *g, const ScoreT *scores,
                       double target_error) {
  int64_t num_nodes = GraphNumNodes(g);
  if (scores == NULL || num_nodes == 0)
    return false;
  ScoreT base_score = (1.0f - kDamp) / (ScoreT)num_nodes;
  ScoreT *incoming = (ScoreT *)calloc((size_t)num_nodes, sizeof(ScoreT));
  if (incoming == NULL)
    return false;
  double error = 0;
  for (NodeID u = 0; u < num_nodes; ++u) {
    int64_t degree = GraphOutDegree(g, u);
    ScoreT outgoing_contrib = degree > 0 ? scores[u] / degree : 0;
    NodeID *begin = GraphOutNeighborsBegin(g, u);
    NodeID *end = GraphOutNeighborsEnd(g, u);
    for (NodeID *nbr = begin; nbr < end; ++nbr)
      incoming[*nbr] += outgoing_contrib;
  }
  for (NodeID n = 0; n < num_nodes; ++n) {
    error += fabs(base_score + kDamp * incoming[n] - scores[n]);
    incoming[n] = 0;
  }
  PrintTime("Total Error", error);
  free(incoming);
  return error < target_error;
}

int main(int argc, char **argv) {
  CLPageRank cli;
  CLPageRankInit(&cli, argc, argv, "pagerank", 1e-4, 20);
  if (!CLPageRankParseArgs(&cli))
    return -1;
  Builder builder;
  BuilderInit(&builder, &cli.app.base);
  Graph g;
  BuilderMakeGraph(&builder, &g);
  GraphPrintStats(&g);
  int trials = CLAppNumTrials(&cli.app);
  double total_seconds = 0;
  for (int iter = 0; iter < trials; ++iter) {
    Timer trial_timer;
    TimerStart(&trial_timer);
    ScoreT *scores = PageRankPull(&g, CLPageRankMaxIters(&cli),
                                  CLPageRankTolerance(&cli),
                                  CLAppLoggingEnabled(&cli.app));
    TimerStop(&trial_timer);
    double seconds = TimerSeconds(&trial_timer);
    PrintTime("Trial Time", seconds);
    total_seconds += seconds;
    if (CLAppDoAnalysis(&cli.app) && iter == trials - 1 && scores != NULL)
      PrintTopScores(&g, scores);
    if (CLAppDoVerify(&cli.app)) {
      bool ok = PRVerifier(&g, scores, CLPageRankTolerance(&cli));
      PrintLabel("Verification", ok ? "PASS" : "FAIL");
    }
    free(scores);
  }
  if (trials > 0)
    PrintTime("Average Time", total_seconds / trials);
  GraphFree(&g);
  return 0;
}
