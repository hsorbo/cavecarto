#include "igrf.h"
#include "igrf_coeffs.h"
#include <assert.h>
#include <math.h>

#define NaN log(-1.0)
#define PI 3.141592654
#define RAD2DEG (180.0 / PI)

#define IEXT 0
#define FALSE 0
#define TRUE 1 /* constants */

#define EXT_COEFF1 (double)0
#define EXT_COEFF2 (double)0
#define EXT_COEFF3 (double)0

#define MAXDEG 13
#define MAXCOEFF                                                               \
  (MAXDEG * (MAXDEG + 2) + 1) /* index starts with 1!, (from old Fortran?) */

typedef struct dfhi {
  double d, f, h, i;
} dfhi;

typedef struct xyz {
  double x, y, z;
} xyz;

/* Geomag global variables */
typedef struct state {
  double gh1[MAXCOEFF];
  double gh2[MAXCOEFF];
  double gha[MAXCOEFF];
  struct dfhi dfhi;
  struct xyz xyz;
} state;

static double julday(int month, int day, int year);
static int interpsh(struct state *glob, double date, double dte1, int nmax1,
             double dte2, int nmax2, double *ghx);
static int extrapsh(struct state *glob, double date, double dte1, int nmax1, int nmax2,
             double *ghx);
static void shval3(int igdgc, double flat, double flon, double elev, int nmax,
            int iext, double ext1, double ext2, double ext3, xyz *xyz,
            double *ghx);
static void dihf(xyz *xyz, dfhi *dfhi);
static void getshc(struct state *ctx, int model_index, int iflag, int nmax_of_gh,
            int gh);

typedef struct input {
  double latitude;
  double longitude;
  double alt;
  double sdate;
} input;

double declination(int year, int month, int day, double latitude, double longitude,
                  double altitude) {
  struct state glob;
  int nmax;
  input input = {.alt = altitude,
                 .latitude = latitude,
                 .longitude = longitude,
                 .sdate = julday(month, day, year)};
  /*  Obtain the desired model file and read the data  */

  /* Pick model */
  int modelI;
  for (modelI = 0; modelI < MAXMOD; modelI++)
    if (input.sdate < models[modelI].yrmax)
      break;
  if (modelI == MAXMOD)
    modelI--; /* if beyond end of last model use last model */

  // convert meter
  input.alt *= 0.001;
  assert(input.alt > models[modelI].altmin && input.alt < models[modelI].altmax);

  

  // Enter the decimal latitude (-90 to 90) (- for Southern hemisphere).
  assert(input.latitude >= -90 && input.latitude <= 90);

  // Enter the decimal longitude (-180 to 180) (- for Western "hemisphere).
  assert(input.longitude > -180);
  assert(input.longitude < 180);

  int model_from = modelI;
  int model_to = modelI < (MAXMOD - 1) ? modelI + 1 : modelI;
  model_t *fra = &models[model_from];
  model_t *til = &models[model_from + 1];

  if (models[modelI].max2 == 0) {
    getshc(&glob, model_from, 1, fra->max1, 1);
    getshc(&glob, model_to, 1, til->max1, 2);

    nmax = interpsh(&glob, input.sdate, fra->yrmin, fra->max1, til->yrmin,
                    til->max1, glob.gha);
  } else {
    getshc(&glob, model_from, 1, fra->max1, 1);
    getshc(&glob, model_to, 0, fra->max2, 2);

    nmax = extrapsh(&glob, input.sdate, fra->epoch, fra->max1, fra->max2,
                    glob.gha);
  }

  /* Do the first calculations */
  int igdgc = 1;
  shval3(igdgc, input.latitude, input.longitude, input.alt, nmax, IEXT,
         EXT_COEFF1, EXT_COEFF2, EXT_COEFF3, &glob.xyz, glob.gha);
  dihf(&glob.xyz, &glob.dfhi);

  glob.dfhi.d = glob.dfhi.d * (RAD2DEG);
  glob.dfhi.i = glob.dfhi.i * (RAD2DEG);
  return glob.dfhi.d;
}

static double julday(int month, int day, int year) {
  int days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int leap_year =
      (((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0)));
  double day_in_year = (days[month - 1] + day + (month > 2 ? leap_year : 0));
  return ((double)year + (day_in_year / (365.0 + leap_year)));
}

