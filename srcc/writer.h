#ifndef WRITER_H_
#define WRITER_H_

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "util.h"

typedef struct {
  Graph *graph;
} Writer;

typedef struct {
  WGraph *graph;
} WeightedWriter;

static inline void WriterInit(Writer *writer, Graph *graph) {
  writer->graph = graph;
}

static inline void WeightedWriterInit(WeightedWriter *writer, WGraph *graph) {
  writer->graph = graph;
}

static inline void WriterWriteELToFile(const Graph *graph, FILE *fp) {
  for (NodeID u = 0; u < GraphNumNodes(graph); ++u) {
    for (NodeID *it = GraphOutNeighborsBegin(graph, u);
         it < GraphOutNeighborsEnd(graph, u); ++it) {
      fprintf(fp, "%d %d\n", u, *it);
    }
  }
}

static inline void WeightedWriterWriteELToFile(const WGraph *graph, FILE *fp) {
  for (NodeID u = 0; u < GraphNumNodesW(graph); ++u) {
    for (NodeWeight *it = WGraphOutNeighborsBegin(graph, u);
         it < WGraphOutNeighborsEnd(graph, u); ++it) {
      fprintf(fp, "%d %d %d\n", u, it->v, it->w);
    }
  }
}

static inline void WriterWriteSerialized(const Graph *graph, FILE *fp) {
  if (sizeof(NodeID) != sizeof(SGID)) {
    printf("serialized graphs only allowed for 32b IDs\n");
    exit(-4);
  }
  bool directed = GraphDirected(graph);
  SGOffset num_nodes = GraphNumNodes(graph);
  SGOffset edges_to_write = GraphNumEdgesDirected(graph);
  size_t index_bytes = (size_t)(num_nodes + 1) * sizeof(SGOffset);
  size_t neigh_bytes = (size_t)edges_to_write * sizeof(SGID);
  fwrite(&directed, sizeof(bool), 1, fp);
  fwrite(&edges_to_write, sizeof(SGOffset), 1, fp);
  fwrite(&num_nodes, sizeof(SGOffset), 1, fp);
  Vector offsets;
  GraphVertexOffsets(graph, 0, &offsets);
  fwrite(VectorData(&offsets), index_bytes, 1, fp);
  fwrite(GraphOutNeighborsBegin(graph, 0), neigh_bytes, 1, fp);
  if (directed) {
    VectorFree(&offsets);
    GraphVertexOffsets(graph, 1, &offsets);
    fwrite(VectorData(&offsets), index_bytes, 1, fp);
    fwrite(GraphInNeighborsBegin(graph, 0), neigh_bytes, 1, fp);
  }
  VectorFree(&offsets);
}

static inline void WeightedWriterWriteSerialized(const WGraph *graph, FILE *fp) {
  if (sizeof(NodeID) != sizeof(SGID)) {
    printf("serialized graphs only allowed for 32b IDs\n");
    exit(-4);
  }
  bool directed = WGraphDirected(graph);
  SGOffset num_nodes = GraphNumNodesW(graph);
  SGOffset edges_to_write = GraphNumEdgesDirectedW(graph);
  size_t index_bytes = (size_t)(num_nodes + 1) * sizeof(SGOffset);
  size_t neigh_bytes = (size_t)edges_to_write * sizeof(NodeWeight);
  fwrite(&directed, sizeof(bool), 1, fp);
  fwrite(&edges_to_write, sizeof(SGOffset), 1, fp);
  fwrite(&num_nodes, sizeof(SGOffset), 1, fp);
  Vector offsets;
  WGraphVertexOffsets(graph, 0, &offsets);
  fwrite(VectorData(&offsets), index_bytes, 1, fp);
  fwrite(WGraphOutNeighborsBegin(graph, 0), neigh_bytes, 1, fp);
  if (directed) {
    VectorFree(&offsets);
    WGraphVertexOffsets(graph, 1, &offsets);
    fwrite(VectorData(&offsets), index_bytes, 1, fp);
    fwrite(WGraphInNeighborsBegin(graph, 0), neigh_bytes, 1, fp);
  }
  VectorFree(&offsets);
}

static inline void WriterWriteGraph(Writer *writer, const char *filename, int serialized) {
  if (filename == NULL || filename[0] == '\0') {
    printf("No output filename given (Use -h for help)\n");
    exit(-8);
  }
  FILE *fp = fopen(filename, serialized ? "wb" : "w");
  if (fp == NULL) {
    printf("Couldn't write to file %s: %s\n", filename, strerror(errno));
    exit(-5);
  }
  if (serialized)
    WriterWriteSerialized(writer->graph, fp);
  else
    WriterWriteELToFile(writer->graph, fp);
  fclose(fp);
}

static inline void WeightedWriterWriteGraph(WeightedWriter *writer,
                                            const char *filename, int serialized) {
  if (filename == NULL || filename[0] == '\0') {
    printf("No output filename given (Use -h for help)\n");
    exit(-8);
  }
  FILE *fp = fopen(filename, serialized ? "wb" : "w");
  if (fp == NULL) {
    printf("Couldn't write to file %s: %s\n", filename, strerror(errno));
    exit(-5);
  }
  if (serialized)
    WeightedWriterWriteSerialized(writer->graph, fp);
  else
    WeightedWriterWriteELToFile(writer->graph, fp);
  fclose(fp);
}

#endif  // WRITER_H_
