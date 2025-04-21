#pragma once
#include <stdbool.h>
#include <time.h>

typedef struct survey_point {
  double x;
  double y;
} survey_point;

/*
 * Absolute geographic location in decimal degrees.
 */
typedef struct survey_location {
  double lon;
  double lat;
} survey_location;

/*
 * Contains an azimuth (in degrees from North) and a linear 
 * distance (in meters).
 */
typedef struct survey_distance {
  double azimuth;
  double length;
} survey_distance;

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
typedef struct survey_station {
  double depth;
  bool fixed;
  struct survey_station *parent;
  struct survey_station *closure;
  union {
    survey_location location;
    survey_distance distance;
  };
  struct tm date;
  void *user_data;
} survey_station;

typedef enum {
  TRANSFORM_NONE         = 0,
  TRANSFORM_CLOSE_LOOPS  = (1 << 0),
  TRANSFORM_MAG_NORTH    = (1 << 1),
} transform_flags_t;

typedef enum {
  STATION_SUCCESS = 0,
  STATION_NULL,
  STATION_NO_DATUM,
  STATION_CYCLE_DETECTED,
  STATION_RELATIVE_MISSING_PARENT,
} station_result_t;

/**
 * Resolves the 2D Cartesian coordinates (in meters) of each station relative to the datum.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin use NULL to auto select first fixed station
 * @param out Output array of `point` (must be at least `count` long)
 * @param flags Bitmask of `coord_flags_t` (e.g., TRANSFORM_CLOSE_LOOPS)
 */
station_result_t compute_station_xy(survey_station stations[],
  size_t count,
  survey_location *datum,
  survey_point out[],
  transform_flags_t flags);


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
station_result_t compute_station_latlon(survey_station stations[],
  size_t count,
  survey_location *datum,
  survey_location out[],
  transform_flags_t flags);