static void getshc(struct state *ctx, int model_index, int iflag, int nmax_of_gh,
            int gh) {
  assert(iflag == 1 || iflag == 0);
  model_t mod = models[model_index];
  int ii = 0;
  int j = 0;
  for (int nn = 1; nn <= nmax_of_gh; ++nn) {
    for (int mm = 0; mm <= nn; ++mm) {
      coeffs_t c = mod.coeffs[j++];
      assert(nn == c.n && mm == c.m);
      ii = ii + 1;
      double *ghx = gh == 1 ? ctx->gh1 : ctx->gh2;
      ghx[ii] = iflag == 1 ? c.g1 : c.g2;
      if (c.m != 0) {
        ii = ii + 1;
        ghx[ii] = iflag == 1 ? c.h1 : c.h2;
      }
    }
  }
}

static int extrapsh(struct state *ctx, double date, double dte1, int nmax1, int nmax2,
             double *ghx) {
  int nmax;
  int k, l;

  double factor = date - dte1;
  if (nmax1 == nmax2) {
    k = nmax1 * (nmax1 + 2);
    nmax = nmax1;
  } else {
    if (nmax1 > nmax2) {
      k = nmax2 * (nmax2 + 2);
      l = nmax1 * (nmax1 + 2);

      for (int ii = k + 1; ii <= l; ++ii) {
        ghx[ii] = ctx->gh1[ii];
      }
      nmax = nmax1;
    } else {
      k = nmax1 * (nmax1 + 2);
      l = nmax2 * (nmax2 + 2);

      for (int ii = k + 1; ii <= l; ++ii) {
        ghx[ii] = factor * ctx->gh2[ii];
      }
      nmax = nmax2;
    }
  }

  for (int ii = 1; ii <= k; ++ii) {
    ctx->gha[ii] = ctx->gh1[ii] + factor * ctx->gh2[ii];
  }

  return (nmax);
}

static int interpsh(struct state *ctx, double date, double dte1, int nmax1,
             double dte2, int nmax2, double *ghx) {
  int nmax;
  int k, l;
  double factor = (date - dte1) / (dte2 - dte1);

  if (nmax1 == nmax2) {
    k = nmax1 * (nmax1 + 2);
    nmax = nmax1;
  } else {
    if (nmax1 > nmax2) {
      k = nmax2 * (nmax2 + 2);
      l = nmax1 * (nmax1 + 2);
      for (int ii = k + 1; ii <= l; ++ii) {
        ghx[ii] = ctx->gh1[ii] + factor * (-ctx->gh1[ii]);
      }
      nmax = nmax1;
    } else {
      k = nmax1 * (nmax1 + 2);
      l = nmax2 * (nmax2 + 2);
      for (int ii = k + 1; ii <= l; ++ii) {
        ghx[ii] = factor * ctx->gh2[ii];
      }
      nmax = nmax2;
    }
  }
  for (int ii = 1; ii <= k; ++ii) {
    ghx[ii] = ctx->gh1[ii] + factor * (ctx->gh2[ii] - ctx->gh1[ii]);
  }
  return (nmax);
}

