#ifndef BUILDER_H_
#define BUILDER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_line.h"
#include "generator.h"
#include "graph.h"
#include "reader.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

typedef struct {
  const CLBase *cli;
  bool symmetrize;
} Builder;

typedef struct {
  const CLBase *cli;
  bool symmetrize;
} WeightedBuilder;

static int CompareEdge(const void *a, const void *b) {
  const Edge *ea = (const Edge *)a;
  const Edge *eb = (const Edge *)b;
  if (ea->u != eb->u)
    return (ea->u < eb->u) ? -1 : 1;
  if (ea->v != eb->v)
    return (ea->v < eb->v) ? -1 : 1;
  return 0;
}

static int CompareWEdge(const void *a, const void *b) {
  const WEdge *ea = (const WEdge *)a;
  const WEdge *eb = (const WEdge *)b;
  if (ea->u != eb->u)
    return (ea->u < eb->u) ? -1 : 1;
  if (ea->v.v != eb->v.v)
    return (ea->v.v < eb->v.v) ? -1 : 1;
  return 0;
}

static int CompareNodeIDAsc(const void *a, const void *b) {
  NodeID va = *(const NodeID *)a;
  NodeID vb = *(const NodeID *)b;
  if (va == vb)
    return 0;
  return (va < vb) ? -1 : 1;
}

static uint64_t EdgeKey(NodeID u, NodeID v) {
  uint32_t uu = (uint32_t)u;
  uint32_t vv = (uint32_t)v;
  return (((uint64_t)uu) << 32) | vv;
}

static void RadixSortEdges(Edge *edges, size_t n) {
  if (n <= 1)
    return;
  Edge *tmp = (Edge *)malloc(n * sizeof(Edge));
  if (tmp == NULL)
    exit(EXIT_FAILURE);
  const int BITS = 16;
  const int PASSES = 4;
  const size_t BUCKETS = 1 << BITS;
  size_t counts[BUCKETS];
  Edge *src = edges;
  Edge *dst = tmp;
  for (int pass = 0; pass < PASSES; ++pass) {
    memset(counts, 0, sizeof(counts));
    unsigned shift = pass * BITS;
    for (size_t i = 0; i < n; ++i) {
      uint64_t key = EdgeKey(src[i].u, src[i].v);
      size_t bucket = (size_t)((key >> shift) & (BUCKETS - 1));
      counts[bucket]++;
    }
    size_t sum = 0;
    for (size_t b = 0; b < BUCKETS; ++b) {
      size_t c = counts[b];
      counts[b] = sum;
      sum += c;
    }
    for (size_t i = 0; i < n; ++i) {
      uint64_t key = EdgeKey(src[i].u, src[i].v);
      size_t bucket = (size_t)((key >> shift) & (BUCKETS - 1));
      dst[counts[bucket]++] = src[i];
    }
    Edge *tmp_ptr = src;
    src = dst;
    dst = tmp_ptr;
  }
  if (src != edges)
    memcpy(edges, src, n * sizeof(Edge));
  free(tmp);
}

static void RadixSortWEdges(WEdge *edges, size_t n) {
  if (n <= 1)
    return;
  WEdge *tmp = (WEdge *)malloc(n * sizeof(WEdge));
  if (tmp == NULL)
    exit(EXIT_FAILURE);
  const int BITS = 16;
  const int PASSES = 4;
  const size_t BUCKETS = 1 << BITS;
  size_t counts[BUCKETS];
  WEdge *src = edges;
  WEdge *dst = tmp;
  for (int pass = 0; pass < PASSES; ++pass) {
    memset(counts, 0, sizeof(counts));
    unsigned shift = pass * BITS;
    for (size_t i = 0; i < n; ++i) {
      uint64_t key = EdgeKey(src[i].u, src[i].v.v);
      size_t bucket = (size_t)((key >> shift) & (BUCKETS - 1));
      counts[bucket]++;
    }
    size_t sum = 0;
    for (size_t b = 0; b < BUCKETS; ++b) {
      size_t c = counts[b];
      counts[b] = sum;
      sum += c;
    }
    for (size_t i = 0; i < n; ++i) {
      uint64_t key = EdgeKey(src[i].u, src[i].v.v);
      size_t bucket = (size_t)((key >> shift) & (BUCKETS - 1));
      dst[counts[bucket]++] = src[i];
    }
    WEdge *tmp_ptr = src;
    src = dst;
    dst = tmp_ptr;
  }
  if (src != edges)
    memcpy(edges, src, n * sizeof(WEdge));
  free(tmp);
}

