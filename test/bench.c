#include "cavecarto.h"
#include "ubench.h"
#include "igrf.h"
#include <assert.h>
#include "bench.h"


UBENCH_EX(get_coords, compute_none) {
  CCPoint *pos = malloc(sizeof(CCPoint) * station_count);

  UBENCH_DO_BENCHMARK() {
    cc_compute_station_xy(stations, station_count, NULL, pos, CC_TRANSFORM_NONE);
  }

  free(pos);
}

UBENCH_EX(get_coords, compute_close_loops) {
  CCPoint *pos = malloc(sizeof(CCPoint) * station_count);

  UBENCH_DO_BENCHMARK() {
    cc_compute_station_xy(stations, station_count, NULL, pos, CC_TRANSFORM_CLOSE_LOOPS);
  }

  free(pos);
}

UBENCH_EX(get_coords, compute_mag_north) {
  CCPoint *pos = malloc(sizeof(CCPoint) * station_count);

  UBENCH_DO_BENCHMARK() {
    cc_compute_station_xy(stations, station_count, NULL, pos, CC_TRANSFORM_MAG_NORTH);
  }

  free(pos);
}

UBENCH_EX(get_coords, compute_mag_north_fast) {
  CCPoint *pos = malloc(sizeof(CCPoint) * station_count);

  UBENCH_DO_BENCHMARK() {
    cc_compute_station_xy(stations, station_count, NULL, pos, CC_TRANSFORM_MAG_NORTH_FAST);
  }

  free(pos);
}

UBENCH(igrf, declination) {
  declination(2018, 12, 8, 40., 40., 0.0);
}

UBENCH_MAIN();
