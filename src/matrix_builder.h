#ifndef MATRIX_BUILDER_H
#define MATRIX_BUILDER_H

#include "csparse.h"
#include <stdlib.h>

typedef struct matrix_builder matrix_builder;

matrix_builder *mb_init(size_t size);
void mb_free(matrix_builder *mb);
void mb_update(matrix_builder *builder, size_t col, size_t row, double inc);
cs *mb_csc(matrix_builder *builder);
#endif