static void BuilderDeduplicateEdges(Vector *edges, bool weighted) {
  size_t n = VectorSize(edges);
  if (n == 0)
    return;
  if (weighted) {
    WEdge *elist = (WEdge *)VectorData(edges);
    RadixSortWEdges(elist, n);
    size_t write = 0;
    for (size_t read = 0; read < n; ++read) {
      if (elist[read].u == elist[read].v.v)
        continue;
      if (write == 0 ||
          elist[read].u != elist[write-1].u ||
          elist[read].v.v != elist[write-1].v.v) {
        elist[write++] = elist[read];
      }
    }
    VectorResize(edges, write);
  } else {
    Edge *elist = (Edge *)VectorData(edges);
    RadixSortEdges(elist, n);
    size_t write = 0;
    for (size_t read = 0; read < n; ++read) {
      if (elist[read].u == elist[read].v)
        continue;
      if (write == 0 ||
          elist[read].u != elist[write-1].u ||
          elist[read].v != elist[write-1].v) {
        elist[write++] = elist[read];
      }
    }
    VectorResize(edges, write);
  }
}

static int64_t BuilderMaxNode(const Vector *edges, bool weighted) {
  int64_t max_node = -1;
  size_t n = VectorSize(edges);
  if (weighted) {
    const WEdge *elist = (const WEdge *)VectorData(edges);
    for (size_t i = 0; i < n; ++i) {
      if (elist[i].u > max_node)
        max_node = elist[i].u;
      if (elist[i].v.v > max_node)
        max_node = elist[i].v.v;
    }
  } else {
    const Edge *elist = (const Edge *)VectorData(edges);
    for (size_t i = 0; i < n; ++i) {
      if (elist[i].u > max_node)
        max_node = elist[i].u;
      if (elist[i].v > max_node)
        max_node = elist[i].v;
    }
  }
  return max_node;
}

