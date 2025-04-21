#include "cavecarto.h"
#include "ubench.h"
#include "igrf.h"
#include <assert.h>
#include "bench.h"


UBENCH_EX(get_coords, obh_close_loops) {
  vec2 *pos = malloc(sizeof(vec2) * station_count);

  UBENCH_DO_BENCHMARK() {
    compute_station_xy(stations, station_count, NULL, pos, TRANSFORM_CLOSE_LOOPS);
  }

  free(pos);
}

UBENCH_EX(get_coords, obh_mag_north) {
  vec2 *pos = malloc(sizeof(vec2) * station_count);

  UBENCH_DO_BENCHMARK() {
    compute_station_xy(stations, station_count, NULL, pos, TRANSFORM_MAG_NORTH);
  }

  free(pos);
}

UBENCH(igrf, declination) {
  declination(2018, 12, 8, 40., 40., 0.0);
}

UBENCH_MAIN();
