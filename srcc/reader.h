#ifndef READER_H_
#define READER_H_

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "graph.h"
#include "timer.h"
#include "util.h"
#include "vector.h"

typedef struct {
  char *filename;
} Reader;

static inline char *ReaderDupString(const char *s) {
  size_t len = strlen(s) + 1;
  char *copy = (char *)malloc(len);
  if (copy == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memcpy(copy, s, len);
  return copy;
}

static inline void ReaderInit(Reader *reader, const char *filename) {
  reader->filename = ReaderDupString(filename);
}

static inline void ReaderFree(Reader *reader) {
  free(reader->filename);
  reader->filename = NULL;
}

static inline const char *ReaderGetSuffix(const Reader *reader) {
  const char *suffix = strrchr(reader->filename, '.');
  if (suffix == NULL) {
    printf("Couldn't find suffix of %s\n", reader->filename);
    exit(-1);
  }
  return suffix;
}

static inline void ReaderPushEdge(Vector *edges, int expects_weighted,
                                  NodeID u, NodeID v, WeightT w) {
  if (expects_weighted) {
    WEdge edge;
    edge.u = u;
    edge.v.v = v;
    edge.v.w = w;
    VectorPushBack(edges, &edge);
  } else {
    Edge edge;
    edge.u = u;
    edge.v = v;
    VectorPushBack(edges, &edge);
  }
}

static inline void ReaderReadEL(const Reader *reader, int expects_weighted,
                                Vector *edges) {
  FILE *fp = fopen(reader->filename, "r");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  NodeID u, v;
  while (fscanf(fp, "%d %d", &u, &v) == 2) {
    ReaderPushEdge(edges, expects_weighted, u, v, 1);
  }
  fclose(fp);
}

static inline void ReaderReadWEL(const Reader *reader, Vector *edges) {
  FILE *fp = fopen(reader->filename, "r");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  NodeID u, v;
  WeightT w;
  while (fscanf(fp, "%d %d %d", &u, &v, &w) == 3) {
    ReaderPushEdge(edges, 1, u, v, w);
  }
  fclose(fp);
}

static inline void ReaderReadGR(const Reader *reader, int expects_weighted,
                                Vector *edges) {
  FILE *fp = fopen(reader->filename, "r");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  char line[1024];
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == 'a') {
      char type;
      NodeID u;
      NodeID v;
      WeightT w = 1;
      if (sscanf(line, "%c %d %d %d", &type, &u, &v, &w) >= 3) {
        u -= 1;
        v -= 1;
        ReaderPushEdge(edges, expects_weighted, u, v, w);
      }
    }
  }
  fclose(fp);
}

static inline void ReaderReadMetis(const Reader *reader, int expects_weighted,
                                   Vector *edges, int *needs_weights) {
  FILE *fp = fopen(reader->filename, "r");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  char line[4096];
  NodeID num_nodes = 0;
  int read_weights = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == '%')
      continue;
    int fmt = 0;
    int64_t dummy_edges;
    int count = sscanf(line, "%d %lld %d", &num_nodes, &dummy_edges, &fmt);
    if (count >= 2) {
      if (fmt == 1)
        read_weights = 1;
      else if (fmt != 0 && fmt != 100) {
        printf("Unsupported METIS fmt type: %d\n", fmt);
        exit(-20);
      }
      break;
    }
  }
  NodeID u = 0;
  while (u < num_nodes && fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == '%')
      continue;
    char *token = strtok(line, " \t\r\n");
    while (token != NULL) {
      NodeID v = atoi(token) - 1;
      WeightT w = 1;
      if (read_weights) {
        token = strtok(NULL, " \t\r\n");
        if (token == NULL)
          break;
        w = (WeightT)atoi(token);
      }
      ReaderPushEdge(edges, expects_weighted, u, v, w);
      token = strtok(NULL, " \t\r\n");
    }
    u++;
  }
  fclose(fp);
  if (needs_weights != NULL)
    *needs_weights = expects_weighted && !read_weights;
}

