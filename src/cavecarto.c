#include "cavecarto.h"
#include "assert.h"
#include "csparse.h"
#include "matrix_builder.h"
#include <math.h>
#include <string.h>
#include "igrf.h"

const double EARTH_CIRCUMFERENCE = 4.007516E7;

typedef struct vec2 {
  double x;
  double y;
} vec2;

vec2 decompose_lengths(double length, double azimuth) {
  double d = (-azimuth) / 360.0 * 2.0 * M_PI;
  return (vec2){.x = length * sin(d), .y = length * cos(d)};
}

typedef struct vec3 {
  double x;
  double y;
  double z;
} vec3;

vec3 vec3_subtract(vec3 a, vec3 b) {
  return (vec3){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

double vec3_magnitude(vec3 v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double gps_distance_x(location a, location b) {
  return -((a.lon - b.lon) * EARTH_CIRCUMFERENCE *
           cos(((a.lat + b.lat) / 2.0) / 360.0 * 2.0 * M_PI) / 360.0);
}

double gps_distance_y(location a, location b) {
  return (a.lat - b.lat) * EARTH_CIRCUMFERENCE / 360.0;
}

double gps_distance_line(location a, location b) {
  double latdiff = gps_distance_y(a, b);
  double longdiff = (a.lon - b.lon) * EARTH_CIRCUMFERENCE / 360.0 *
                    cos((a.lat / 360.0 * 2.0 * M_PI));
  return sqrt(latdiff * latdiff + longdiff * longdiff);
}

location gps_add(location *src, double x, double y) {
  double lat = src->lat - y / EARTH_CIRCUMFERENCE * 360.0;
  double k = lat / 360.0 * 2.0 * M_PI;
  double lon = src->lon + x / EARTH_CIRCUMFERENCE * 360.0 / cos(k);
  return (location){.lat = lat, .lon = lon};
}

double calc_distance(double from_depth, double to_depth, double length) {
  double delta_depth = fabs(from_depth - to_depth);
  double hyp = length <= delta_depth ? length + 0.05 : length;
  double length_xy = sqrt(pow(hyp, 2) - pow(delta_depth, 2));
  assert(length_xy == length_xy); // Nan check
  return length_xy;
}

int hops_root(station *station) {
  int c = 0;
  while (station->parent != NULL) {
    station = station->parent;
    c++;
  }
  return c;
}

station *find_root(station *station) {
  while (true) {
    if (station->parent == NULL)
      return station;
    station = station->parent;
  }
}

station *find_via(station *a, station *b) {
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

void calc_a(station stations[], size_t stations_length, location *datum,
            double x[], double y[], double adj_azimuth[]) {
  memset(x, 0, stations_length * sizeof(double));
  memset(y, 0, stations_length * sizeof(double));

  station **walk = malloc(sizeof(station *) * 10000);
  for (int i = 0; i < stations_length; i++) {
    station *s = &stations[i];
    assert(s != NULL);
    assert(s->parent == NULL || !s->fixed);
    assert(s->parent != NULL || s->fixed);
    if (x[i] != 0. || y[i] != 0.)
      continue;

    size_t depth = 0;
    for (station *cur = s; cur != NULL; cur = cur->parent) {
      walk[depth++] = cur;
      if (cur->parent != NULL) {
        size_t idx = cur->parent - stations;
        assert(idx <= stations_length);
        assert(idx >= 0);
        if (x[idx] != 0. || y[idx] != 0.) {
          break;
        }
      }
    }

    for (size_t j = depth; j > 0; j--) {
      station *cwd = walk[j - 1];
      size_t idx = cwd - stations;
      if (cwd->fixed) {
        x[idx] = gps_distance_x(*datum, cwd->location);
        y[idx] = gps_distance_y(*datum, cwd->location);
      } else {
        size_t parent_idx = cwd->parent - stations;
        double len =
            calc_distance(cwd->parent->depth, cwd->depth, cwd->distance.length);
        double az_adj = adj_azimuth != NULL ? adj_azimuth[idx] : 0.0;
        vec2 dist = decompose_lengths(len, cwd->distance.azimuth + az_adj);
        x[idx] = x[parent_idx] - dist.x;
        y[idx] = y[parent_idx] - dist.y;
      }
    }
  }
  free(walk);
}

typedef struct start_adj {
  station *start_station;
  station *to_station;
  double weight;
} start_adj;

void close_leastsquare(station stations[], size_t length, double xs[],
                       double ys[]) {
  ssize_t *adjust_station_idx = malloc(sizeof(ssize_t) * length);
  size_t adj_station_count = 0;
  size_t fix_station_count = 0;
  for (int i = 0; i < length; i++) {
    station *current = &stations[i];
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
    station *current = &stations[i];
    if (current->fixed)
      continue;
    station *parent = current->parent;
    size_t current_idx = current - stations;
    size_t parent_idx = parent - stations;
    vec3 current_pos = {
        .x = xs[current_idx], .y = ys[current_idx], .z = current->depth};
    vec3 parent_pos = {
        .x = xs[parent_idx], .y = ys[parent_idx], .z = parent->depth};

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
    bx[idx] += xx.weight * xs[xx.start_station - stations];
    by[idx] += xx.weight * ys[xx.start_station - stations];
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
    station *current = &stations[i];
    ssize_t idx = adjust_station_idx[current - stations];
    if (idx != -1) {
      assert(bx[idx] == bx[idx]); // assert not NaN
      xs[i] = bx[idx];
      ys[i] = by[idx];
    }
  }
  for (int i = 0; i < length; i++) {
    station *s = &stations[i];
    if (s->closure != NULL) {
      xs[i] = xs[s->closure - stations];
      ys[i] = ys[s->closure - stations];
    }
  }
  cs_spfree(A);
  free(bx);
  free(by);
  mb_free(a);
  free(p);
  free(adjust_station_idx);
}

location *find_datum(station stations[], size_t length) {
  for (int i = 0; i < length; i++) {
    if (stations[i].fixed)
      return &stations[i].location;
  }
  return NULL;
}

void get_coords(station stations[], size_t length, location *datum, double xs[],
                double ys[], int flags) {
  datum = datum ? datum : find_datum(stations, length);
  assert(datum != NULL);
  calc_a(stations, length, datum, xs, ys, NULL);
  
  if (flags & COORDS_ADJUST_MAG_NORTH) {
    double * adj_az = malloc(sizeof(double) * length);
    for (int i = 0; i < length; i++) {
      station *s = &stations[i];
      if (!s->fixed) {
        location new = gps_add(datum, xs[i], ys[i]);
        adj_az[i] = declination(
          s->date.tm_year+1900, 
          s->date.tm_mon+1, 
          s->date.tm_mday,
          new.lat, 
          new.lon, 
          0.0);
      }
    }
    calc_a(stations, length, datum, xs, ys, adj_az);
    free(adj_az);
  }

  if (flags & COORDS_CLOSE_LOOPS) {
    close_leastsquare(stations, length, xs, ys);
  }

  if (flags & COORDS_LAT_LON) {
    for (int i = 0; i < length; i++) {
      station *s = &stations[i];
      if (s->fixed) {
        ys[i] = s->location.lat;
        xs[i] = s->location.lon;
      } else {
        location new = gps_add(datum, xs[i], ys[i]);
        ys[i] = new.lat;
        xs[i] = new.lon;
      }
    }
  }
}
