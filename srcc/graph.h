#ifndef GRAPH_H_
#define GRAPH_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "vector.h"

typedef struct {
  int directed;
  int64_t num_nodes;
  int64_t num_edges;
  NodeID **out_index;
  NodeID *out_neighbors;
  NodeID **in_index;
  NodeID *in_neighbors;
} Graph;

typedef struct {
  int directed;
  int64_t num_nodes;
  int64_t num_edges;
  NodeWeight **out_index;
  NodeWeight *out_neighbors;
  NodeWeight **in_index;
  NodeWeight *in_neighbors;
} WGraph;

static inline void GraphInit(Graph *g, int directed, int64_t num_nodes, int64_t num_edges,
                             NodeID **out_index, NodeID *out_neighbors,
                             NodeID **in_index, NodeID *in_neighbors) {
  g->directed = directed;
  g->num_nodes = num_nodes;
  g->num_edges = num_edges;
  g->out_index = out_index;
  g->out_neighbors = out_neighbors;
  g->in_index = in_index;
  g->in_neighbors = in_neighbors;
}

static inline void WGraphInit(WGraph *g, int directed, int64_t num_nodes, int64_t num_edges,
                              NodeWeight **out_index, NodeWeight *out_neighbors,
                              NodeWeight **in_index, NodeWeight *in_neighbors) {
  g->directed = directed;
  g->num_nodes = num_nodes;
  g->num_edges = num_edges;
  g->out_index = out_index;
  g->out_neighbors = out_neighbors;
  g->in_index = in_index;
  g->in_neighbors = in_neighbors;
}

static inline void GraphFree(Graph *g) {
  free(g->out_index);
  free(g->out_neighbors);
  if (g->directed) {
    free(g->in_index);
    free(g->in_neighbors);
  }
  g->out_index = NULL;
  g->out_neighbors = NULL;
  g->in_index = NULL;
  g->in_neighbors = NULL;
}

static inline void WGraphFree(WGraph *g) {
  free(g->out_index);
  free(g->out_neighbors);
  if (g->directed) {
    free(g->in_index);
    free(g->in_neighbors);
  }
  g->out_index = NULL;
  g->out_neighbors = NULL;
  g->in_index = NULL;
  g->in_neighbors = NULL;
}

static inline int GraphDirected(const Graph *g) {
  return g->directed;
}

static inline int WGraphDirected(const WGraph *g) {
  return g->directed;
}

static inline int64_t GraphNumNodes(const Graph *g) {
  return g->num_nodes;
}

static inline int64_t GraphNumNodesW(const WGraph *g) {
  return g->num_nodes;
}

static inline int64_t GraphNumEdges(const Graph *g) {
  return g->num_edges;
}

static inline int64_t GraphNumEdgesW(const WGraph *g) {
  return g->num_edges;
}

static inline int64_t GraphNumEdgesDirected(const Graph *g) {
  return g->directed ? g->num_edges : g->num_edges * 2;
}

static inline int64_t GraphNumEdgesDirectedW(const WGraph *g) {
  return g->directed ? g->num_edges : g->num_edges * 2;
}

static inline int64_t GraphOutDegree(const Graph *g, NodeID v) {
  return g->out_index[v + 1] - g->out_index[v];
}

static inline int64_t GraphOutDegreeW(const WGraph *g, NodeID v) {
  return g->out_index[v + 1] - g->out_index[v];
}

static inline int64_t WGraphOutDegree(const WGraph *g, NodeID v) {
  return GraphOutDegreeW(g, v);
}

static inline int64_t GraphInDegree(const Graph *g, NodeID v) {
  return g->in_index[v + 1] - g->in_index[v];
}

static inline int64_t GraphInDegreeW(const WGraph *g, NodeID v) {
  return g->in_index[v + 1] - g->in_index[v];
}

static inline int64_t WGraphInDegree(const WGraph *g, NodeID v) {
  return GraphInDegreeW(g, v);
}

static inline NodeID *GraphOutNeighborsBegin(const Graph *g, NodeID v) {
  return g->out_index[v];
}

static inline NodeID *GraphOutNeighborsEnd(const Graph *g, NodeID v) {
  return g->out_index[v + 1];
}

