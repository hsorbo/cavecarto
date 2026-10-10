#include "cavecarto.h"
#include "assert.h"
#include "csparse.h"
#include "dok_builder.h"
#include <math.h>
#include <string.h>
#include "igrf.h"
#ifdef USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif


#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288
#endif

static const size_t MAX_LINE_HOP_COUNT = 10000;
static const double EARTH_CIRCUMFERENCE = 4.007516E7;

void cc_station_init_fixed(
    CCStation* s,
    ssize_t parent,
    double lat,
    double lon,
    double depth) {
    if (!s) return;
    s->parent = parent;
    s->depth = depth;
    s->fixed = true;
    s->closure = -1;
    s->location = (CCLocation) {.lat = lat, .lon = lon};
}

void cc_station_init_relative(
    CCStation* s,
    ssize_t parent,
    ssize_t closure,
    double azimuth,
    double length,
    double depth,
    CCDate date) {
    if (!s) return;
    s->parent = parent;
    s->depth = depth;
    s->fixed = false;
    s->closure = closure;
    s->distance = (CCDistance){.azimuth = azimuth, .length = length};
    s->date = date;
}

CCDate cc_make_date(uint16_t year, uint8_t month, uint8_t day) {
  CCDate date = {
    .year = year,
    .month = month,
    .day = day,
  };
  return date;
}

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