static void BuilderEdgesToCSR(const Vector *edges, int64_t num_nodes,
                              bool symmetrize, Graph *graph) {
  size_t edge_count = VectorSize(edges);
  SGOffset *out_degrees = (SGOffset *)calloc(num_nodes, sizeof(SGOffset));
  SGOffset *in_degrees = (SGOffset *)calloc(num_nodes, sizeof(SGOffset));
  if (out_degrees == NULL || in_degrees == NULL) {
    printf("Allocation failure\n");
    exit(EXIT_FAILURE);
  }
  const Edge *elist = (const Edge *)VectorData(edges);
  for (size_t i = 0; i < edge_count; ++i) {
    out_degrees[elist[i].u]++;
    in_degrees[elist[i].v]++;
  }
  SGOffset *out_offsets = (SGOffset *)malloc((num_nodes + 1) * sizeof(SGOffset));
  SGOffset *in_offsets = (SGOffset *)malloc((num_nodes + 1) * sizeof(SGOffset));
  if (out_offsets == NULL || in_offsets == NULL)
    exit(EXIT_FAILURE);
  SGOffset sum = 0;
  for (int64_t n = 0; n < num_nodes; ++n) {
    out_offsets[n] = sum;
    sum += out_degrees[n];
  }
  out_offsets[num_nodes] = sum;
  sum = 0;
  for (int64_t n = 0; n < num_nodes; ++n) {
    in_offsets[n] = sum;
    sum += in_degrees[n];
  }
  in_offsets[num_nodes] = sum;
  size_t out_neigh_cap = (size_t)out_offsets[num_nodes];
  if (out_neigh_cap == 0)
    out_neigh_cap = 1;
  NodeID *out_neighs = (NodeID *)malloc(out_neigh_cap * sizeof(NodeID));
  size_t in_neigh_cap = (size_t)in_offsets[num_nodes];
  if (in_neigh_cap == 0)
    in_neigh_cap = 1;
  NodeID *in_neighs = (NodeID *)malloc(in_neigh_cap * sizeof(NodeID));
  if (out_neighs == NULL || in_neighs == NULL)
    exit(EXIT_FAILURE);
  SGOffset *out_positions = (SGOffset *)malloc(num_nodes * sizeof(SGOffset));
  SGOffset *in_positions = (SGOffset *)malloc(num_nodes * sizeof(SGOffset));
  if (out_positions == NULL || in_positions == NULL)
    exit(EXIT_FAILURE);
  memcpy(out_positions, out_offsets, num_nodes * sizeof(SGOffset));
  memcpy(in_positions, in_offsets, num_nodes * sizeof(SGOffset));
  for (size_t i = 0; i < edge_count; ++i) {
    NodeID u = elist[i].u;
    NodeID v = elist[i].v;
    SGOffset out_idx = out_positions[u]++;
    SGOffset in_idx = in_positions[v]++;
    out_neighs[out_idx] = v;
    in_neighs[in_idx] = u;
  }
  free(out_positions);
  free(in_positions);
  NodeID **out_index = GraphBuildIndex(out_offsets, (size_t)num_nodes + 1, out_neighs);
  NodeID **in_index = GraphBuildIndex(in_offsets, (size_t)num_nodes + 1, in_neighs);
  if (out_index == NULL || in_index == NULL)
    exit(EXIT_FAILURE);
  free(out_offsets);
  free(in_offsets);
  bool directed = !symmetrize;
  int64_t stored_edges = directed ? (int64_t)edge_count : (int64_t)(edge_count / 2);
  GraphInit(graph, directed, num_nodes, stored_edges,
            out_index, out_neighs, in_index, in_neighs);
  free(out_degrees);
  free(in_degrees);
}

