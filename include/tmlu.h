#pragma once
#include "cavecarto.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum Type { Start = 1, Real, Closure };

typedef struct tmlu_meta {
  int id;
} tmlu_meta;

typedef struct survey_data {
  int id;
  int parent;
  int close_to_id;
  enum Type type;
  double lat;
  double lon;
  double len;
  double azimuth;
  double depth;
  struct tm date;
  char comment[4096];
} survey_data;

int parse(FILE *fp, void (*on_survey_data)(survey_data *data, void *userData),
          void *userData);
cave *open_file(const char *filename);