#include "tmlu.h"
#include <stdio.h>

static void parser(survey_data *data, void *userData) {
  // comment: id,""
  // explorer: id, "name"
  // ui: id, locked, color,
  char date[20];
  if (data->type == Closure) {
    printf("closure: %d,%d,%d\n", data->id, data->parent, data->close_to_id);
  }
  if (data->type == Start) {
    strftime(date, 20, "%Y-%m-%d", &data->date);
    printf("fixed: %d,%f,%f,%f,%s\n", data->id, data->lat, data->lon,
           data->depth, date);
  }
  if (data->type == Real) {
    strftime(date, 20, "%Y-%m-%d", &data->date);
    printf("relative: %d,%d,%f,%f,%f,%s\n", data->id, data->parent,
           data->azimuth, data->len, data->depth, date);
  }
  if (strcmp(data->comment, "") != 0 && data->type != Closure) {
    printf("comment: %d,\"%s\"\n", data->id, data->comment);
  }
}

int main(const int argc, const char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <tmlu-file>\n", argv[0]);
    return 1;
  }
  FILE *fp = fopen(argv[1], "r");
  if (!fp) {
    perror("fopen");
    return 1;
  }
  int count = parse(fp, parser, NULL);
  fprintf(stderr, "Processed %d stations\n", count);
  fclose(fp);
  return 0;
}