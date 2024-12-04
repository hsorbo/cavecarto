#pragma once
#include "sqlite3.h"
#include "cavecarto.h"

size_t read_stations(sqlite3 *db, station *stations[]);