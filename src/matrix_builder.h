#pragma once
#include "csparse.h"
#include <stdlib.h>

typedef struct rowdata {
  size_t row;
  double val;
} rowdata;

#define MAX_ROWS 10
typedef struct column_info {
  rowdata rows[MAX_ROWS];
  size_t row_count;
} column_info;

typedef struct matrix_builder {
  column_info *cols;
  size_t col_count; // row-col-count as symmetric
} matrix_builder;

matrix_builder *mb_init(size_t size);
void mb_free(matrix_builder *mb);
void mb_update(matrix_builder *builder, size_t col, size_t row, double inc);
cs *mb_csc(matrix_builder *builder);