#include "cavecarto.h"
#include "criterion/criterion.h"
#include "matrix_builder.h"
#include <math.h>

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

Test(simple, csc) {
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
  cr_assert_arr_eq(A->p, p, 6); // no bueno
  //         // let mut values = Vec::<f64>::new();
  //         // let mut indicies = Vec::<usize>::new();
  //         // let mut col_ptr = Vec::<isize>::new();
  //         // csr(&a, &mut values, &mut indicies, &mut col_ptr);
  //         // assert_eq!(vec![8., 2., 5., 7., 1., 9., 2.], values);
  //         // assert_eq!(vec![0, 0, 1, 4, 4, 6, 4], indicies);
  //         // assert_eq!(vec![0, 1, 1, 4, 6, 7], col_ptr);
}

void print_xy(double xs[], double ys[], size_t length) {
  for (int i = 0; i < length; i++) {
    printf("%d: %f %f\n", i, xs[i], ys[i]);
  }
}

Test(simple, bowtie1) {
  station *s = malloc(sizeof(station) * 8);
  start(&s[0], (location){.lat = 60., .lon = 60.});
  start(&s[5], (location){.lat = 60., .lon = 60.018});
  rel(&s[1], &s[0], (distance){.azimuth = 0., .length = 10.});
  rel(&s[2], &s[1], (distance){.azimuth = 95., .length = 50.});
  rel(&s[3], &s[0], (distance){.azimuth = 85., .length = 50.});
  rel(&s[4], &s[5], (distance){.azimuth = 275., .length = 50.});
  rel(&s[6], &s[5], (distance){.azimuth = 0., .length = 10.});
  rel(&s[7], &s[6], (distance){.azimuth = 265., .length = 50.});

  double lon[8], lat[8];
  s[2].closure = &s[7];
  s[3].closure = &s[7];
  s[4].closure = &s[7];

  get_locations(s, 8, NULL, lon, lat, true);
  print_xy(lon, lat, 8);
  cr_assert(fabs(lat[1] - 60.000088782284315) < 1e-12);
  // cr_assert(fabs(lon[1] - 60.000000851048455) < 1e-12);

  // cr_assert(fabs(lat[2] - 60.000044391142154) < 1e-12);
  // cr_assert(fabs(lon[2] - 60.000900001207754) < 1e-12);

  // cr_assert(fabs(lat[6] - 60.000088782284315) < 1e-12);
  // cr_assert(fabs(lon[6] - 60.001799153782564) < 1e-12);

  free(s);
}