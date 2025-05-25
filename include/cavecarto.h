#ifndef CAVECARTO_H
#define CAVECARTO_H

#include <stdbool.h>
#include <time.h>

/**
 * Represents a 2D Cartesian point in meters.
 *
 * Used for local map coordinates after projection.
 */
typedef struct CCPoint {
  double x; ///< X coordinate in meters (East)
  double y; ///< Y coordinate in meters (North)
} CCPoint;

/**
 * Represents an absolute geographic location in decimal degrees.
 */
typedef struct CCLocation {
  double lon; ///< Longitude in decimal degrees
  double lat; ///< Latitude in decimal degrees
} CCLocation;

/**
 * Represents a distance and direction from another point.
 *
 * Azimuth is in degrees from geographic North.
 * Length is in meters.
 */
typedef struct CCDistance {
  double azimuth; ///< Azimuth in degrees from North
  double length;  ///< Distance in meters
} CCDistance;

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
typedef struct CCStation {
  double depth;              ///< Depth in meters (msw)
  bool fixed;                ///< Whether the station has an absolute location
  struct CCStation *parent;  ///< Parent station (if relative)
  struct CCStation *closure; ///< Optional closure target for loop closures
  union {
    CCLocation location;     ///< Absolute position (if fixed)
    CCDistance distance;     ///< Relative offset (if not fixed)
  };
  struct tm date;            ///< Date the station was recorded
  void *user_data;           ///< Optional user data
} CCStation;

/**
 * Flags controlling coordinate transformation behavior.
 */
typedef enum {
  CC_TRANSFORM_NONE        = 0,
  CC_TRANSFORM_CLOSE_LOOPS = (1 << 0), ///< Attempt to close station loops
  CC_TRANSFORM_MAG_NORTH   = (1 << 1), ///< Adjust for magnetic declination
} CCTransformFlags;

/**
 * Possible outcomes from station coordinate computations.
 */
typedef enum {
  CCStation_SUCCESS = 0,                  ///< Successful computation
  CCStation_NULL,                         ///< Input station array is NULL
  CCStation_NO_DATUM,                     ///< No valid datum could be determined
  CCStation_CYCLE_DETECTED,               ///< Circular dependency in station graph
  CCStation_RELATIVE_MISSING_PARENT,      ///< Relative station is missing a parent
} CCComputeResult;

/**
 * Resolves the 2D Cartesian coordinates (in meters) of each station relative to the datum.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin. Use NULL to auto-select the first fixed station.
 * @param out Output array of `CCPoint` (must have at least `count` elements)
 * @param flags Bitmask of `CCTransformFlags` (e.g., CC_TRANSFORM_CLOSE_LOOPS)
 * @return Result code indicating success or failure
 */
CCComputeResult cc_compute_station_xy(CCStation stations[],
  size_t count,
  CCLocation *datum,
  CCPoint out[],
  CCTransformFlags flags);

/**
 * Resolves the geographic coordinates (latitude/longitude) of each station.
 *
 * Internally uses local XY resolution, then projects results to geodetic space.
 *
 * @param stations Array of input stations
 * @param count Number of stations
 * @param datum Datum to use as origin and projection center
 * @param out Output array of `CCLocation` (must have at least `count` elements)
 * @param flags Bitmask of `CCTransformFlags` (e.g., CC_TRANSFORM_CLOSE_LOOPS, CC_TRANSFORM_MAG_NORTH)
 * @return Result code indicating success or failure
 */
CCComputeResult cc_compute_station_latlon(CCStation stations[],
  size_t count,
  CCLocation *datum,
  CCLocation out[],
  CCTransformFlags flags);

#endif
