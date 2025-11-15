#ifndef COMMAND_LINE_H_
#define COMMAND_LINE_H_

#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vector.h"

static inline char *CLDupString(const char *src) {
  size_t len = strlen(src) + 1;
  char *copy = (char *)malloc(len);
  if (copy == NULL)
    abort();
  memcpy(copy, src, len);
  return copy;
}

typedef struct {
  int argc;
  char **argv;
  const char *name;
  char get_args[64];
  Vector help_strings;  // stores char*
  int scale;
  int degree;
  const char *filename;
  bool symmetrize;
  bool uniform_graph;
  bool in_place;
} CLBase;

typedef int (*OptionHandler)(void *ctx, signed char opt, char *optarg);

static inline void CLBaseInit(CLBase *cl, int argc, char **argv, const char *name) {
  cl->argc = argc;
  cl->argv = argv;
  cl->name = name;
  snprintf(cl->get_args, sizeof(cl->get_args), "f:g:hk:su:m");
  VectorInit(&cl->help_strings, sizeof(char *));
  cl->scale = -1;
  cl->degree = 16;
  cl->filename = NULL;
  cl->symmetrize = false;
  cl->uniform_graph = false;
  cl->in_place = false;
}

static inline void CLBaseAddHelpLine(CLBase *cl, char opt, const char *opt_arg,
                                     const char *text, const char *def) {
  const int kBufLen = 100;
  char buf[kBufLen];
  char opt_buf[32] = "";
  char def_buf[32] = "";
  if (opt_arg != NULL && opt_arg[0] != '\0')
    snprintf(opt_buf, sizeof(opt_buf), "<%s>", opt_arg);
  if (def != NULL && def[0] != '\0')
    snprintf(def_buf, sizeof(def_buf), "[%s]", def);
  snprintf(buf, kBufLen, " -%c %-9s: %-54s%10s", opt, opt_buf, text, def_buf);
  char *line = CLDupString(buf);
  VectorPushBack(&cl->help_strings, &line);
}

static inline void CLBasePrintUsage(CLBase *cl) {
  printf("%s\n", cl->name != NULL ? cl->name : "");
  for (size_t i = 0; i < VectorSize(&cl->help_strings); ++i) {
    char *line = VECTOR_AT(&cl->help_strings, char *, i);
    printf("%s\n", line);
  }
  exit(0);
}

static inline int CLBaseHandleArg(CLBase *cl, signed char opt, char *optarg) {
  switch (opt) {
    case 'f':
      cl->filename = optarg;
      return 1;
    case 'g':
      cl->scale = atoi(optarg);
      return 1;
    case 'h':
      CLBasePrintUsage(cl);
      return 1;
    case 'k':
      cl->degree = atoi(optarg);
      return 1;
    case 's':
      cl->symmetrize = true;
      return 1;
    case 'u':
      cl->uniform_graph = true;
      cl->scale = atoi(optarg);
      return 1;
    case 'm':
      cl->in_place = true;
      return 1;
    default:
      return 0;
  }
}

static inline void CLBaseFree(CLBase *cl) {
  for (size_t i = 0; i < VectorSize(&cl->help_strings); ++i) {
    char *line = VECTOR_AT(&cl->help_strings, char *, i);
    free(line);
  }
  VectorFree(&cl->help_strings);
}

static inline bool CLBaseParseArgs(CLBase *cl, OptionHandler handler, void *ctx) {
  optind = 1;
  int c_opt;
  while ((c_opt = getopt(cl->argc, cl->argv, cl->get_args)) != -1) {
    int handled = 0;
    if (handler != NULL)
      handled = handler(ctx, (signed char)c_opt, optarg);
    if (!handled)
      CLBaseHandleArg(cl, (signed char)c_opt, optarg);
  }
  if (cl->filename == NULL && cl->scale == -1) {
    printf("No graph input specified. (Use -h for help)\n");
    return false;
  }
  if (cl->scale != -1)
    cl->symmetrize = true;
  return true;
}

typedef struct {
  CLBase base;
  bool do_analysis;
  int num_trials;
  int64_t start_vertex;
  bool do_verify;
  bool enable_logging;
} CLApp;

