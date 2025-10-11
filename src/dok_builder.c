#include "dok_builder.h"
#include "assert.h"
#include "stdbool.h"
#include <string.h>

#define DOK_INLINE_CAP 5

typedef struct RowEntry {
  size_t row;
  double val;
} RowEntry;

typedef struct ColumnInfo {
  RowEntry * rows;
  RowEntry inline_rows[DOK_INLINE_CAP];
  size_t size;
  size_t capacity;
} ColumnInfo;

struct DokBuilder {
  ColumnInfo * cols;
  size_t col_count;
};

static inline void column_free(ColumnInfo *c) {
  if (c->rows != c->inline_rows) free(c->rows);
}

static inline void column_init(ColumnInfo *c) {
  c->rows = c->inline_rows;
  c->size = 0;
  c->capacity = DOK_INLINE_CAP;
}

static inline void column_ensure_capacity(ColumnInfo *c, size_t needed) {
  if (needed <= c->capacity) return;

  size_t new_cap = c->capacity * 2;
  if (new_cap < DOK_INLINE_CAP * 2) new_cap = DOK_INLINE_CAP * 2;
  if (new_cap < needed) new_cap = needed;

  if (c->rows == c->inline_rows) {
    RowEntry *p = malloc(new_cap * sizeof *p);
    memcpy(p, c->inline_rows, c->size * sizeof *p);
    c->rows = p;
  } else {
    c->rows = realloc(c->rows, new_cap * sizeof *c->rows);
  }

  c->capacity = new_cap;
}

static inline void column_add_value(ColumnInfo *c, size_t row, double val) {
  column_ensure_capacity(c, c->size + 1);
  c->rows[c->size++] = (RowEntry){.row = row, .val = val};
}

DokBuilder *dok_builder_create(size_t size) {
  DokBuilder *a = malloc(sizeof(DokBuilder));
  a->cols = malloc(sizeof(ColumnInfo) * size);
  a->col_count = size;
  for (size_t i = 0; i < size; ++i) column_init(&a->cols[i]);
  return a;
}

void dok_builder_destroy(DokBuilder *dok) {
  for (size_t i = 0; i < dok->col_count; ++i) column_free(&dok->cols[i]);
  free(dok->cols);
  free(dok);
}

static int compare_row_entry(const void *a, const void *b) {
  RowEntry *aa = (RowEntry *)a;
  RowEntry *bb = (RowEntry *)b;
  return (int)(aa->row - bb->row);
}

static void sort_cols(RowEntry *row, size_t length) {
  qsort(row, length, sizeof(RowEntry), compare_row_entry);
}

void dok_builder_add(DokBuilder *builder, size_t col, size_t row, double inc) {
  ColumnInfo *colinfo = &builder->cols[col];
  for (int i = 0; i < colinfo->size; i++) {
    if (colinfo->rows[i].row == row) {
      colinfo->rows[i].val += inc;
      return;
    }
  }
  column_add_value(colinfo, row, inc);
}

// Sparse parlance: number of nonzeros (nnz)
static size_t dok_builder_nnz(DokBuilder *builder) {
  size_t num_values = 0;
  for (int x = 0; x < builder->col_count; x++) {
    num_values += builder->cols[x].size;
  }
  return num_values;
}

cs *dok_builder_to_csc(DokBuilder *builder) {
  int n = (int)builder->col_count;
  cs *A = cs_spalloc(n, n, (int)dok_builder_nnz(builder), 1, 0);
  memset(A->p, 0, sizeof(int) * builder->col_count);
  assert(A != NULL);
  assert(A->x != NULL);

  size_t num_values = 0;
  int prev_col = -1;
  for (size_t col = 0; col < builder->col_count; col++) {
    ColumnInfo * rowcoll = &builder->cols[col];
    sort_cols(rowcoll->rows, rowcoll->size);
    for (int k = prev_col; k < col; k++) {
      if (k + 1 > builder->col_count)
        break;
      A->p[k + 1] = (int)num_values;
    }
    prev_col = (int)col;
    for (size_t i = 0; i < rowcoll->size; i++) {
      RowEntry row = rowcoll->rows[i];
      A->i[num_values] = (int)row.row;
      A->x[num_values++] = row.val;
    }
  }
  A->p[prev_col + 1] = (int)num_values;
  return A;
}
