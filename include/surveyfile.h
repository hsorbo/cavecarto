#pragma once
#include "cavecarto.h"
#include "sqlite3.h"

size_t read_stations(sqlite3 *db, station *stations[]);