static CCComputeResult build_xy_map(CCStation stations[],
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
    assert(s->parent == -1 || !s->fixed);
    if(!s->fixed && s->parent == -1) {
      result = CCStation_RELATIVE_MISSING_PARENT;
      goto beach;
    }
    if (out[i].x != 0. || out[i].y != 0.)
      continue;

    size_t depth = 0;
    for (ssize_t cur_i = (s - stations); cur_i != -1; cur_i = stations[cur_i].parent) {
      if (depth >= MAX_LINE_HOP_COUNT) {
        result = CCStation_CYCLE_DETECTED;
        goto beach;
      }
      walk[depth++] = &stations[cur_i];
      if (stations[cur_i].parent != -1) {
        size_t idx = (size_t)stations[cur_i].parent;
        assert(idx <= stations_length);
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
        size_t parent_idx = (size_t)cwd->parent;
        double len = calc_distance(stations[parent_idx].depth, cwd->depth, cwd->distance.length);
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

#ifdef USE_ACCELERATE
static void solve(DokBuilder *builder, double *b1, double *b2) {
  cs *A = dok_builder_to_csc(builder);
  const int n = (int)A->n;

  /* Accelerate requires columnStarts as long*. Copy A->p (int*) -> long*. */
  long *colStarts = (long *)malloc((size_t)(n + 1) * sizeof(long));
  for (int i = 0; i <= n; ++i) colStarts[i] = (long)A->p[i];

  /* Attributes: symmetric, lower triangle, not transposed */
  SparseAttributes_t attrs;
  attrs.transpose = false;
  attrs.triangle  = SparseLowerTriangle;
  attrs.kind      = SparseSymmetric;

  SparseMatrixStructure structure = (SparseMatrixStructure){
    .rowCount     = n,
    .columnCount  = n,
    .columnStarts = colStarts,         /* long* */
    .rowIndices   = (int *)A->i,       /* int*  */
    .attributes   = attrs,
    .blockSize    = 1
  };

  SparseMatrix_Double As = (SparseMatrix_Double){
    .structure = structure,
    .data      = A->x                  /* double* values */
  };

  /* RHS as dense n×2, column-major */
  double *Bdata = (double *)malloc((size_t)n * 2 * sizeof(double));
  memcpy(Bdata,             b1, (size_t)n * sizeof(double));
  memcpy(Bdata + (size_t)n, b2, (size_t)n * sizeof(double));

  DenseMatrix_Double B = (DenseMatrix_Double){
    .rowCount     = n,
    .columnCount  = 2,
    .columnStride = n,
    .attributes   = (SparseAttributes_t){0},  /* ordinary dense */
    .data         = Bdata
  };

  /* Factor (Cholesky) and solve */
  SparseOpaqueFactorization_Double F =
      SparseFactor(SparseFactorizationCholesky, As);
  /* If A might be only PSD or not strictly SPD, try:
     SparseFactor(SparseFactorizationCholeskyAtA, As); */

  SparseSolve(F, B);  /* overwrites B.data with solutions */

  /* Copy back, cleanup */
  memcpy(b1, Bdata,             (size_t)n * sizeof(double));
  memcpy(b2, Bdata + (size_t)n, (size_t)n * sizeof(double));

  SparseCleanup(F);
  free(Bdata);
  free(colStarts);
  cs_spfree(A);
}
#else
static void solve(DokBuilder *a, double *b1, double *b2) {
  // https://people.math.sc.edu/Burkardt/c_src/csparse/csparse.html
  int order = 0;
  cs *A = dok_builder_to_csc(a);

  css *S = cs_schol(A, order);
  assert(S && "cs_schol failed");

  csn *N = cs_chol(A, S);
  assert(N && "cs_chol failed");

  int n = (int)A->n;
  double *tmp = (double *)malloc((size_t)n * sizeof(double));

  cs_ipvec(n, S->Pinv, b1, tmp);
  cs_lsolve(N->L, tmp);
  cs_ltsolve(N->L, tmp);
  cs_pvec(n, S->Pinv, tmp, b1);

  cs_ipvec(n, S->Pinv, b2, tmp);
  cs_lsolve(N->L, tmp);
  cs_ltsolve(N->L, tmp);
  cs_pvec(n, S->Pinv, tmp, b2);

  free(tmp);
  cs_nfree(N);
  cs_sfree(S);
  cs_spfree(A);
}
#endif

// Least squares loop closure on the survey network, reduced before solving:
// - dangling branches carry no misclosure and just follow their attachment point
// - chains of degree 2 stations collapse into one edge between junctions
//   (weights combine in series), and the misclosure is spread back along them
// Both are exact eliminations, so the result matches solving the full system.

typedef struct ls_edge {
  size_t a; // x_a - x_b ~= d
  size_t b;
  double weight;
  CCPoint d;
} ls_edge;

typedef struct ls_step {
  size_t edge;
  size_t next; // node reached by this step
} ls_step;

typedef struct ls_chain {
  size_t from;
  size_t to;
  size_t first; // index into steps
  size_t count;
  CCPoint d;         // summed displacement, x_to - x_from
  double resistance; // sum of 1 / weight
} ls_chain;

enum { LS_EDGE_ALIVE, LS_EDGE_PRUNED, LS_EDGE_CHAINED };

static inline size_t ls_rep(CCStation stations[], size_t i) {
  return stations[i].closure != -1 ? (size_t)stations[i].closure : i;
}

static inline size_t ls_other(const ls_edge *e, size_t v) {
  return e->a == v ? e->b : e->a;
}

// displacement x_next - x_prev when walking edge e towards next
static inline CCPoint ls_step_d(const ls_edge *e, size_t next) {
  return e->a == next ? e->d : (CCPoint){.x = -e->d.x, .y = -e->d.y};
}

static size_t ls_alive_edge(size_t v, const size_t *adj_start, const size_t *adj,
                            const uint8_t *edge_state) {
  for (size_t k = adj_start[v]; k < adj_start[v + 1]; k++) {
    if (edge_state[adj[k]] == LS_EDGE_ALIVE) return adj[k];
  }
  return SIZE_MAX;
}

static void close_leastsquare(CCStation stations[], size_t length, CCPoint out[]) {
  // Edges between representative nodes (closures merged into their target)
  ls_edge *edges = malloc(sizeof(ls_edge) * length);
  size_t edge_count = 0;
  for (size_t i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    if (current->fixed) continue;
    size_t parent_idx = (size_t)current->parent;
    vec3 current_pos = {.x = out[i].x, .y = out[i].y, .z = current->depth};
    vec3 parent_pos = {
        .x = out[parent_idx].x, .y = out[parent_idx].y, .z = stations[parent_idx].depth};
    vec3 distance = vec3_subtract(current_pos, parent_pos);
    double wdistance = vec3_magnitude(distance);
    size_t a = ls_rep(stations, i);
    size_t b = ls_rep(stations, parent_idx);
    if (a == b) continue; // closes onto its own parent, contributes nothing
    edges[edge_count++] = (ls_edge){
        .a = a,
        .b = b,
        .weight = 1.0 / (wdistance < 0.1 ? 0.1 : wdistance),
        .d = {.x = distance.x, .y = distance.y}};
  }

  size_t *adj_start = calloc(length + 1, sizeof(size_t));
  for (size_t e = 0; e < edge_count; e++) {
    adj_start[edges[e].a + 1]++;
    adj_start[edges[e].b + 1]++;
  }
  for (size_t v = 0; v < length; v++) adj_start[v + 1] += adj_start[v];
  size_t *adj = malloc(sizeof(size_t) * 2 * edge_count);
  size_t *deg = calloc(length, sizeof(size_t));
  for (size_t e = 0; e < edge_count; e++) {
    adj[adj_start[edges[e].a] + deg[edges[e].a]++] = e;
    adj[adj_start[edges[e].b] + deg[edges[e].b]++] = e;
  }
  uint8_t *edge_state = calloc(edge_count, sizeof(uint8_t));

  // Prune dangling branches, leaf first. order doubles as work queue.
  size_t *order = malloc(sizeof(size_t) * length);
  size_t *order_edge = malloc(sizeof(size_t) * length);
  size_t head = 0, tail = 0;
  for (size_t v = 0; v < length; v++) {
    if (!stations[v].fixed && deg[v] == 1) order[tail++] = v;
  }
  while (head < tail) {
    size_t v = order[head];
    size_t e = ls_alive_edge(v, adj_start, adj, edge_state);
    order_edge[head++] = e;
    if (e == SIZE_MAX) continue;
    edge_state[e] = LS_EDGE_PRUNED;
    deg[v] = 0;
    size_t u = ls_other(&edges[e], v);
    if (--deg[u] == 1 && !stations[u].fixed) order[tail++] = u;
  }

  // Collapse chains between junctions (fixed nodes or degree != 2)
#define LS_IS_JUNCTION(v) (stations[v].fixed || deg[v] != 2)
  ls_chain *chains = malloc(sizeof(ls_chain) * (edge_count + 1));
  ls_step *steps = malloc(sizeof(ls_step) * (edge_count + 1));
  size_t chain_count = 0, step_count = 0;
  for (size_t j = 0; j < length; j++) {
    if (deg[j] == 0 || !LS_IS_JUNCTION(j)) continue;
    for (size_t k = adj_start[j]; k < adj_start[j + 1]; k++) {
      size_t e = adj[k];
      if (edge_state[e] != LS_EDGE_ALIVE) continue;
      ls_chain c = {.from = j, .first = step_count};
      size_t cur = j;
      for (;;) {
        edge_state[e] = LS_EDGE_CHAINED;
        size_t next = ls_other(&edges[e], cur);
        CCPoint sd = ls_step_d(&edges[e], next);
        c.d.x += sd.x;
        c.d.y += sd.y;
        c.resistance += 1.0 / edges[e].weight;
        steps[step_count++] = (ls_step){.edge = e, .next = next};
        c.count++;
        cur = next;
        if (LS_IS_JUNCTION(cur)) break;
        e = ls_alive_edge(cur, adj_start, adj, edge_state);
        assert(e != SIZE_MAX);
      }
      c.to = cur;
      chains[chain_count++] = c;
    }
  }

  // Solve the reduced system on the adjustable junctions
  ssize_t *junction_idx = malloc(sizeof(ssize_t) * length);
  size_t junction_count = 0;
  for (size_t v = 0; v < length; v++) {
    bool adjustable = deg[v] > 0 && !stations[v].fixed && LS_IS_JUNCTION(v);
    junction_idx[v] = adjustable ? (ssize_t)junction_count++ : -1;
  }
#undef LS_IS_JUNCTION

  if (junction_count > 0) {
    double *bx = calloc(junction_count, sizeof(double));
    double *by = calloc(junction_count, sizeof(double));
    DokBuilder *a = dok_builder_create(junction_count);

    for (size_t i = 0; i < chain_count; i++) {
      ls_chain *c = &chains[i];
      if (c->from == c->to) continue;
      double weight = 1.0 / c->resistance;
      ssize_t to_index = junction_idx[c->to];
      ssize_t from_index = junction_idx[c->from];

      if (to_index != -1) {
        bx[to_index] += weight * c->d.x;
        by[to_index] += weight * c->d.y;
        dok_builder_add(a, to_index, to_index, weight);
      }
      if (from_index != -1) {
        bx[from_index] += -weight * c->d.x;
        by[from_index] += -weight * c->d.y;
        dok_builder_add(a, from_index, from_index, weight);
      }
      if (to_index != -1 && from_index != -1) {
        dok_builder_add(a, to_index, from_index, -weight);
        dok_builder_add(a, from_index, to_index, -weight);
      } else if (to_index != -1) {
        bx[to_index] += weight * out[c->from].x;
        by[to_index] += weight * out[c->from].y;
      } else if (from_index != -1) {
        bx[from_index] += weight * out[c->to].x;
        by[from_index] += weight * out[c->to].y;
      }
    }

    solve(a, bx, by);

    for (size_t v = 0; v < length; v++) {
      ssize_t idx = junction_idx[v];
      if (idx != -1) {
        assert(bx[idx] == bx[idx]); // assert not NaN
        out[v].x = bx[idx];
        out[v].y = by[idx];
      }
    }
    free(bx);
    free(by);
    dok_builder_destroy(a);
  }

  // Spread each chain's misclosure along it, proportional to resistance
  for (size_t i = 0; i < chain_count; i++) {
    ls_chain *c = &chains[i];
    CCPoint from = out[c->from];
    double mx = out[c->to].x - from.x - c->d.x;
    double my = out[c->to].y - from.y - c->d.y;
    CCPoint acc = {0};
    double resistance = 0;
    for (size_t j = 0; j + 1 < c->count; j++) {
      ls_step *s = &steps[c->first + j];
      CCPoint sd = ls_step_d(&edges[s->edge], s->next);
      acc.x += sd.x;
      acc.y += sd.y;
      resistance += 1.0 / edges[s->edge].weight;
      double f = resistance / c->resistance;
      out[s->next].x = from.x + acc.x + mx * f;
      out[s->next].y = from.y + acc.y + my * f;
    }
  }

  // Dangling branches follow their attachment point, root first
  for (size_t i = tail; i > 0; i--) {
    size_t v = order[i - 1];
    size_t e = order_edge[i - 1];
    if (e == SIZE_MAX) continue;
    size_t u = ls_other(&edges[e], v);
    CCPoint sd = ls_step_d(&edges[e], v);
    out[v].x = out[u].x + sd.x;
    out[v].y = out[u].y + sd.y;
  }

  for (size_t i = 0; i < length; i++) {
    CCStation *s = &stations[i];
    if (s->closure != -1) {
      out[i].x = out[(size_t)s->closure].x;
      out[i].y = out[(size_t)s->closure].y;
    }
  }

  free(junction_idx);
  free(steps);
  free(chains);
  free(order_edge);
  free(order);
  free(edge_state);
  free(deg);
  free(adj);
  free(adj_start);
  free(edges);
}

CCLocation *find_datum(CCStation stations[], size_t length) {
  for (int i = 0; i < length; i++) {
    if (stations[i].fixed)
      return &stations[i].location;
  }
  return NULL;
}

static inline bool is_valid_date(CCDate d) {
  return (d.year >= 1900 && d.year <= 2500) &&
         (d.month >= 1   && d.month <= 12) &&
         (d.day >= 1     && d.day <= 31);
}

static inline bool is_leap_year(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

// Fraction of the year elapsed at the start of the given day, 0 on Jan 1
static double year_fraction(CCDate d) {
  static const uint16_t days_before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  bool leap = is_leap_year(d.year);
  double day = days_before[d.month - 1] + (d.month > 2 && leap) + (d.day - 1);
  return day / (leap ? 366.0 : 365.0);
}

static CCComputeResult get_coords(CCStation stations[],
                                   size_t length, 
                                   CCLocation *datum, 
                                   CCPoint out[],
                                   CCTransformFlags flags,
                                   bool latlon) {
  CCComputeResult result = CCStation_SUCCESS;
  datum = datum ? datum : find_datum(stations, length);
  if (datum == NULL) return CCStation_NO_DATUM;

  
  
  if (flags & CC_TRANSFORM_MAG_NORTH) {
    result = build_xy_map(stations, length, datum, out, NULL);
    if (result != CCStation_SUCCESS) return result;
    double * adj_az = calloc(length, sizeof(double));
    for (int i = 0; i < length; i++) {
      CCStation *s = &stations[i];
      if (s->fixed) continue;
      if (!is_valid_date(s->date)) continue;

      CCLocation new = gps_add(datum, out[i].x, out[i].y);
      adj_az[i] = declination(
        s->date.year, 
        s->date.month, 
        s->date.day,
        new.lat, 
        new.lon, 
        0.0);
    }
    result = build_xy_map(stations, length, datum, out, adj_az);
    free(adj_az);
    if (result != CCStation_SUCCESS) return result;
  }
  else if (flags & CC_TRANSFORM_MAG_NORTH_FAST) {
    // Declination at the datum drifts slowly, so evaluate it on Jan 1 of
    // each year and interpolate linearly within the year.
    uint16_t first_year = UINT16_MAX, last_year = 0;
    for (size_t i = 0; i < length; ++i) {
      CCStation *s = &stations[i];
      if (s->fixed) continue;
      if (!is_valid_date(s->date)) continue;
      if (s->date.year < first_year) first_year = s->date.year;
      if (s->date.year > last_year) last_year = s->date.year;
    }

    double *adj_az = (double *)calloc(length, sizeof(double));
    if (first_year <= last_year) {
      size_t knot_count = (size_t)(last_year - first_year) + 2;
      double *knots = (double *)malloc(knot_count * sizeof(double));
      for (size_t k = 0; k < knot_count; ++k) {
        knots[k] = declination((int)(first_year + k), 1, 1, datum->lat, datum->lon, 0.0);
      }
      for (size_t i = 0; i < length; ++i) {
        CCStation *s = &stations[i];
        if (s->fixed) continue;
        if (!is_valid_date(s->date)) continue;
        size_t k = s->date.year - first_year;
        adj_az[i] = knots[k] + (knots[k + 1] - knots[k]) * year_fraction(s->date);
      }
      free(knots);
    }

    result = build_xy_map(stations, length, datum, out, adj_az);

    free(adj_az);

    if (result != CCStation_SUCCESS) return result;
  }
  else {
    result = build_xy_map(stations, length, datum, out, NULL);
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