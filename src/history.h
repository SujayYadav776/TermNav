#ifndef TERMNAV_HISTORY_H
#define TERMNAV_HISTORY_H
#include "fs_ops.h"
#define HISTORY_MAX 64
typedef struct { char path[PATH_MAX], selected[NAME_MAX + 1], filter[256]; size_t cursor, scroll; } HistoryEntry;
typedef struct { HistoryEntry entries[HISTORY_MAX]; size_t count, position; } History;
void history_push(History *h, const char *path);
#endif
