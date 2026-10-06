#ifndef TERMNAV_DRIVES_H
#define TERMNAV_DRIVES_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#define DRIVE_MAX 16
typedef struct { char path[PATH_MAX], source[PATH_MAX]; uint64_t total, available; } Drive;
typedef struct { Drive items[DRIVE_MAX]; size_t count; } Drives;
int drives_read(Drives *drives);
#endif
