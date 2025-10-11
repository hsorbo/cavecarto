#include "cavecarto.h"
#include "dok_builder.h"
#include "utest.h"
#include "igrf.h"
#include <math.h>

const float GPS_MM = 0.00000001;
const float GPS_CM = 0.0000001;
const float GPS_M = 0.000001;
const float GPS_KM = 0.01;

void rel(CCStation *s, CCStation *parent, double azimuth, double length) {
  cc_station_init_relative(s, parent, NULL, azimuth, length, 0., (CCDate){0});
}

void start(CCStation *s, double lat, double lon) {
  cc_station_init_fixed(s, NULL, lat, lon, 0.);
}

UTEST(simple, csc) {
  DokBuilder *a = dok_builder_create(8);
  // https://matteding.github.io/2019/04/25/sparse-matrices/
  dok_builder_add(a, 0, 0, 8.0); // [8, 0, 2, 0, 0]
  dok_builder_add(a, 2, 0, 2.0); // [0, 0, 5, 0, 0]
  dok_builder_add(a, 2, 1, 5.0); // [0, 0, 0, 0, 0]
  dok_builder_add(a, 2, 4, 7.0); // [0, 0, 0, 0, 0]
  dok_builder_add(a, 3, 4, 1.0); // [0, 0, 7, 1, 2]
  dok_builder_add(a, 4, 4, 2.0); // [0, 0, 0, 0, 0]
  dok_builder_add(a, 3, 6, 9.0); // [0, 0, 0, 9, 0]
  cs *A = dok_builder_to_csc(a);
  
  int indptr[] = {0, 1, 1, 4, 6, 7};
  for (int i = 0; i < sizeof(indptr) / sizeof(indptr[0]); i++) {
    ASSERT_EQ(A->p[i], indptr[i]);
  }
  
  int indices[] = {0, 0, 1, 4, 4, 6, 4};
  for (int i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
    ASSERT_EQ(A->i[i], indices[i]);
  }

  double data[] = {8.0, 2.0, 5.0, 7.0, 1.0, 9.0, 2.0};
  for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    ASSERT_EQ(A->x[i], data[i]);
  }
}

UTEST(simple, csc_scattered) {
  // Same as csc but build order is randomized
  DokBuilder *a = dok_builder_create(8);
  dok_builder_add(a, 3, 6, 9.0);
  dok_builder_add(a, 2, 4, 7.0);
  dok_builder_add(a, 0, 0, 8.0);
  dok_builder_add(a, 4, 4, 2.0);
  dok_builder_add(a, 2, 0, 2.0);
  dok_builder_add(a, 2, 1, 5.0);
  dok_builder_add(a, 3, 4, 1.0);
  cs *A = dok_builder_to_csc(a);
  
  int indptr[] = {0, 1, 1, 4, 6, 7};
  for (int i = 0; i < sizeof(indptr) / sizeof(indptr[0]); i++) {
    ASSERT_EQ(A->p[i], indptr[i]);
  }
  
  int indices[] = {0, 0, 1, 4, 4, 6, 4};
  for (int i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
    ASSERT_EQ(A->i[i], indices[i]);
  }

  double data[] = {8.0, 2.0, 5.0, 7.0, 1.0, 9.0, 2.0};
  for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    ASSERT_EQ(A->x[i], data[i]);
  }
}

// static void print_xy(double xs[], double ys[], size_t length) {
//   for (int i = 0; i < length; i++) {
//     printf("%d: %.15f\t%.15f\n", i, xs[i], ys[i]);
//   }
// }
UTEST(simple, bowtie1) {
  CCStation s[8] = {0};
  start(&s[0], 60., 60.);
  start(&s[5], 60., 60.0018);
  rel(&s[1], &s[0], 0.,  10.);
  rel(&s[2], &s[1], 95., 50.);
  rel(&s[3], &s[0], 85., 50.);
  rel(&s[4], &s[5], 275.,50.);
  rel(&s[6], &s[5], 0.,  10.);
  rel(&s[7], &s[6], 265.,50.);

  s[2].closure = &s[7];
  s[3].closure = &s[7];
  s[4].closure = &s[7];

  CCLocation loc[8];
  cc_compute_station_latlon(s, 8, NULL, loc, CC_TRANSFORM_CLOSE_LOOPS);

  ASSERT_NEAR(loc[1].lat, 60.000088782284315, 1e-12);
  ASSERT_NEAR(loc[1].lon, 60.000000851048455, 1e-12);

  ASSERT_NEAR(loc[2].lat, 60.000044391142154, 1e-12);
  ASSERT_NEAR(loc[2].lon, 60.000900001207754, 1e-12);

  ASSERT_EQ(loc[3].lat, loc[2].lat);
  ASSERT_EQ(loc[3].lon, loc[2].lon);

  ASSERT_EQ(loc[4].lat, loc[2].lat);
  ASSERT_EQ(loc[4].lon, loc[2].lon);

  ASSERT_EQ(loc[5].lat, 60.0);
  ASSERT_EQ(loc[5].lon, 60.0018);

  ASSERT_EQ(loc[6].lat, loc[1].lat);
  ASSERT_NEAR(loc[6].lon, 60.001799153782564, 1e-12);

  ASSERT_EQ(loc[7].lat, loc[2].lat);
  ASSERT_EQ(loc[7].lon, loc[2].lon);
}

UTEST(igrf, declination) {
  /*
  Testdata from ppigrf 

  import ppigrf
  from datetime import datetime
  [Be, Bn, Bu] = ppigrf.igrf(lon=40., lat=40., h=0.0, date=datetime(2018, 12, 8))
  print(ppigrf.get_inclination_declination(Be, Bn, Bu))
  */
  double actual = declination(2018, 12, 8, 40., 40., 0.0);
  ASSERT_NEAR(6.15883078, actual, 1e-3);
}
UTEST_MAIN();