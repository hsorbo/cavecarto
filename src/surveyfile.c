#include "surveyfile.h"
#include <stdlib.h>

static int cmp(const void *a, const void *b) {
  int int_a = *(const int *)a;
  int int_b = *(const int *)b;
  return (int_a > int_b) - (int_a < int_b);
}

size_t read_stations(sqlite3 *db, station *stations_in[]) {
  sqlite3_stmt *stmt;
  char query_count[] = "select count(*) from stations;";

  if (sqlite3_prepare_v2(db, query_count, -1, &stmt, NULL) != SQLITE_OK) {
    return -1; //Failed to count stations
  }

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -1;
  }
  
  size_t station_count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);

  if (stations_in == NULL) {
    return station_count;
  }

  char *query = "select \
        id,\
        parent,\
        linked,\
        fixed,\
        depth,\
        date,\
        latitude,\
        longitude,\
        distance,\
        azimuth \
        from stations ORDER by id;";
  
  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    return -1; //Failed to prepare
  }

  station* stations = malloc(sizeof(station) * station_count);
  size_t *ids = malloc(sizeof(size_t) * station_count);
  size_t *parents = malloc(sizeof(size_t) * station_count);
  size_t *linked = malloc(sizeof(size_t) * station_count);
  
  int i = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    station *station = &stations[i];
    ids[i] = sqlite3_column_int(stmt, 0);
    parents[i] = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 1);
    linked[i] = sqlite3_column_type(stmt, 2) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 2);
    station->fixed = sqlite3_column_int(stmt, 3);
    station->depth = sqlite3_column_double(stmt, 4);
    strptime((char *)sqlite3_column_text(stmt, 5), "%Y-%m-%d", &station->date);
    if (station->fixed) {
      station->location.lat = sqlite3_column_double(stmt, 6);
      station->location.lon = sqlite3_column_double(stmt, 7);
    }
    else {
      station->distance.length = sqlite3_column_double(stmt, 8);
      station->distance.azimuth = sqlite3_column_double(stmt, 9);
    }
    station->parent = NULL;
    station->closure = NULL;
    station->userData = (void *) ids[i];
    i++;
  }
  sqlite3_finalize(stmt);
  //assert(i == station_count);

  for (i = 0; i < station_count; i++) {
    station *station = &stations[i];
    size_t parent = parents[i];
    size_t link = linked[i];
    if (parent != -1) {
      // odds are parents are just a few before
      bool found = false;
      for (int j = i; j > 0 && i - j < 10 && !found; j--) {
        if (ids[j] == parent) {
          found = true;
          station->parent = &stations[j];
        }
      }
      if (!found) {
        size_t *result = (size_t *)bsearch(&parent, ids, station_count, sizeof(size_t), cmp);
        // assert(result != NULL);
        station->parent = &stations[result - ids];
      }
    }
    if (link != -1) {
      size_t *result = (size_t *)bsearch(&link, ids, station_count, sizeof(size_t), cmp);
      //assert(result != NULL);
      station->closure = &stations[result - ids];
    }
  }

  free(ids);
  free(parents);
  free(linked);
  *stations_in = stations;
  return station_count;
}
