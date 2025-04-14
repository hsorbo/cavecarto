#include "cavecarto.h"
#include "surveyfile.h"
#include "ubench.h"
#include "igrf.h"
#include <assert.h>
#include "bench.h"


UBENCH_EX(get_coords, obh_close_loops) {
  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  UBENCH_DO_BENCHMARK() {
    get_coords(stations, station_count, NULL, xs, ys, COORDS_CLOSE_LOOPS);
  }

  free(xs);
  free(ys);
}

UBENCH_EX(get_coords, obh_mag_north) {
  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  UBENCH_DO_BENCHMARK() {
    get_coords(stations, station_count, NULL, xs, ys, COORDS_ADJUST_MAG_NORTH);
  }

  free(xs);
  free(ys);
}

UBENCH(igrf, declination) {
  declination(2018, 12, 8, 40., 40., 0.0);
}

UBENCH_MAIN();
