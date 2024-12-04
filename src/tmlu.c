#include "tmlu.h"
#include "assert.h"
#include <expat.h>

typedef struct parser_state {
  bool parsing;
  void (*on_survey_data)(survey_data *data, void *userData);
  void *userData;
  char value[4096];
  char *valueptr;
  survey_data *data;
  int count;
} parser_state;

void XMLCALL handler(void *userData, const XML_Char *s, int len) {
  parser_state *state = (parser_state *)userData;
  if (!state->parsing)
    return;
  if (len == 1 && s[0] == '\n') // WHY?
    return;
  state->valueptr = stpncpy(state->valueptr, s, len);
}

static void XMLCALL startElement(void *userData, const char *name,
                                 const char **atts) {
  parser_state *state = (parser_state *)userData;
  if (strcmp(name, "SRVD") == 0) {
    state->parsing = true;
  }
}

static void XMLCALL endElement(void *userData, const char *name) {
  parser_state *state = (parser_state *)userData;
  if (!state->parsing)
    return;

  if (strcmp(name, "ID") == 0)
    state->data->id = strtol(state->value, NULL, 10);
  else if (strcmp(name, "CM") == 0)
    strcpy(state->data->comment, state->value);
  else if (strcmp(name, "DT") == 0)
    strptime(state->value, "%Y-%m-%d", &state->data->date);
  else if (strcmp(name, "CID") == 0)
    state->data->close_to_id = strtol(state->value, NULL, 10);
  else if (strcmp(name, "DP") == 0)
    state->data->depth = strtod(state->value, NULL);
  else if (strcmp(name, "LT") == 0)
    state->data->lat = strtod(state->value, NULL);
  else if (strcmp(name, "LGT") == 0)
    state->data->lon = strtod(state->value, NULL);
  else if (strcmp(name, "AZ") == 0)
    state->data->azimuth = strtod(state->value, NULL);
  else if (strcmp(name, "LG") == 0)
    state->data->len = strtod(state->value, NULL);
  else if (strcmp(name, "FRID") == 0)
    state->data->parent = strtol(state->value, NULL, 10);
  else if (strcmp(name, "TY") == 0) {
    if (strncmp(state->value, "START", 5) == 0)
      state->data->type = Start;
    else if (strncmp(state->value, "REAL", 4) == 0)
      state->data->type = Real;
    else if (strncmp(state->value, "VIRTUAL", 7) == 0)
      state->data->type = Real;
    else if (strncmp(state->value, "CLOSURE", 7) == 0)
      state->data->type = Closure;
  } else if (strcmp(name, "SRVD") == 0) {
    state->parsing = false;
    if (state->on_survey_data)
      state->on_survey_data(state->data, state->userData);
    bzero(state->data, sizeof(survey_data));
    state->count++;
  }
  bzero(state->value, sizeof(state->value));
  state->valueptr = state->value;
}

int parse(FILE *fp, void (*on_survey_data)(survey_data *data, void *userData),
          void *userData) {
  parser_state state = {.parsing = false,
                        .userData = userData,
                        .on_survey_data = on_survey_data,
                        .data = calloc(1, sizeof(survey_data))};
  bzero(state.value, sizeof(state.value));
  state.valueptr = state.value;

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &state);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, handler);
  char buf[BUFSIZ];
  bool done = false;
  do {
    int len = (int)fread(buf, 1, sizeof(buf), fp);
    done = len < sizeof(buf);
    if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR) {
      // todo handle error
      fprintf(stderr, "Parse error at line %ld:\n%s\n",
              (long)XML_GetCurrentLineNumber(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
      return -1;
    }
  } while (!done);
  XML_ParserFree(parser);
  return state.count;
}

typedef struct lookup {
  int id;
  enum Type type;
  union {
    station *station;
  };
  union {
    int parent_id;
    int closure_ids[2];
  };
} lookup;

typedef struct context {
  lookup *ids;
  int ids_count;
  cave *cave;
  int closure_count;
  tmlu_meta *meta;
} context;

static lookup *find(lookup *l, int size, int id) {
  int left = 0, right = size - 1;
  while (left <= right) {
    int mid = left + (right - left) / 2;
    lookup *cur = l + mid;
    if (cur->id == id) {
      return cur;
    }
    if (cur->id < id)
      left = mid + 1;
    else
      right = mid - 1;
  }
  return NULL;
}

