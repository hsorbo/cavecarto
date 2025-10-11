#ifndef DOK_BUILDER_H
#define DOK_BUILDER_H

#include "csparse.h"
#include <stdlib.h>

typedef struct DokBuilder DokBuilder;

DokBuilder *dok_builder_create(size_t size);
void dok_builder_destroy(DokBuilder *mb);
void dok_builder_add(DokBuilder *builder, size_t col, size_t row, double inc);
cs *dok_builder_to_csc(DokBuilder *builder);
#endif
