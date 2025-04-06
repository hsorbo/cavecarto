#include "cavecarto.h"
#include "sqlite3.h"
#include "surveyfile.h"
#include "ubench.h"
#include "igrf.h"
#include <assert.h>


static int get_stations(station **stations) {
  char *file = "big.db";
  sqlite3 *db;
  int result = sqlite3_open(file, &db);
  assert(result == SQLITE_OK);
  int station_count = read_stations(db, stations);
  sqlite3_close(db);
  assert(station_count > 0);
  return station_count;
}

UBENCH_EX(get_coords, obh_close_loops) {
  station *stations = NULL;
  int station_count = get_stations(&stations);
  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  UBENCH_DO_BENCHMARK() {
    get_coords(stations, station_count, NULL, xs, ys, COORDS_CLOSE_LOOPS);
  }

  free(xs);
  free(ys);
  free(stations);
}

UBENCH_EX(get_coords, obh_mag_north) {
  station *stations = NULL;
  int station_count = get_stations(&stations);
  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  UBENCH_DO_BENCHMARK() {
    get_coords(stations, station_count, NULL, xs, ys, COORDS_ADJUST_MAG_NORTH);
  }

  free(xs);
  free(ys);
  free(stations);
}

UBENCH(igrf, declination) {
  declination(2018, 12, 8, 40., 40., 0.0);
}

UBENCH_MAIN();
