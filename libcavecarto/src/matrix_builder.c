#include "matrix_builder.h"
#include "assert.h"
#include "cavecarto.h"
#include <math.h>
#include <string.h>

matrix_builder *mb_init(size_t size) {
  matrix_builder *a = malloc(sizeof(matrix_builder));
  a->cols = malloc(sizeof(column_info) * size);
  a->col_count = size;
  bzero(a->cols, sizeof(column_info) * size);
  return a;
}

void mb_free(matrix_builder *mb) {
  free(mb->cols);
  free(mb);
}

int cmp(const void *a, const void *b) {
  rowdata *aa = (rowdata *)a;
  rowdata *bb = (rowdata *)b;
  return aa->row - bb->row;
}

void sort_cols(rowdata *row, size_t length) {
  qsort(row, length, sizeof(rowdata), cmp);
}

void mb_update(matrix_builder *builder, size_t col, size_t row, double inc) {
  column_info *colinfo = &builder->cols[col];
  for (int i = 0; i < colinfo->row_count; i++) {
    if (colinfo->rows[i].row == row) {
      colinfo->rows[i].val += inc;
      return;
    }
  }
  if (colinfo->row_count < MAX_ROWS) {
    colinfo->rows[colinfo->row_count++] = (rowdata){.row = row, .val = inc};
    return;
  }
  assert(false);
}

cs *mb_csc(matrix_builder *builder) {
  size_t num_values = 0;
  for (int x = 0; x < builder->col_count; x++) {
    column_info *ax = &builder->cols[x];
    sort_cols(ax->rows, ax->row_count);
    num_values += ax->row_count;
  }

  cs *A = cs_spalloc(builder->col_count, builder->col_count, num_values, 1, 0);
  bzero(A->p, sizeof(int) * builder->col_count);
  assert(A != NULL);
  assert(A->x != NULL);

  num_values = 0;
  int prev_col = -1;
  for (size_t col = 0; col < builder->col_count; col++) {
    column_info *rowcoll = &builder->cols[col];
    for (int k = prev_col; k < col; k++) {
      if (k + 1 > builder->col_count)
        break;
      A->p[k + 1] = num_values;
    }
    prev_col = col;
    for (size_t i = 0; i < rowcoll->row_count; i++) {
      rowdata row = rowcoll->rows[i];
      A->i[num_values] = row.row;
      A->x[num_values++] = row.val;
    }
  }
  A->p[prev_col + 1] = (int)num_values;
  return A;
}
