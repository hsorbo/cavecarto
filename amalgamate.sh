#!/bin/sh
IGNORE="(csparse|dok_builder|igrf|igrf_coeffs)\.h"
OUT=cavecarto.c
echo > $OUT
for file in csparse.h dok_builder.h igrf_coeffs.h igrf.h csparse.c dok_builder.c igrf.c cavecarto.c
do
  cat src/$file | grep -vE $IGNORE >> $OUT
  echo >> $OUT
done

