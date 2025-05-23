#!/bin/sh
IGNORE="(csparse|matrix_builder|igrf|igrf_coeffs)\.h"
OUT=cavecarto.c
echo > $OUT
for file in csparse.h matrix_builder.h igrf_coeffs.h igrf.h csparse.c matrix_builder.c igrf.c cavecarto.c
do
  cat src/$file | grep -vE $IGNORE >> $OUT
  echo >> $OUT
done