static inline void ReaderReadMTX(const Reader *reader, int expects_weighted,
                                 Vector *edges, int *needs_weights) {
  FILE *fp = fopen(reader->filename, "r");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  char header[512];
  if (fgets(header, sizeof(header), fp) == NULL) {
    printf("Empty .mtx file\n");
    exit(-21);
  }
  char start[32], object[32], format[32], field[32], symmetry[32];
  if (sscanf(header, "%31s %31s %31s %31s %31s",
             start, object, format, field, symmetry) != 5) {
    printf(".mtx header malformed\n");
    exit(-21);
  }
  if (strcmp(start, "%%MatrixMarket") != 0 ||
      strcmp(object, "matrix") != 0 ||
      strcmp(format, "coordinate") != 0) {
    printf("Only MatrixMarket coordinate format supported\n");
    exit(-22);
  }
  int read_weights = 0;
  if (strcmp(field, "pattern") == 0) {
    read_weights = 0;
  } else if (strcmp(field, "real") == 0 ||
             strcmp(field, "double") == 0 ||
             strcmp(field, "integer") == 0) {
    read_weights = 1;
  } else {
    printf("Unsupported field type for .mtx\n");
    exit(-24);
  }
  int undirected = 0;
  if (strcmp(symmetry, "symmetric") == 0) {
    undirected = 1;
  } else if (strcmp(symmetry, "general") == 0 ||
             strcmp(symmetry, "skew-symmetric") == 0) {
    undirected = 0;
  } else {
    printf("Unsupported symmetry type for .mtx\n");
    exit(-25);
  }
  char line[512];
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] != '%')
      break;
  }
  if (feof(fp)) {
    printf(".mtx missing size line\n");
    exit(-26);
  }
  int64_t m, n, nonzeros;
  if (sscanf(line, "%lld %lld %lld", &m, &n, &nonzeros) != 3) {
    printf(".mtx size line malformed\n");
    exit(-26);
  }
  if (m != n) {
    printf("Matrix must be square for .mtx\n");
    exit(-26);
  }
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == '%' || line[0] == '\n')
      continue;
    NodeID u, v;
    WeightT w = 1;
    if (read_weights) {
      double weight_val;
      if (sscanf(line, "%d %d %lf", &u, &v, &weight_val) < 3)
        continue;
      w = (WeightT)weight_val;
    } else {
      if (sscanf(line, "%d %d", &u, &v) < 2)
        continue;
    }
    u -= 1;
    v -= 1;
    ReaderPushEdge(edges, expects_weighted, u, v, w);
    if (undirected && u != v)
      ReaderPushEdge(edges, expects_weighted, v, u, w);
  }
  fclose(fp);
  if (needs_weights != NULL)
    *needs_weights = expects_weighted && !read_weights;
}

static inline void ReaderReadEdgeList(const Reader *reader, int expects_weighted,
                                      Vector *edges, int *needs_weights) {
  const char *suffix = ReaderGetSuffix(reader);
  Timer t;
  TimerStart(&t);
  if (expects_weighted)
    VectorInit(edges, sizeof(WEdge));
  else
    VectorInit(edges, sizeof(Edge));
  int local_needs_weights = expects_weighted ? 1 : 0;
  if (strcmp(suffix, ".el") == 0) {
    ReaderReadEL(reader, expects_weighted, edges);
  } else if (strcmp(suffix, ".wel") == 0) {
    ReaderReadWEL(reader, edges);
    local_needs_weights = 0;
  } else if (strcmp(suffix, ".gr") == 0) {
    ReaderReadGR(reader, expects_weighted, edges);
    local_needs_weights = 0;
  } else if (strcmp(suffix, ".graph") == 0) {
    ReaderReadMetis(reader, expects_weighted, edges, &local_needs_weights);
  } else if (strcmp(suffix, ".mtx") == 0) {
    ReaderReadMTX(reader, expects_weighted, edges, &local_needs_weights);
  } else {
    printf("Unrecognized suffix for edge list: %s\n", suffix);
    exit(-3);
  }
  if (needs_weights != NULL)
    *needs_weights = local_needs_weights;
  TimerStop(&t);
  PrintTime("Read Time", TimerSeconds(&t));
}

static inline void ReaderCheckSerializedTypes(int weighted) {
  if (sizeof(NodeID) != sizeof(SGID)) {
    printf("Serialized graphs only allowed for 32-bit NodeID\n");
    exit(-5);
  }
  if (weighted && sizeof(WeightT) != sizeof(SGID)) {
    printf(".wsg only allowed for int32_t weights\n");
    exit(-5);
  }
}

