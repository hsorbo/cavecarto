#include "cavecarto.h"
#include "matrix_builder.h"
#include "utest.h"
#include "igrf.h"
#include <math.h>

const float GPS_MM = 0.00000001;
const float GPS_CM = 0.0000001;
const float GPS_M = 0.000001;
const float GPS_KM = 0.01;

void rel(station *s, station *parent, distance d) {
  s->fixed = false;
  s->distance = d;
  s->parent = parent;
  s->depth = 0.;
  s->date = (struct tm){0};
}
void start(station *s, location l) {
  s->fixed = true;
  s->location = l;
  s->parent = NULL;
  s->depth = 0.;
  s->date = (struct tm){0};
}

UTEST(simple, csc) {
  matrix_builder *a = mb_init(8);
  // https://matteding.github.io/2019/04/25/sparse-matrices/
  mb_update(a, 0, 0, 8.0); // [8, 0, 2, 0, 0]
  mb_update(a, 2, 0, 2.0); // [0, 0, 5, 0, 0]
  mb_update(a, 2, 1, 5.0); // [0, 0, 0, 0, 0]
  mb_update(a, 2, 4, 7.0); // [0, 0, 0, 0, 0]
  mb_update(a, 3, 4, 1.0); // [0, 0, 7, 1, 2]
  mb_update(a, 4, 4, 2.0); // [0, 0, 0, 0, 0]
  mb_update(a, 3, 6, 9.0); // [0, 0, 0, 9, 0]
  cs *A = mb_csc(a);
  int p[] = {0, 1, 1, 4, 6, 7};
  for (int i = 0; i < 6; i++) {
    ASSERT_EQ(A->p[i], p[i]);
  }
}

// static void print_xy(double xs[], double ys[], size_t length) {
//   for (int i = 0; i < length; i++) {
//     printf("%d: %.15f\t%.15f\n", i, xs[i], ys[i]);
//   }
// }
UTEST(simple, bowtie1) {
  station s[8] = {0};
  start(&s[0], (location){.lat = 60., .lon = 60.});
  start(&s[5], (location){.lat = 60., .lon = 60.0018});
  rel(&s[1], &s[0], (distance){.azimuth = 0., .length = 10.});
  rel(&s[2], &s[1], (distance){.azimuth = 95., .length = 50.});
  rel(&s[3], &s[0], (distance){.azimuth = 85., .length = 50.});
  rel(&s[4], &s[5], (distance){.azimuth = 275., .length = 50.});
  rel(&s[6], &s[5], (distance){.azimuth = 0., .length = 10.});
  rel(&s[7], &s[6], (distance){.azimuth = 265., .length = 50.});

  s[2].closure = &s[7];
  s[3].closure = &s[7];
  s[4].closure = &s[7];

  double lon[8], lat[8];
  get_coords(s, 8, NULL, lon, lat, COORDS_LAT_LON | COORDS_CLOSE_LOOPS);

  ASSERT_NEAR(lat[1], 60.000088782284315, 1e-12);
  ASSERT_NEAR(lon[1], 60.000000851048455, 1e-12);

  ASSERT_NEAR(lat[2], 60.000044391142154, 1e-12);
  ASSERT_NEAR(lon[2], 60.000900001207754, 1e-12);

  ASSERT_EQ(lat[3], lat[2]);
  ASSERT_EQ(lon[3], lon[2]);

  ASSERT_EQ(lat[4], lat[2]);
  ASSERT_EQ(lon[4], lon[2]);

  ASSERT_EQ(lat[5], 60.0);
  ASSERT_EQ(lon[5], 60.0018);

  ASSERT_EQ(lat[6], lat[1]);
  ASSERT_NEAR(lon[6], 60.001799153782564, 1e-12);

  ASSERT_EQ(lat[7], lat[2]);
  ASSERT_EQ(lon[7], lon[2]);
}

UTEST(igrf, declination) {
  double actual = declination(2018, 12, 8, 40., 40., 0.0);
  ASSERT_NEAR(actual, 6.162, 1e-3);
}
UTEST_MAIN();