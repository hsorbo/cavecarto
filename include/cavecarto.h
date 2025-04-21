#pragma once
#include <stdbool.h>
#include <time.h>

typedef struct cc_point {
  double x;
  double y;
} cc_point;

/*
 * Absolute geographic location in decimal degrees.
 */
typedef struct cc_location {
  double lon;
  double lat;
} cc_location;

/*
 * Contains an azimuth (in degrees from North) and a linear 
 * distance (in meters).
 */
typedef struct cc_distance {
  double azimuth;
  double length;
} cc_distance;

/*
 * Represents a survey station, either fixed or relative.
 *
 * A station can either have absolute coordinates (fixed) or a relative offset from
 * a parent station (non-fixed). Optionally, it can reference another station to
 * indicate a loop closure. Date is used when calculating magnetic north.
 *
 * - If `fixed` is true, `location` must be used and `parent` should be NULL.
 * - If `fixed` is false, `distance` and `parent` must be set.
 * - If `closure` is set, it must refer to a station with no further closure.
 */
typedef struct cc_station {
  double depth;
  bool fixed;
  struct cc_station *parent;
  struct cc_station *closure;
  union {
    cc_location location;
    cc_distance distance;
  };
  struct tm date;
  void *user_data;
} cc_station;

typedef enum {
  TRANSFORM_NONE         = 0,
  TRANSFORM_CLOSE_LOOPS  = (1 << 0),
  TRANSFORM_MAG_NORTH    = (1 << 1),
} cc_transform_flags_t;

typedef enum {
  STATION_SUCCESS = 0,
  STATION_NULL,
  STATION_NO_DATUM,
  STATION_CYCLE_DETECTED,
  STATION_RELATIVE_MISSING_PARENT,
} cc_station_result_t;

/**
 * Resolves the 2D Cartesian coordinates (in meters) of each station relative to the datum.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin use NULL to auto select first fixed station
 * @param out Output array of `point` (must be at least `count` long)
 * @param flags Bitmask of `coord_flags_t` (e.g., TRANSFORM_CLOSE_LOOPS)
 */
cc_station_result_t cc_compute_station_xy(cc_station stations[],
  size_t count,
  cc_location *datum,
  cc_point out[],
  cc_transform_flags_t flags);


/**
 * Resolves the geographic coordinates (latitude/longitude) of each station.
 *
 * Internally uses local XY resolution, then projects results to geodetic space.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin and projection center
 * @param out Output array of `location` (must be at least `count` long)
 * @param flags Bitmask of `coord_flags_t` (e.g., TRANSFORM_CLOSE_LOOPS, TRANSFORM_ADJUST_MAG_NORTH)
 */
cc_station_result_t cc_compute_station_latlon(cc_station stations[],
  size_t count,
  cc_location *datum,
  cc_location out[],
  cc_transform_flags_t flags);