static inline void CLAppInit(CLApp *app, int argc, char **argv, const char *name) {
  CLBaseInit(&app->base, argc, argv, name);
  strncat(app->base.get_args, "an:r:vl", sizeof(app->base.get_args) - strlen(app->base.get_args) - 1);
  app->do_analysis = false;
  app->num_trials = 16;
  app->start_vertex = -1;
  app->do_verify = false;
  app->enable_logging = false;
  CLBaseAddHelpLine(&app->base, 'a', "", "output analysis of last run", "false");
  CLBaseAddHelpLine(&app->base, 'n', "n", "perform n trials", "16");
  CLBaseAddHelpLine(&app->base, 'r', "node", "start from node r", "rand");
  CLBaseAddHelpLine(&app->base, 'v', "", "verify the output of each run", "false");
  CLBaseAddHelpLine(&app->base, 'l', "", "log performance within each trial", "false");
}

static inline int CLAppHandleOption(void *ctx, signed char opt, char *optarg) {
  CLApp *app = (CLApp *)ctx;
  switch (opt) {
    case 'a':
      app->do_analysis = true;
      return 1;
    case 'n':
      app->num_trials = atoi(optarg);
      return 1;
    case 'r':
      app->start_vertex = atoll(optarg);
      return 1;
    case 'v':
      app->do_verify = true;
      return 1;
    case 'l':
      app->enable_logging = true;
      return 1;
    default:
      return 0;
  }
}

static inline bool CLAppParseArgs(CLApp *app) {
  return CLBaseParseArgs(&app->base, CLAppHandleOption, app);
}

static inline bool CLAppDoAnalysis(const CLApp *app) { return app->do_analysis; }
static inline int CLAppNumTrials(const CLApp *app) { return app->num_trials; }
static inline int64_t CLAppStartVertex(const CLApp *app) { return app->start_vertex; }
static inline bool CLAppDoVerify(const CLApp *app) { return app->do_verify; }
static inline bool CLAppLoggingEnabled(const CLApp *app) { return app->enable_logging; }

typedef struct {
  CLApp app;
  int num_iters;
} CLIterApp;

static inline void CLIterAppInit(CLIterApp *cli, int argc, char **argv, const char *name, int num_iters) {
  CLAppInit(&cli->app, argc, argv, name);
  strncat(cli->app.base.get_args, "i:", sizeof(cli->app.base.get_args) - strlen(cli->app.base.get_args) - 1);
  cli->num_iters = num_iters;
  char def_buf[16];
  snprintf(def_buf, sizeof(def_buf), "%d", num_iters);
  CLBaseAddHelpLine(&cli->app.base, 'i', "i", "perform i iterations", def_buf);
}

static inline int CLIterAppHandleOption(void *ctx, signed char opt, char *optarg) {
  CLIterApp *cli = (CLIterApp *)ctx;
  switch (opt) {
    case 'i':
      cli->num_iters = atoi(optarg);
      return 1;
    default:
      return CLAppHandleOption(&cli->app, opt, optarg);
  }
}

static inline bool CLIterAppParseArgs(CLIterApp *cli) {
  return CLBaseParseArgs(&cli->app.base, CLIterAppHandleOption, cli);
}

static inline int CLIterAppNumIters(const CLIterApp *cli) { return cli->num_iters; }

typedef struct {
  CLApp app;
  int max_iters;
  double tolerance;
} CLPageRank;

static inline void CLPageRankInit(CLPageRank *cli, int argc, char **argv, const char *name,
                                  double tolerance, int max_iters) {
  CLAppInit(&cli->app, argc, argv, name);
  strncat(cli->app.base.get_args, "i:t:", sizeof(cli->app.base.get_args) - strlen(cli->app.base.get_args) - 1);
  cli->max_iters = max_iters;
  cli->tolerance = tolerance;
  char buf_iters[16];
  snprintf(buf_iters, sizeof(buf_iters), "%d", max_iters);
  char buf_tol[32];
  snprintf(buf_tol, sizeof(buf_tol), "%g", tolerance);
  CLBaseAddHelpLine(&cli->app.base, 'i', "i", "perform at most i iterations", buf_iters);
  CLBaseAddHelpLine(&cli->app.base, 't', "t", "use tolerance t", buf_tol);
}