static void BuilderWeightedEdgesToCSR(const Vector *edges, int64_t num_nodes,
                                      bool symmetrize, WGraph *graph) {
  size_t edge_count = VectorSize(edges);
  SGOffset *out_degrees = (SGOffset *)calloc(num_nodes, sizeof(SGOffset));
  SGOffset *in_degrees = (SGOffset *)calloc(num_nodes, sizeof(SGOffset));
  if (out_degrees == NULL || in_degrees == NULL)
    exit(EXIT_FAILURE);
  const WEdge *elist = (const WEdge *)VectorData(edges);
  for (size_t i = 0; i < edge_count; ++i) {
    out_degrees[elist[i].u]++;
    in_degrees[elist[i].v.v]++;
  }
  SGOffset *out_offsets = (SGOffset *)malloc((num_nodes + 1) * sizeof(SGOffset));
  SGOffset *in_offsets = (SGOffset *)malloc((num_nodes + 1) * sizeof(SGOffset));
  if (out_offsets == NULL || in_offsets == NULL)
    exit(EXIT_FAILURE);
  SGOffset sum = 0;
  for (int64_t n = 0; n < num_nodes; ++n) {
    out_offsets[n] = sum;
    sum += out_degrees[n];
  }
  out_offsets[num_nodes] = sum;
  sum = 0;
  for (int64_t n = 0; n < num_nodes; ++n) {
    in_offsets[n] = sum;
    sum += in_degrees[n];
  }
  in_offsets[num_nodes] = sum;
  size_t out_neigh_cap = (size_t)out_offsets[num_nodes];
  if (out_neigh_cap == 0)
    out_neigh_cap = 1;
  NodeWeight *out_neighs = (NodeWeight *)malloc(out_neigh_cap * sizeof(NodeWeight));
  size_t in_neigh_cap = (size_t)in_offsets[num_nodes];
  if (in_neigh_cap == 0)
    in_neigh_cap = 1;
  NodeWeight *in_neighs = (NodeWeight *)malloc(in_neigh_cap * sizeof(NodeWeight));
  if (out_neighs == NULL || in_neighs == NULL)
    exit(EXIT_FAILURE);
  SGOffset *out_positions = (SGOffset *)malloc(num_nodes * sizeof(SGOffset));
  SGOffset *in_positions = (SGOffset *)malloc(num_nodes * sizeof(SGOffset));
  if (out_positions == NULL || in_positions == NULL)
    exit(EXIT_FAILURE);
  memcpy(out_positions, out_offsets, num_nodes * sizeof(SGOffset));
  memcpy(in_positions, in_offsets, num_nodes * sizeof(SGOffset));
  for (size_t i = 0; i < edge_count; ++i) {
    NodeID u = elist[i].u;
    NodeID v = elist[i].v.v;
    WeightT w = elist[i].v.w;
    SGOffset out_idx = out_positions[u]++;
    SGOffset in_idx = in_positions[v]++;
    out_neighs[out_idx].v = v;
    out_neighs[out_idx].w = w;
    in_neighs[in_idx].v = u;
    in_neighs[in_idx].w = w;
  }
  free(out_positions);
  free(in_positions);
  NodeWeight **out_index = WGraphBuildIndex(out_offsets, (size_t)num_nodes + 1, out_neighs);
  NodeWeight **in_index = WGraphBuildIndex(in_offsets, (size_t)num_nodes + 1, in_neighs);
  if (out_index == NULL || in_index == NULL)
    exit(EXIT_FAILURE);
  free(out_offsets);
  free(in_offsets);
  bool directed = !symmetrize;
  int64_t stored_edges = directed ? (int64_t)edge_count : (int64_t)(edge_count / 2);
  WGraphInit(graph, directed, num_nodes, stored_edges,
             out_index, out_neighs, in_index, in_neighs);
  free(out_degrees);
  free(in_degrees);
}

static void BuilderInitEdgesFromGenerator(bool weighted, int uniform,
                                          int scale, int degree, Vector *edges,
                                          int64_t *num_nodes, int *needs_weights) {
  Generator gen;
  GeneratorInit(&gen, scale, degree);
  *num_nodes = gen.num_nodes;
  Vector temp;
  if (weighted) {
    VectorInit(&temp, sizeof(Edge));
    GeneratorGenerateEL(&gen, uniform, &temp);
    VectorInit(edges, sizeof(WEdge));
    VectorResize(edges, VectorSize(&temp));
    WEdge *dst = (WEdge *)VectorData(edges);
    Edge *src = (Edge *)VectorData(&temp);
    for (size_t i = 0; i < VectorSize(&temp); ++i) {
      dst[i].u = src[i].u;
      dst[i].v.v = src[i].v;
      dst[i].v.w = 1;
    }
    VectorFree(&temp);
    *needs_weights = 1;
  } else {
    GeneratorGenerateEL(&gen, uniform, edges);
    if (needs_weights != NULL)
      *needs_weights = 0;
  }
}

static bool BuilderInitEdgesFromFile(const CLBase *cli, bool weighted,
                                     Vector *edges, int *needs_weights,
                                     int64_t *num_nodes,
                                     Graph *graph, WGraph *wgraph) {
  const char *filename = CLBaseFilename(cli);
  Reader reader;
  ReaderInit(&reader, filename);
  const char *suffix = ReaderGetSuffix(&reader);
  if (!weighted && strcmp(suffix, ".sg") == 0) {
    ReaderReadSerializedGraph(&reader, graph, 1);
    ReaderFree(&reader);
    return true;
  }
  if (weighted && strcmp(suffix, ".wsg") == 0) {
    ReaderReadSerializedWeightedGraph(&reader, wgraph, 1);
    ReaderFree(&reader);
    return true;
  }
  ReaderReadEdgeList(&reader, weighted ? 1 : 0, edges, needs_weights);
  int64_t max_node = BuilderMaxNode(edges, weighted);
  *num_nodes = max_node + 1;
  ReaderFree(&reader);
  return false;
}