static void shval3(int igdgc, double flat, double flon, double elev, int nmax,
            int iext, double ext1, double ext2, double ext3, xyz *xyz,
            double *ghx) {
  double earths_radius = 6371.2;
  double dtr = 0.01745329;
  double aa, bb, cc, dd;
  double rr = 0;
  double fn = 0;
  double sl[14];
  double cl[14];
  double p[119];
  double q[119];
  double a2 = 40680631.59; /* WGS84 */
  double b2 = 40408299.98; /* WGS84 */
  double r = elev;
  double slat = sin(flat * dtr);
  if ((90.0 - flat) < 0.001) {
    aa = 89.999; /*  300 ft. from North pole  */
  } else {
    if ((90.0 + flat) < 0.001) {
      aa = -89.999; /*  300 ft. from South pole  */
    } else {
      aa = flat;
    }
  }
  double clat = cos(aa * dtr);
  sl[1] = sin(flon * dtr);
  cl[1] = cos(flon * dtr);

  xyz->x = 0;
  xyz->y = 0;
  xyz->z = 0;

  double sd = 0.0;
  double cd = 1.0;
  int l = 1;
  int n = 0;
  int m = 1;
  int npq = (nmax * (nmax + 3)) / 2;
  if (igdgc == 1) {
    aa = a2 * clat * clat;
    bb = b2 * slat * slat;
    cc = aa + bb;
    dd = sqrt(cc);
    r = sqrt(elev * (elev + 2.0 * dd) + (a2 * aa + b2 * bb) / cc);
    cd = (elev + dd) / r;
    sd = (a2 - b2) / dd * slat * clat / r;
    aa = slat;
    slat = slat * cd - clat * sd;
    clat = clat * cd + aa * sd;
  }
  double ratio = earths_radius / r;
  aa = sqrt(3.0);
  p[1] = 2.0 * slat;
  p[2] = 2.0 * clat;
  p[3] = 4.5 * slat * slat - 1.5;
  p[4] = 3.0 * aa * clat * slat;
  q[1] = -clat;
  q[2] = slat;
  q[3] = -3.0 * clat * slat;
  q[4] = aa * (slat * slat - clat * clat);
  for (int k = 1; k <= npq; ++k) {
    if (n < m) {
      m = 0;
      n = n + 1;
      rr = pow(ratio, n + 2);
      fn = n;
    }
    double fm = m;
    if (k >= 5) {
      if (m == n) {
        aa = sqrt((1.0 - 0.5 / fm));
        int j = k - n - 1;
        p[k] = (1.0 + 1.0 / fm) * aa * clat * p[j];
        q[k] = aa * (clat * q[j] + slat / fm * p[j]);
        sl[m] = sl[m - 1] * cl[1] + cl[m - 1] * sl[1];
        cl[m] = cl[m - 1] * cl[1] - sl[m - 1] * sl[1];
      } else {
        aa = sqrt(fn * fn - fm * fm);
        bb = sqrt(((fn - 1.0) * (fn - 1.0)) - (fm * fm)) / aa;
        cc = (2.0 * fn - 1.0) / aa;
        int ii = k - n;
        int j = k - 2 * n + 1;
        p[k] = (fn + 1.0) * (cc * slat / fn * p[ii] - bb / (fn - 1.0) * p[j]);
        q[k] = cc * (slat * q[ii] - clat / fn * p[ii]) - bb * q[j];
      }
    }
    aa = rr * ghx[l];
    if (m == 0) {
      xyz->x = xyz->x + aa * q[k];
      xyz->z = xyz->z - aa * p[k];
      l = l + 1;
    } else {
      bb = rr * ghx[l + 1];
      cc = aa * cl[m] + bb * sl[m];
      xyz->x = xyz->x + cc * q[k];
      xyz->z = xyz->z - cc * p[k];
      if (clat > 0) {
        xyz->y = xyz->y +
                 (aa * sl[m] - bb * cl[m]) * fm * p[k] / ((fn + 1.0) * clat);
      } else {
        xyz->y = xyz->y + (aa * sl[m] - bb * cl[m]) * q[k] * slat;
      }
      l = l + 2;
    }
    m = m + 1;
  }
  if (iext != 0) {
    aa = ext2 * cl[1] + ext3 * sl[1];
    xyz->x = xyz->x - ext1 * clat + aa * slat;
    xyz->y = xyz->y + ext2 * sl[1] - ext3 * cl[1];
    xyz->z = xyz->z + ext1 * slat + aa * clat;
  }

  aa = xyz->x;
  xyz->x = xyz->x * cd + xyz->z * sd;
  xyz->z = xyz->z * cd - aa * sd;
}

static void dihf(xyz *xyz, dfhi *dfhi) {
  double sn = 0.0001;
  for (int j = 1; j <= 1; ++j) {
    double h2 = xyz->x * xyz->x + xyz->y * xyz->y;
    double argument = h2;
    dfhi->h = sqrt(argument); /* calculate horizontal intensity */
    argument = h2 + xyz->z * xyz->z;
    dfhi->f = sqrt(argument); /* calculate total intensity */
    if (dfhi->f < sn) {
      dfhi->d = NaN; /* If d and i cannot be determined, */
      dfhi->i = NaN; /*       set equal to NaN         */
    } else {
      argument = xyz->z;
      double argument2 = dfhi->h;
      dfhi->i = atan2(argument, argument2);
      if (dfhi->h < sn) {
        dfhi->d = NaN;
      } else {
        double hpx = dfhi->h + xyz->x;
        if (hpx < sn) {
          dfhi->d = PI;
        } else {
          argument = xyz->y;
          argument2 = hpx;
          dfhi->d = 2.0 * atan2(argument, argument2);
        }
      }
    }
  }
}
