#include <stdio.h>

#include "base.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "writer.h"

int main(int argc, char **argv) {
  CLConvert cli;
  CLConvertInit(&cli, argc, argv, "converter");
  if (!CLConvertParseArgs(&cli))
    return -1;
  if (CLConvertOutFilename(&cli) == NULL) {
    printf("No output filename specified\n");
    return -1;
  }
  if (CLConvertOutWeighted(&cli)) {
    WeightedBuilder builder;
    WeightedBuilderInit(&builder, &cli.base);
    WGraph wg;
    WeightedBuilderMakeGraph(&builder, &wg);
    WGraphPrintStats(&wg);
    WeightedWriter writer;
    WeightedWriterInit(&writer, &wg);
    WeightedWriterWriteGraph(&writer, CLConvertOutFilename(&cli),
                             CLConvertOutSG(&cli));
    WGraphFree(&wg);
  } else {
    Builder builder;
    BuilderInit(&builder, &cli.base);
    Graph g;
    BuilderMakeGraph(&builder, &g);
    GraphPrintStats(&g);
    Writer writer;
    WriterInit(&writer, &g);
    WriterWriteGraph(&writer, CLConvertOutFilename(&cli),
                     CLConvertOutSG(&cli));
    GraphFree(&g);
  }
  return 0;
}