static void BuilderAppendSymmetry(Vector *edges, bool weighted) {
  size_t original = VectorSize(edges);
  if (original == 0)
    return;
  if (weighted) {
    WEdge *elist = (WEdge *)VectorData(edges);
    VectorResize(edges, original * 2);
    elist = (WEdge *)VectorData(edges);
    for (size_t i = 0; i < original; ++i) {
      WEdge rev;
      rev.u = elist[i].v.v;
      rev.v.v = elist[i].u;
      rev.v.w = elist[i].v.w;
      elist[original + i] = rev;
    }
  } else {
    Edge *elist = (Edge *)VectorData(edges);
    VectorResize(edges, original * 2);
    elist = (Edge *)VectorData(edges);
    for (size_t i = 0; i < original; ++i) {
      Edge rev;
      rev.u = elist[i].v;
      rev.v = elist[i].u;
      elist[original + i] = rev;
    }
  }
}

static void BuilderProcessEdges(Vector *edges, bool weighted, bool symmetrize) {
  if (symmetrize)
    BuilderAppendSymmetry(edges, weighted);
  BuilderDeduplicateEdges(edges, weighted);
}

static void BuilderBuildGraph(const CLBase *cli, bool weighted, Graph *graph,
                              WGraph *wgraph) {
  bool symmetrize = CLBaseSymmetrize(cli);
  const char *filename = CLBaseFilename(cli);
  Vector edges;
  int needs_weights = 0;
  int64_t num_nodes = -1;
  if (filename != NULL && filename[0] != '\0') {
    if (BuilderInitEdgesFromFile(cli, weighted, &edges, &needs_weights,
                                 &num_nodes, graph, wgraph)) {
      return;
    }
  } else {
    int scale = CLBaseScale(cli);
    int degree = CLBaseDegree(cli);
    int uniform = CLBaseUniform(cli) ? 1 : 0;
    BuilderInitEdgesFromGenerator(weighted, uniform, scale, degree,
                                  &edges, &num_nodes, &needs_weights);
  }
  Timer build_timer;
  TimerStart(&build_timer);
  if (weighted && needs_weights)
    GeneratorInsertWeights(&edges);
  if (num_nodes < 0)
    num_nodes = BuilderMaxNode(&edges, weighted) + 1;
  BuilderProcessEdges(&edges, weighted, symmetrize);
  if (weighted) {
    BuilderWeightedEdgesToCSR(&edges, num_nodes, symmetrize, wgraph);
  } else {
    BuilderEdgesToCSR(&edges, num_nodes, symmetrize, graph);
  }
  TimerStop(&build_timer);
  PrintTime("Build Time", TimerSeconds(&build_timer));
  VectorFree(&edges);
}

typedef struct {
  int64_t degree;
  NodeID node;
} DegreeNode;

static int CompareDegreeNodeDesc(const void *a, const void *b) {
  const DegreeNode *da = (const DegreeNode *)a;
  const DegreeNode *db = (const DegreeNode *)b;
  if (da->degree == db->degree)
    return (da->node < db->node) ? -1 : 1;
  return (da->degree < db->degree) ? 1 : -1;
}

