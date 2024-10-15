#pragma once
#include "stdbool.h"
#include "time.h"

typedef struct location {
  double lat;
  double lon;
} location;

typedef struct distance {
  double azimuth;
  double length;
} distance;

typedef struct station {
  double depth;
  bool fixed;
  // Must be set if not fixed, else optional
  struct station *parent;
  // Must be NULL if fixed, else optional.
  // It's not possible to close to an already closed station.
  // (station->closure->closure MUST be NULL)
  struct station *closure;
  // If fixed use location, else distance
  union {
    location location;
    distance distance;
  };
  struct tm date;
  void *userData;
} station;

typedef struct cave {
  station *stations;
  size_t stations_count;
} cave;

void get_coords(station stations[], size_t length, location *datum, double xs[],
                double ys[], bool close_loops);
void get_locations(station stations[], size_t length, location *datum,
                   double lon[], double lat[], bool close_loops);