static inline void ReaderReadSerializedGraph(const Reader *reader,
                                             Graph *graph,
                                             int invert) {
  ReaderCheckSerializedTypes(0);
  const char *suffix = ReaderGetSuffix(reader);
  if (strcmp(suffix, ".sg") != 0) {
    printf("ReaderReadSerializedGraph requires .sg input\n");
    exit(-6);
  }
  FILE *fp = fopen(reader->filename, "rb");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  Timer t;
  TimerStart(&t);
  bool directed;
  SGOffset num_edges, num_nodes;
  if (fread(&directed, sizeof(bool), 1, fp) != 1 ||
      fread(&num_edges, sizeof(SGOffset), 1, fp) != 1 ||
      fread(&num_nodes, sizeof(SGOffset), 1, fp) != 1) {
    printf("Failed reading serialized graph header\n");
    exit(-6);
  }
  size_t index_bytes = (size_t)(num_nodes + 1) * sizeof(SGOffset);
  size_t neigh_bytes = (size_t)num_edges * sizeof(SGID);
  SGOffset *offsets = (SGOffset *)malloc(index_bytes);
  if (offsets == NULL) {
    printf("malloc failed for offsets\n");
    exit(-6);
  }
  NodeID *neighs = (NodeID *)malloc(neigh_bytes);
  if (neighs == NULL) {
    printf("malloc failed for neighbors\n");
    exit(-6);
  }
  if (fread(offsets, index_bytes, 1, fp) != 1 ||
      fread(neighs, neigh_bytes, 1, fp) != 1) {
    printf("Failed reading serialized graph data\n");
    exit(-6);
  }
  NodeID **out_index = GraphBuildIndex(offsets, (size_t)num_nodes + 1, neighs);
  NodeID **in_index = NULL;
  NodeID *in_neighs = NULL;
  if (directed && invert) {
    in_neighs = (NodeID *)malloc(neigh_bytes);
    if (in_neighs == NULL)
      exit(-6);
    if (fread(offsets, index_bytes, 1, fp) != 1 ||
        fread(in_neighs, neigh_bytes, 1, fp) != 1) {
      printf("Failed reading inverse neighbors\n");
      exit(-6);
    }
    in_index = GraphBuildIndex(offsets, (size_t)num_nodes + 1, in_neighs);
  } else if (!directed) {
    in_index = out_index;
    in_neighs = neighs;
  }
  free(offsets);
  fclose(fp);
  TimerStop(&t);
  PrintTime("Read Time", TimerSeconds(&t));
  GraphInit(graph, directed, num_nodes, directed ? num_edges : num_edges / 2,
            out_index, neighs, in_index, in_neighs);
}

static inline void ReaderReadSerializedWeightedGraph(const Reader *reader,
                                                     WGraph *graph,
                                                     int invert) {
  ReaderCheckSerializedTypes(1);
  const char *suffix = ReaderGetSuffix(reader);
  if (strcmp(suffix, ".wsg") != 0) {
    printf("ReaderReadSerializedWeightedGraph requires .wsg input\n");
    exit(-6);
  }
  FILE *fp = fopen(reader->filename, "rb");
  if (fp == NULL) {
    printf("Couldn't open file %s: %s\n", reader->filename, strerror(errno));
    exit(-2);
  }
  Timer t;
  TimerStart(&t);
  bool directed;
  SGOffset num_edges, num_nodes;
  if (fread(&directed, sizeof(bool), 1, fp) != 1 ||
      fread(&num_edges, sizeof(SGOffset), 1, fp) != 1 ||
      fread(&num_nodes, sizeof(SGOffset), 1, fp) != 1) {
    printf("Failed reading serialized graph header\n");
    exit(-6);
  }
  size_t index_bytes = (size_t)(num_nodes + 1) * sizeof(SGOffset);
  size_t neigh_bytes = (size_t)num_edges * sizeof(NodeWeight);
  SGOffset *offsets = (SGOffset *)malloc(index_bytes);
  if (offsets == NULL)
    exit(-6);
  NodeWeight *neighs = (NodeWeight *)malloc(neigh_bytes);
  if (neighs == NULL)
    exit(-6);
  if (fread(offsets, index_bytes, 1, fp) != 1 ||
      fread(neighs, neigh_bytes, 1, fp) != 1) {
    printf("Failed reading serialized weighted graph data\n");
    exit(-6);
  }
  NodeWeight **out_index = WGraphBuildIndex(offsets, (size_t)num_nodes + 1, neighs);
  NodeWeight **in_index = NULL;
  NodeWeight *in_neighs = NULL;
  if (directed && invert) {
    in_neighs = (NodeWeight *)malloc(neigh_bytes);
    if (in_neighs == NULL)
      exit(-6);
    if (fread(offsets, index_bytes, 1, fp) != 1 ||
        fread(in_neighs, neigh_bytes, 1, fp) != 1) {
      printf("Failed reading inverse weighted neighbors\n");
      exit(-6);
    }
    in_index = WGraphBuildIndex(offsets, (size_t)num_nodes + 1, in_neighs);
  } else if (!directed) {
    in_index = out_index;
    in_neighs = neighs;
  }
  free(offsets);
  fclose(fp);
  TimerStop(&t);
  PrintTime("Read Time", TimerSeconds(&t));
  WGraphInit(graph, directed, num_nodes,
             directed ? num_edges : num_edges / 2,
             out_index, neighs, in_index, in_neighs);
}

#endif  // READER_H_