static Graph BuilderRelabelByDegree(const Graph *g) {
  if (GraphDirected(g)) {
    printf("Cannot relabel directed graph\n");
    exit(-11);
  }
  Timer relabel_timer;
  TimerStart(&relabel_timer);
  int64_t num_nodes = GraphNumNodes(g);
  DegreeNode *pairs = (DegreeNode *)malloc(sizeof(DegreeNode) * (size_t)num_nodes);
  if (pairs == NULL && num_nodes > 0)
    exit(EXIT_FAILURE);
  for (NodeID n = 0; n < num_nodes; ++n) {
    pairs[n].degree = GraphOutDegree(g, n);
    pairs[n].node = n;
  }
  qsort(pairs, (size_t)num_nodes, sizeof(DegreeNode), CompareDegreeNodeDesc);
  NodeID *degrees = (NodeID *)malloc(sizeof(NodeID) * (size_t)num_nodes);
  NodeID *new_ids = (NodeID *)malloc(sizeof(NodeID) * (size_t)num_nodes);
  if ((degrees == NULL || new_ids == NULL) && num_nodes > 0)
    exit(EXIT_FAILURE);
  for (NodeID n = 0; n < num_nodes; ++n) {
    degrees[n] = (NodeID)pairs[n].degree;
    new_ids[pairs[n].node] = n;
  }
  SGOffset *offsets = (SGOffset *)malloc(sizeof(SGOffset) * (size_t)(num_nodes + 1));
  if (offsets == NULL && num_nodes > 0)
    exit(EXIT_FAILURE);
  SGOffset total = 0;
  for (NodeID n = 0; n < num_nodes; ++n) {
    offsets[n] = total;
    total += degrees[n];
  }
  offsets[num_nodes] = total;
  NodeID *neighs = (NodeID *)malloc(sizeof(NodeID) * (size_t)total);
  if (neighs == NULL && total > 0)
    exit(EXIT_FAILURE);
  NodeID **index = GraphBuildIndex(offsets, (size_t)num_nodes + 1, neighs);
  SGOffset *write_offsets = (SGOffset *)malloc(sizeof(SGOffset) * (size_t)(num_nodes + 1));
  if (write_offsets == NULL)
    exit(EXIT_FAILURE);
  memcpy(write_offsets, offsets, sizeof(SGOffset) * (size_t)(num_nodes + 1));
  for (NodeID u = 0; u < num_nodes; ++u) {
    NodeID new_u = new_ids[u];
    NodeID *begin = GraphOutNeighborsBegin(g, u);
    NodeID *end = GraphOutNeighborsEnd(g, u);
    for (NodeID *nbr = begin; nbr < end; ++nbr) {
      NodeID new_v = new_ids[*nbr];
      neighs[write_offsets[new_u]++] = new_v;
    }
  }
  free(write_offsets);
  for (NodeID n = 0; n < num_nodes; ++n) {
    NodeID *start = index[n];
    NodeID *end = index[n + 1];
    size_t count = (size_t)(end - start);
    if (count > 1)
      qsort(start, count, sizeof(NodeID), CompareNodeIDAsc);
  }
  Graph relabeled;
  GraphInit(&relabeled, false, num_nodes, GraphNumEdges(g),
            index, neighs, index, neighs);
  TimerStop(&relabel_timer);
  PrintTime("Relabel", TimerSeconds(&relabel_timer));
  free(pairs);
  free(degrees);
  free(new_ids);
  free(offsets);
  return relabeled;
}

static inline void BuilderInit(Builder *builder, const CLBase *cli) {
  builder->cli = cli;
  builder->symmetrize = CLBaseSymmetrize(cli);
}

static inline void WeightedBuilderInit(WeightedBuilder *builder,
                                       const CLBase *cli) {
  builder->cli = cli;
  builder->symmetrize = CLBaseSymmetrize(cli);
}

static inline void BuilderMakeGraph(Builder *builder, Graph *graph) {
  BuilderBuildGraph(builder->cli, false, graph, NULL);
}

static inline void WeightedBuilderMakeGraph(WeightedBuilder *builder,
                                            WGraph *graph) {
  BuilderBuildGraph(builder->cli, true, NULL, graph);
}

#endif  // BUILDER_H_