static inline NodeWeight *WGraphOutNeighborsBegin(const WGraph *g, NodeID v) {
  return g->out_index[v];
}

static inline NodeWeight *WGraphOutNeighborsEnd(const WGraph *g, NodeID v) {
  return g->out_index[v + 1];
}

static inline NodeID *GraphOutNeighborsOffset(const Graph *g, NodeID v, size_t offset) {
  NodeID *begin = GraphOutNeighborsBegin(g, v);
  NodeID *end = GraphOutNeighborsEnd(g, v);
  size_t length = (size_t)(end - begin);
  if (offset > length)
    offset = length;
  return begin + offset;
}

static inline NodeID *GraphInNeighborsBegin(const Graph *g, NodeID v) {
  return g->in_index[v];
}

static inline NodeID *GraphInNeighborsEnd(const Graph *g, NodeID v) {
  return g->in_index[v + 1];
}

static inline NodeWeight *WGraphInNeighborsBegin(const WGraph *g, NodeID v) {
  return g->in_index[v];
}

static inline NodeWeight *WGraphInNeighborsEnd(const WGraph *g, NodeID v) {
  return g->in_index[v + 1];
}

static inline NodeID *GraphNeighborStorageBegin(const Graph *g) {
  return g->out_index[0];
}

static inline void GraphPrintStats(const Graph *g) {
  printf("Graph has %" PRId64 " nodes and %" PRId64 " %s edges for degree: %" PRId64 "\n",
         g->num_nodes, g->num_edges, g->directed ? "directed" : "undirected",
         g->num_nodes == 0 ? 0 : g->num_edges / g->num_nodes);
}

static inline void WGraphPrintStats(const WGraph *g) {
  printf("Graph has %" PRId64 " nodes and %" PRId64 " %s edges for degree: %" PRId64 "\n",
         g->num_nodes, g->num_edges, g->directed ? "directed" : "undirected",
         g->num_nodes == 0 ? 0 : g->num_edges / g->num_nodes);
}

static inline void GraphPrintTopology(const Graph *g) {
  for (NodeID i = 0; i < g->num_nodes; ++i) {
    printf("%d: ", i);
    for (NodeID *it = GraphOutNeighborsBegin(g, i); it < GraphOutNeighborsEnd(g, i); ++it) {
      printf("%d ", *it);
    }
    printf("\n");
  }
}

static inline void GraphVertexOffsets(const Graph *g, int in_graph, Vector *offsets) {
  VectorInit(offsets, sizeof(SGOffset));
  VectorResize(offsets, g->num_nodes + 1);
  for (NodeID n = 0; n < g->num_nodes + 1; ++n) {
    SGOffset value;
    if (in_graph && g->in_index != NULL)
      value = g->in_index[n] - g->in_index[0];
    else
      value = g->out_index[n] - g->out_index[0];
    VECTOR_AT(offsets, SGOffset, n) = value;
  }
}

static inline void WGraphVertexOffsets(const WGraph *g, int in_graph, Vector *offsets) {
  VectorInit(offsets, sizeof(SGOffset));
  VectorResize(offsets, g->num_nodes + 1);
  for (NodeID n = 0; n < g->num_nodes + 1; ++n) {
    SGOffset value;
    if (in_graph && g->in_index != NULL)
      value = g->in_index[n] - g->in_index[0];
    else
      value = g->out_index[n] - g->out_index[0];
    VECTOR_AT(offsets, SGOffset, n) = value;
  }
}

static inline NodeID **GraphBuildIndex(const SGOffset *offsets, size_t length, NodeID *neighbors) {
  NodeID **index = (NodeID **)malloc(sizeof(NodeID *) * length);
  if (index == NULL)
    return NULL;
  for (size_t n = 0; n < length; ++n)
    index[n] = neighbors + offsets[n];
  return index;
}

static inline NodeWeight **WGraphBuildIndex(const SGOffset *offsets, size_t length,
                                            NodeWeight *neighbors) {
  NodeWeight **index = (NodeWeight **)malloc(sizeof(NodeWeight *) * length);
  if (index == NULL)
    return NULL;
  for (size_t n = 0; n < length; ++n)
    index[n] = neighbors + offsets[n];
  return index;
}

#endif  // GRAPH_H_