static void parser(survey_data *data, void *userData) {
  context *ctx = (context *)userData;
  lookup *l = &ctx->ids[ctx->ids_count++];

  l->id = data->id;
  l->type = data->type;
  if (data->type == Closure) {
    ctx->closure_count++;
    l->closure_ids[0] = data->parent;
    l->closure_ids[1] = data->close_to_id;
  } else if (data->type == Real) {
    tmlu_meta *meta = &ctx->meta[ctx->cave->stations_count];
    meta->id = data->id;
    l->parent_id = data->parent;
    l->station = &ctx->cave->stations[ctx->cave->stations_count++];
    l->station->fixed = false;
    l->station->date = data->date;
    l->station->userData = meta;
    l->station->closure = NULL;
    l->station->depth = data->depth;
    l->station->distance.azimuth = data->azimuth;
    l->station->distance.length = data->len;
    l->station->parent = NULL; // cave_add_relative needs this not null
    // performance hack, if n-element before is parent set it now
    for (int i = ctx->ids_count - 2; i > 0 && i > ctx->ids_count - 5; i--) {
      lookup *forfather = &ctx->ids[i];
      if (forfather->id == l->parent_id) {
        l->station->parent = forfather->station;
        break;
      }
    }

  } else if (data->type == Start) {
    tmlu_meta *meta = &ctx->meta[ctx->cave->stations_count];
    meta->id = data->id;
    l->parent_id = 0;
    l->station = cave_add_fixed(ctx->cave,
                                (location){.lat = data->lat, .lon = data->lon},
                                data->depth, data->date, meta);
  }
}

typedef struct bucket {
  station *stations[100];
  size_t stations_count;
} bucket;

bool bucket_contains(bucket *b, station *search) {
  for (int j = 0; j < b->stations_count; j++) {
    station *s = b->stations[j];
    if (s == search) {
      return true;
    }
  }
  return false;
}

void bucket_add(bucket *b, station *s) {
  for (int i = 0; i < b->stations_count; i++) {
    if (b->stations[i] == s) {
      return;
    }
  }
  b->stations[b->stations_count++] = s;
}

typedef struct holderhoder {
  bucket bucket[10000];
  size_t bucket_count;
} holderhoder;

cave *open_file(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror("fopen");
    return NULL;
  }
  cave *cave = cave_init_with_capacity(100000);
  context ctx = {.meta = malloc(sizeof(tmlu_meta) * 100000),
                 .ids = malloc(sizeof(lookup) * 100000),
                 .ids_count = 0,
                 .cave = cave};
  bzero(ctx.meta, sizeof(tmlu_meta) * 100000);
  parse(fp, parser, &ctx);
  fclose(fp);

  holderhoder *h = malloc(sizeof(holderhoder));
  bzero(h, sizeof(holderhoder));

  for (int i = 0; i < ctx.ids_count; i++) {
    lookup *l = &ctx.ids[i];
    if (l->type == Closure) {
      lookup *a = find(ctx.ids, ctx.ids_count, l->closure_ids[0]);
      lookup *b = find(ctx.ids, ctx.ids_count, l->closure_ids[1]);
      if (a == NULL || a->type == Closure || b == NULL || b->type == Closure) {
        fprintf(stderr, "Missing closure ids %d %d \n", l->closure_ids[0],
                l->closure_ids[1]);
        cave_free(cave);
        goto beach;
      }
      bucket *found_a = NULL;
      bucket *found_b = NULL;
      for (int i = 0; i < h->bucket_count; i++) {
        bucket *bucket = &h->bucket[i];
        if (bucket_contains(bucket, a->station))
          found_a = bucket;
        if (bucket_contains(bucket, b->station))
          found_b = bucket;
      }

      if (found_a != NULL && found_b == NULL) {
        bucket_add(found_a, b->station);
      } else if (found_a == NULL && found_b != NULL) {
        bucket_add(found_b, a->station);
      } else if (found_a != NULL && found_b != NULL && found_a != found_b) {
        for (int i = 0; i < found_b->stations_count; i++) {
          bucket_add(found_a, found_b->stations[i]);
        }
        found_b->stations_count = 0;
        bucket_add(found_a, a->station);
        bucket_add(found_a, a->station);
      }

      else if (found_a == NULL && found_b == NULL) {
        h->bucket[h->bucket_count].stations[0] = a->station;
        h->bucket[h->bucket_count].stations[1] = b->station;
        h->bucket[h->bucket_count].stations_count = 2;
        h->bucket_count++;
      }
    } else {
      if (l->type == Real && l->station->parent == NULL) {
        lookup *parent = find(ctx.ids, ctx.ids_count, l->parent_id);
        if (parent == NULL || parent->type == Closure) {
          fprintf(stderr, "Missing parent id\n");
          cave_free(cave);
          goto beach;
        }
        l->station->parent = parent->station;
      }
    }
  }

  for (int i = 0; i < h->bucket_count; i++) {
    bucket *bucket = &h->bucket[i];
    station *master = NULL;
    for (int j = 0; j < bucket->stations_count; j++) {
      station *s = bucket->stations[j];
      if (s->fixed) {
        master = s;
        break;
      }
      if (master == NULL || master > s) {
        master = s;
      }
    }
    for (int j = 0; j < bucket->stations_count; j++) {
      station *s = bucket->stations[j];
      if (s == master)
        continue;
      s->closure = master;
    }
  }
  free(ctx.ids);
  free(ctx.meta);
  free(h);
  // cave->stations =
  //     realloc(cave->stations, sizeof(station) * cave->stations_count);
  return cave;
beach:
  free(ctx.ids);
  return NULL;
}
