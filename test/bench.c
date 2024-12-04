#include "cavecarto.h"
#include "sqlite3.h"
#include "surveyfile.h"
#include "ubench.h"

UBENCH_EX(close, obh) {
  char *file = "/Users/hsorbo/Projects/cave/testdata-private/OBH_Complete.db";

  sqlite3 *db;
  if (sqlite3_open(file, &db) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  station *stations = NULL;
  int station_count = read_stations(db, &stations);
  sqlite3_close(db);

  if (station_count < 0) {
    return;
  }

  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  UBENCH_DO_BENCHMARK() {
    get_coords(stations, station_count, NULL, xs, ys, true);
  }

  free(xs);
  free(ys);
  free(stations);
}

UBENCH_MAIN();
