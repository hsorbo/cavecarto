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

typedef struct start_adj {
  CCStation *start_station;
  CCStation *to_station;
  double weight;
} start_adj;

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

static void close_leastsquare(CCStation stations[], size_t length, CCPoint out[]) {
  ssize_t *adjust_station_idx = malloc(sizeof(ssize_t) * length);
  size_t adj_station_count = 0;
  size_t fix_station_count = 0;
  for (int i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    bool adjustable = !current->fixed && current->closure == -1;
    // this estimation is shady, do better
    fix_station_count += current->fixed ? 1 : 0;
    fix_station_count += current->parent != -1 && stations[current->parent].fixed ? 1 : 0;
    adjust_station_idx[i] = adjustable ? adj_station_count++ : -1;
  }
  double *bx = calloc(adj_station_count, sizeof(double));
  double *by = calloc(adj_station_count, sizeof(double));

  DokBuilder *a = dok_builder_create(adj_station_count);

  start_adj *p = calloc(fix_station_count, sizeof(start_adj));
  int plen = 0;

  for (int i = 0; i < length; i++) {
    CCStation *current = &stations[i];
    if (current->fixed) {
      continue;
    }
    size_t parent_idx = (size_t)current->parent; CCStation *parent = (current->parent != -1) ? &stations[parent_idx] : NULL;
    size_t current_idx = current - stations;
        vec3 current_pos = {
        .x = out[current_idx].x, .y = out[current_idx].y, .z = current->depth};
    vec3 parent_pos = {
        .x = out[parent_idx].x, .y = out[parent_idx].y, .z = parent->depth};

    vec3 distance = vec3_subtract(current_pos, parent_pos);
    double wdistance = vec3_magnitude(distance);
    double weight = 1.0 / (wdistance < 0.1 ? 0.1 : wdistance);

    current = (current->closure != -1) ? &stations[current->closure] : current;
    parent = (parent->closure != -1) ? &stations[parent->closure] : parent;

    ssize_t to_index = adjust_station_idx[current - stations];
    ssize_t from_index = adjust_station_idx[parent - stations];

    assert((to_index == -1 && current->fixed) || (to_index >= 0 && !current->fixed));
    assert((from_index == -1 && parent->fixed) || (from_index >= 0 && !parent->fixed));

    if (!current->fixed) {
      bx[to_index] += weight * distance.x;
      by[to_index] += weight * distance.y;
      dok_builder_add(a, to_index, to_index, weight);
    }

    if (!parent->fixed) {
      bx[from_index] += -weight * distance.x;
      by[from_index] += -weight * distance.y;
      dok_builder_add(a, from_index, from_index, weight);
    }

    if (!parent->fixed && !current->fixed) {
      dok_builder_add(a, to_index, from_index, -weight);
      dok_builder_add(a, from_index, to_index, -weight);
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
  
  solve(a, bx, by);
  
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
    if (s->closure != -1) {
      out[i].x = out[(size_t)s->closure].x;
      out[i].y = out[(size_t)s->closure].y;
    }
  }
  free(bx);
  free(by);
  dok_builder_destroy(a);
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

static inline bool is_valid_date(CCDate d) {
  return (d.year >= 1900 && d.year <= 2500) &&
         (d.month >= 1   && d.month <= 12) &&
         (d.day >= 1     && d.day <= 31);
}

typedef struct {
  size_t  idx;
  CCDate  date;
  uint32_t key;
} DateIndex;

static inline uint32_t cc_date_key(CCDate d) {
  return (uint32_t)d.year * 10000u + (uint32_t)d.month * 100u + (uint32_t)d.day;
}

static int cc_cmp_dateindex(const void *a, const void *b) {
  const DateIndex *aa = (const DateIndex *)a;
  const DateIndex *bb = (const DateIndex *)b;
  if (aa->key < bb->key) return -1;
  if (aa->key > bb->key) return  1;
  return 0;
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
    DateIndex *list = (DateIndex *)malloc(length * sizeof(DateIndex));
    size_t m = 0;
    for (size_t i = 0; i < length; ++i) {
      CCStation *s = &stations[i];
      if (s->fixed) continue;
      if (!is_valid_date(s->date)) continue;
      list[m++] = (DateIndex){ .idx = i, .date = s->date, .key = cc_date_key(s->date) };
    }

    qsort(list, m, sizeof(DateIndex), cc_cmp_dateindex);

    double *adj_az = (double *)calloc(length, sizeof(double));
    uint32_t prev_key = UINT32_MAX;
    double prev_decl = 0.0;
    for (size_t k = 0; k < m; ++k) {
      DateIndex di = list[k];
      if (di.key != prev_key) {
        prev_key = di.key;
        prev_decl = declination(di.date.year, di.date.month, di.date.day,
                                datum->lat, datum->lon, 0.0);
      }
      adj_az[di.idx] = prev_decl;
    }

    result = build_xy_map(stations, length, datum, out, adj_az);

    free(adj_az);
    free(list);

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