static inline int CLPageRankHandleOption(void *ctx, signed char opt, char *optarg) {
  CLPageRank *cli = (CLPageRank *)ctx;
  switch (opt) {
    case 'i':
      cli->max_iters = atoi(optarg);
      return 1;
    case 't':
      cli->tolerance = atof(optarg);
      return 1;
    default:
      return CLAppHandleOption(&cli->app, opt, optarg);
  }
}

static inline bool CLPageRankParseArgs(CLPageRank *cli) {
  return CLBaseParseArgs(&cli->app.base, CLPageRankHandleOption, cli);
}

static inline int CLPageRankMaxIters(const CLPageRank *cli) { return cli->max_iters; }
static inline double CLPageRankTolerance(const CLPageRank *cli) { return cli->tolerance; }

static inline int CLBaseScale(const CLBase *cl) { return cl->scale; }
static inline int CLBaseDegree(const CLBase *cl) { return cl->degree; }
static inline const char *CLBaseFilename(const CLBase *cl) { return cl->filename; }
static inline bool CLBaseSymmetrize(const CLBase *cl) { return cl->symmetrize; }
static inline bool CLBaseUniform(const CLBase *cl) { return cl->uniform_graph; }
static inline bool CLBaseInPlace(const CLBase *cl) { return cl->in_place; }

typedef struct {
  CLApp app;
  double delta;
} CLDelta;

static inline void CLDeltaInit(CLDelta *cli, int argc, char **argv, const char *name, double default_delta) {
  CLAppInit(&cli->app, argc, argv, name);
  strncat(cli->app.base.get_args, "d:", sizeof(cli->app.base.get_args) - strlen(cli->app.base.get_args) - 1);
  cli->delta = default_delta;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.4g", default_delta);
  CLBaseAddHelpLine(&cli->app.base, 'd', "d", "delta parameter", buf);
}

static inline int CLDeltaHandleOption(void *ctx, signed char opt, char *optarg) {
  CLDelta *cli = (CLDelta *)ctx;
  switch (opt) {
    case 'd':
      cli->delta = atof(optarg);
      return 1;
    default:
      return CLAppHandleOption(&cli->app, opt, optarg);
  }
}

static inline bool CLDeltaParseArgs(CLDelta *cli) {
  return CLBaseParseArgs(&cli->app.base, CLDeltaHandleOption, cli);
}

static inline double CLDeltaValue(const CLDelta *cli) {
  return cli->delta;
}

typedef struct {
  CLBase base;
  const char *out_filename;
  bool out_weighted;
  bool out_el;
  bool out_sg;
} CLConvert;

static inline void CLConvertInit(CLConvert *cli, int argc, char **argv, const char *name) {
  CLBaseInit(&cli->base, argc, argv, name);
  strncat(cli->base.get_args, "e:b:w", sizeof(cli->base.get_args) - strlen(cli->base.get_args) - 1);
  cli->out_filename = NULL;
  cli->out_weighted = false;
  cli->out_el = false;
  cli->out_sg = false;
  CLBaseAddHelpLine(&cli->base, 'b', "file", "output serialized graph to file", "");
  CLBaseAddHelpLine(&cli->base, 'e', "file", "output edge list to file", "");
  CLBaseAddHelpLine(&cli->base, 'w', "", "make output weighted", "false");
}

static inline int CLConvertHandleOption(void *ctx, signed char opt, char *optarg) {
  CLConvert *cli = (CLConvert *)ctx;
  switch (opt) {
    case 'b':
      cli->out_sg = true;
      cli->out_filename = optarg;
      return 1;
    case 'e':
      cli->out_el = true;
      cli->out_filename = optarg;
      return 1;
    case 'w':
      cli->out_weighted = true;
      return 1;
    default:
      return CLBaseHandleArg(&cli->base, opt, optarg);
  }
}

static inline bool CLConvertParseArgs(CLConvert *cli) {
  return CLBaseParseArgs(&cli->base, CLConvertHandleOption, cli);
}

static inline const char *CLConvertOutFilename(const CLConvert *cli) { return cli->out_filename; }
static inline bool CLConvertOutWeighted(const CLConvert *cli) { return cli->out_weighted; }
static inline bool CLConvertOutEL(const CLConvert *cli) { return cli->out_el; }
static inline bool CLConvertOutSG(const CLConvert *cli) { return cli->out_sg; }

#endif  // COMMAND_LINE_H_
