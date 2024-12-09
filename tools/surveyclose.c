#include "cavecarto.h"
#include "sqlite3.h"
#include "surveyfile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    printf("Usage: %s file\n", argv[0]);
    return 1;
  }

  sqlite3 *db;
  int result = sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE, NULL);
  if (result != SQLITE_OK) {
    printf("Error: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  station *stations = NULL;
  int station_count = read_stations(db, &stations);
  if (station_count < 0) {
    printf("Failed to read stations\n");
    sqlite3_close(db);
    return 1;
  }

  double *xs = malloc(sizeof(double) * station_count);
  double *ys = malloc(sizeof(double) * station_count);

  get_coords(stations, station_count, NULL, xs, ys, COORDS_LAT_LON | COORDS_CLOSE_LOOPS | COORDS_ADJUST_MAG_NORTH);


  char *error;
  const char create_table[] =
      "CREATE TABLE IF NOT EXISTS tmp_spos (station_id UNIQUE, x REAL, y REAL);";
  char insert[] = "INSERT INTO tmp_spos (station_id, x, y) VALUES (?, ?, ?);";
    
  sqlite3_exec(db,create_table, NULL, NULL, &error);
  if (error != NULL) goto fail;
  
  sqlite3_exec(db,"BEGIN TRANSACTION", NULL, NULL, &error);
  if (error != NULL) goto fail;

  for (int i = 0; i < station_count; i++) {
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, insert, -1, &stmt, NULL) != SQLITE_OK) {
      printf("Failed to prepare insert\n");
      goto fail;
    }
    sqlite3_bind_int(stmt, 1, (intptr_t)stations[i].userData);
    sqlite3_bind_double(stmt, 2, xs[i]);
    sqlite3_bind_double(stmt, 3, ys[i]);

    if (sqlite3_step(stmt) != SQLITE_DONE) goto fail;
    sqlite3_finalize(stmt);
  }
  sqlite3_exec(db,"COMMIT", NULL, NULL, &error);
  if (error != NULL) goto fail;

  sqlite3_close(db);
  free(xs);
  free(ys);
  free(stations);

  return 0;
  fail:
    printf("Error: %s\n", error);
    sqlite3_free(error);
    sqlite3_close(db);
    free(xs);
    free(ys);
    free(stations);
    return 1;
}