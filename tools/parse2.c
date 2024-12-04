#include "cavecarto.h"
#include "tmlu.h"
#include <malloc/malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

void calc(cave *cave) {
  double *x = malloc(sizeof(double) * cave->stations_count);
  double *y = malloc(sizeof(double) * cave->stations_count);
  struct timeval stop, start;
  gettimeofday(&start, NULL);
  get_coords(cave->stations, cave->stations_count, NULL, x, y, true);
  gettimeofday(&stop, NULL);
  long took =
      (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
  printf("took %lu us\n", took);
  // for (int i = 0; i < cave->stations_count; i++) {
  //   station * s = &cave->stations[i];
  //   //printf("%d (%f,%f) -> (%f,%f)\n", i, old_x[i], old_y[i], x[i], y[i]);
  //   //printf("%d (%f,%f)\n", ((tmlu_meta*)s->userData)->id , x[i], y[i]);
  //   printf("%d (%.4f,%.4f)\n", ((tmlu_meta*)s->userData)->id , x[i], y[i]);
  // }
}

void dumpclosures(cave *cave) {
  for (int i = 0; i < cave->stations_count; i++) {
    station *station = &cave->stations[i];
    if (station->closure != NULL) {
      printf("%d -> %d\n", ((tmlu_meta *)station->userData)->id,
             ((tmlu_meta *)station->closure->userData)->id);
    }
  }
}

int main(const int argc, const char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <tmlu-file>\n", argv[0]);
    return 1;
  }

  cave *cave = open_file(argv[1]);
  if (cave == NULL) {
    printf("Failed to open %s\n", argv[1]);
    return 1;
  }
  calc(cave);

  // fprintf(stderr, "%zu stations\n", cave->stations_count);

  fflush(stdout);
  return 0;
}