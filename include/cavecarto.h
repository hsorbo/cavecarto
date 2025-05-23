#ifndef CAVECARTO_H
#define CAVECARTO_H

#include <stdbool.h>
#include <time.h>

/**
 * Represents a 2D Cartesian point in meters.
 *
 * Used for local map coordinates after projection.
 */
typedef struct cc_point {
  double x; ///< X coordinate in meters (East)
  double y; ///< Y coordinate in meters (North)
} cc_point;

/**
 * Represents an absolute geographic location in decimal degrees.
 */
typedef struct cc_location {
  double lon; ///< Longitude in decimal degrees
  double lat; ///< Latitude in decimal degrees
} cc_location;

/**
 * Represents a distance and direction from another point.
 *
 * Azimuth is in degrees from geographic North.
 * Length is in meters.
 */
typedef struct cc_distance {
  double azimuth; ///< Azimuth in degrees from North
  double length;  ///< Distance in meters
} cc_distance;

/**
 * Represents a survey station, either fixed or relative.
 *
 * A station can either have absolute coordinates (fixed) or a relative offset from
 * a parent station (non-fixed). Optionally, it can reference another station to
 * indicate a loop closure. Depth is in meters (msw). The date is used when calculating
 * magnetic north.
 *
 * - If `fixed` is true, `location` must be used and `parent` must be NULL.
 * - If `fixed` is false, `distance` and `parent` must be set.
 * - If `closure` is set, it must refer to a station with no further closure.
 */
typedef struct cc_station {
  double depth;              ///< Depth in meters (msw)
  bool fixed;                ///< Whether the station has an absolute location
  struct cc_station *parent;  ///< Parent station (if relative)
  struct cc_station *closure; ///< Optional closure target for loop closures
  union {
    cc_location location;    ///< Absolute position (if fixed)
    cc_distance distance;    ///< Relative offset (if not fixed)
  };
  struct tm date;            ///< Date the station was recorded
  void *user_data;           ///< Optional user data
} cc_station;

/**
 * Flags controlling coordinate transformation behavior.
 */
typedef enum {
  CC_TRANSFORM_NONE        = 0,
  CC_TRANSFORM_CLOSE_LOOPS = (1 << 0), ///< Attempt to close station loops
  CC_TRANSFORM_MAG_NORTH   = (1 << 1), ///< Adjust for magnetic declination
} cc_transform_flags_t;

/**
 * Possible outcomes from station coordinate computations.
 */
typedef enum {
  CC_STATION_SUCCESS = 0,                  ///< Successful computation
  CC_STATION_NULL,                         ///< Input station array is NULL
  CC_STATION_NO_DATUM,                     ///< No valid datum could be determined
  CC_STATION_CYCLE_DETECTED,              ///< Circular dependency in station graph
  CC_STATION_RELATIVE_MISSING_PARENT,     ///< Relative station is missing a parent
} cc_station_result_t;

/**
 * Resolves the 2D Cartesian coordinates (in meters) of each station relative to the datum.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin. Use NULL to auto-select the first fixed station.
 * @param out Output array of `cc_point` (must have at least `count` elements)
 * @param flags Bitmask of `cc_transform_flags_t` (e.g., CC_TRANSFORM_CLOSE_LOOPS)
 * @return Result code indicating success or failure
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
 * @param out Output array of `cc_location` (must have at least `count` elements)
 * @param flags Bitmask of `cc_transform_flags_t` (e.g., CC_TRANSFORM_CLOSE_LOOPS, CC_TRANSFORM_MAG_NORTH)
 * @return Result code indicating success or failure
 */
cc_station_result_t cc_compute_station_latlon(cc_station stations[],
  size_t count,
  cc_location *datum,
  cc_location out[],
  cc_transform_flags_t flags);

#endif
