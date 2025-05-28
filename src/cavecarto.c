#include "cavecarto.h"
#include "assert.h"
#include "csparse.h"
#include "matrix_builder.h"
#include <math.h>
#include <string.h>
#include "igrf.h"

static const size_t MAX_LINE_HOP_COUNT = 10000;
static const double EARTH_CIRCUMFERENCE = 4.007516E7;

static CCPoint decompose_lengths(double length, double azimuth) {
  double d = (-azimuth) / 360.0 * 2.0 * M_PI;
  return (CCPoint){.x = length * sin(d), .y = length * cos(d)};
}

typedef struct vec3 {
  double x;
  double y;
  double z;
} vec3;

static vec3 vec3_subtract(vec3 a, vec3 b) {
  return (vec3){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

static double vec3_magnitude(vec3 v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static double gps_distance_x(CCLocation a, CCLocation b) {
  return -((a.lon - b.lon) * EARTH_CIRCUMFERENCE *
           cos(((a.lat + b.lat) / 2.0) / 360.0 * 2.0 * M_PI) / 360.0);
}

static double gps_distance_y(CCLocation a, CCLocation b) {
  return (a.lat - b.lat) * EARTH_CIRCUMFERENCE / 360.0;
}

static CCLocation gps_add(CCLocation *src, double x, double y) {
  double lat = src->lat - y / EARTH_CIRCUMFERENCE * 360.0;
  double k = lat / 360.0 * 2.0 * M_PI;
  double lon = src->lon + x / EARTH_CIRCUMFERENCE * 360.0 / cos(k);
  return (CCLocation){.lat = lat, .lon = lon};
}

static double calc_distance(double from_depth, double to_depth, double length) {
  double delta_depth = fabs(from_depth - to_depth);
  double hyp = length <= delta_depth ? length + 0.05 : length;
  double length_xy = sqrt(pow(hyp, 2) - pow(delta_depth, 2));
  assert(length_xy == length_xy); // Nan check
  return length_xy;
}

/*
static double gps_distance_line(CCLocation a, CCLocation b) {
  double latdiff = gps_distance_y(a, b);
  double longdiff = (a.lon - b.lon) * EARTH_CIRCUMFERENCE / 360.0 *
                    cos((a.lat / 360.0 * 2.0 * M_PI));
  return sqrt(latdiff * latdiff + longdiff * longdiff);
}

static int hops_root(CCStation *station) {
  int c = 0;
  while (station->parent != NULL) {
    station = station->parent;
    c++;
  }
  return c;
}

static CCStation *find_root(CCStation *station) {
  while (true) {
    if (station->parent == NULL)
      return station;
    station = station->parent;
  }
}

static CCStation *find_via(CCStation *a, CCStation *b) {
  if (a == b)
    return a;
  int ad = hops_root(a);
  int bd = hops_root(a);
  while (ad + bd > 0) {
    if (ad >= bd) {
      a = a->parent;
      ad--;
    }
    if (ad <= bd) {
      b = b->parent;
      bd--;
    }
    if (a == b)
      return a;
  }
  return NULL;
}
*/

static CCComputeResult calc_a(CCStation stations[], 
                              size_t stations_length, 
                              CCLocation *datum,
                              CCPoint out[], 
                              double adj_azimuth[]) {
  CCComputeResult result = CCStation_SUCCESS;
  memset(out, 0, stations_length * sizeof(CCPoint));

  CCStation **walk = malloc(sizeof(CCStation *) * MAX_LINE_HOP_COUNT);
  for (int i = 0; i < stations_length; i++) {
    CCStation *s = &stations[i];
    if (s == NULL) {
      result = CCStation_NULL;
      goto beach;
    }
    assert(s->parent == NULL || !s->fixed);
    if(!s->fixed && s->parent == NULL) {
      result = CCStation_RELATIVE_MISSING_PARENT;
      goto beach;
    }
    if (out[i].x != 0. || out[i].y != 0.)
      continue;

    size_t depth = 0;
    for (CCStation *cur = s; cur != NULL; cur = cur->parent) {
      if (depth >= MAX_LINE_HOP_COUNT) {
        result = CCStation_CYCLE_DETECTED;
        goto beach;
      }
      walk[depth++] = cur;
      if (cur->parent != NULL) {
        size_t idx = cur->parent - stations;
        assert(idx <= stations_length);
        assert(idx >= 0);
        if (out[idx].x != 0. || out[idx].y != 0.) {
          break;
        }
      }
    }

    for (size_t j = depth; j > 0; j--) {
      CCStation *cwd = walk[j - 1];
      size_t idx = cwd - stations;
      if (cwd->fixed) {
        out[idx].x = gps_distance_x(*datum, cwd->location);
        out[idx].y = gps_distance_y(*datum, cwd->location);
      } else {
        size_t parent_idx = cwd->parent - stations;
        double len =
            calc_distance(cwd->parent->depth, cwd->depth, cwd->distance.length);
        double az_adj = adj_azimuth != NULL ? adj_azimuth[idx] : 0.0;
        CCPoint dist = decompose_lengths(len, cwd->distance.azimuth + az_adj);
        out[idx].x = out[parent_idx].x - dist.x;
        out[idx].y = out[parent_idx].y - dist.y;
      }
    }
  }
  beach:
  free(walk);
  return result;
}

typedef struct start_adj {
  CCStation *start_station;
  CCStation *to_station;
  double weight;
} start_adj;

static void close_leastsquare(CCStation stations[], size_t length, CCPoint out[]) {
  ssize_t *adjust_station_idx = malloc(sizeof(ssize_t) * length);
  size_t adj_station_count = 0;
  size_t fix_station_count = 0;
  for (int i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    bool adjustable = !current->fixed && current->closure == NULL;
    // this estimation is shady, ... do better
    fix_station_count += current->fixed ? 1 : 0;
    fix_station_count += current->parent != NULL && current->parent->fixed ? 1 : 0;
    adjust_station_idx[i] = adjustable ? adj_station_count++ : -1;
  }
  double *bx = calloc(adj_station_count, sizeof(double));
  double *by = calloc(adj_station_count, sizeof(double));

  matrix_builder *a = mb_init(adj_station_count);

  start_adj *p = calloc(fix_station_count, sizeof(start_adj));
  int plen = 0;

  for (int i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    if (current->fixed) {
      continue;
    }
    CCStation *parent = current->parent;
    size_t current_idx = current - stations;
    size_t parent_idx = parent - stations;
    vec3 current_pos = {
        .x = out[current_idx].x, .y = out[current_idx].y, .z = current->depth};
    vec3 parent_pos = {
        .x = out[parent_idx].x, .y = out[parent_idx].y, .z = parent->depth};

    vec3 distance = vec3_subtract(current_pos, parent_pos);
    double wdistance = vec3_magnitude(distance);
    double weight = 1.0 / (wdistance < 0.1 ? 0.1 : wdistance);

    current = current->closure != NULL ? current->closure : current;
    parent = parent->closure != NULL ? parent->closure : parent;

    ssize_t to_index = adjust_station_idx[current - stations];
    ssize_t from_index = adjust_station_idx[parent - stations];

    assert((to_index == -1 && current->fixed) || (to_index >= 0 && !current->fixed));
    assert((from_index == -1 && parent->fixed) || (from_index >= 0 && !parent->fixed));

    if (!current->fixed) {
      bx[to_index] += weight * distance.x;
      by[to_index] += weight * distance.y;
      mb_update(a, to_index, to_index, weight);
    }

    if (!parent->fixed) {
      bx[from_index] += -weight * distance.x;
      by[from_index] += -weight * distance.y;
      mb_update(a, from_index, from_index, weight);
    }

    if (!parent->fixed && !current->fixed) {
      mb_update(a, to_index, from_index, -weight);
      mb_update(a, from_index, to_index, -weight);
    }

    if (current->fixed && !parent->fixed) {
      p[plen++] = (start_adj){.start_station = current, .to_station = parent, .weight = weight};
    } else if (parent->fixed && !current->fixed) {
      p[plen++] = (start_adj){.start_station = parent, .to_station = current, .weight = weight};
    }
    assert(plen <= (fix_station_count + 1));
  }

  for (int i = 0; i < plen; i++) {
    start_adj xx = p[i];
    ssize_t idx = adjust_station_idx[xx.to_station - stations];
    assert(idx != -1); // used to be if
    bx[idx] += xx.weight * out[xx.start_station - stations].x;
    by[idx] += xx.weight * out[xx.start_station - stations].y;
  }
  cs *A = mb_csc(a);
  // cs_print(A, 0);
  // dump_csc(A);
  // https://people.math.sc.edu/Burkardt/c_src/csparse/csparse.html
  // cs_qrsol
  // cs_lusol
  // cs_chol
  /*order
    case -1: printf ("natural    ") ; break ;
    case  0: printf ("amd(A+A')  ") ; break ;
    case  1: printf ("amd(S'*S)  ") ; break ;
    case  2: printf ("amd(A'*A)  ") ; break ;
  */
  bool solved = cs_cholsol(A, bx, 0);
  assert(solved == true);
  solved = cs_cholsol(A, by, 0);
  assert(solved == true);

  for (int i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    ssize_t idx = adjust_station_idx[current - stations];
    if (idx != -1) {
      assert(bx[idx] == bx[idx]); // assert not NaN
      out[i].x = bx[idx];
      out[i].y = by[idx];
    }
  }
  for (int i = 0; i < length; i++) {
    CCStation *s = &stations[i];
    if (s->closure != NULL) {
      out[i].x = out[s->closure - stations].x;
      out[i].y = out[s->closure - stations].y;
    }
  }
  cs_spfree(A);
  free(bx);
  free(by);
  mb_free(a);
  free(p);
  free(adjust_station_idx);
}

CCLocation *find_datum(CCStation stations[], size_t length) {
  for (int i = 0; i < length; i++) {
    if (stations[i].fixed)
      return &stations[i].location;
  }
  return NULL;
}

static CCComputeResult get_coords(CCStation stations[], 
                                   size_t length, 
                                   CCLocation *datum, 
                                   CCPoint out[],
                                   CCTransformFlags flags,
                                   bool latlon) {
  CCComputeResult result = CCStation_SUCCESS;
  datum = datum ? datum : find_datum(stations, length);
  if (datum == NULL) {
    return CCStation_NO_DATUM;
  }
  result = calc_a(stations, length, datum, out, NULL);
  if (result != CCStation_SUCCESS) return result;
  
  if (flags & CC_TRANSFORM_MAG_NORTH) {
    double * adj_az = malloc(sizeof(double) * length);
    for (int i = 0; i < length; i++) {
      CCStation *s = &stations[i];
      if (!s->fixed) {
        CCLocation new = gps_add(datum, out[i].x, out[i].y);
        adj_az[i] = declination(
          s->date.tm_year+1900, 
          s->date.tm_mon+1, 
          s->date.tm_mday,
          new.lat, 
          new.lon, 
          0.0);
      }
    }
    result = calc_a(stations, length, datum, out, adj_az);
    free(adj_az);
    if (result != CCStation_SUCCESS) return result;
  }

  if (flags & CC_TRANSFORM_CLOSE_LOOPS) {
    close_leastsquare(stations, length, out);
  }

  if (latlon) {
    for (int i = 0; i < length; i++) {
      CCStation *s = &stations[i];
      if (s->fixed) {
        out[i].y = s->location.lat;
        out[i].x = s->location.lon;
      } else {
        CCLocation new = gps_add(datum, out[i].x, out[i].y);
        out[i].y = new.lat;
        out[i].x = new.lon;
      }
    }
  }
  return result;
}


CCComputeResult cc_compute_station_xy(CCStation stations[],
  size_t count,
  CCLocation *datum,
  CCPoint out[],
  CCTransformFlags flags) {
    return get_coords(stations, count, datum, out, flags, false);
  }

CCComputeResult cc_compute_station_latlon(CCStation stations[],
  size_t count,
  CCLocation *datum,
  CCLocation out[],
  CCTransformFlags flags) {
    return get_coords(stations, count, datum, (CCPoint*)out, flags, true);
  }