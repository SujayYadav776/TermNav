#ifndef TERMNAV_USAGE_H
#define TERMNAV_USAGE_H
#include "fs_ops.h"
#include <pthread.h>
#include <stdatomic.h>
typedef struct { uint64_t bytes, allocated, files, directories, errors; } Usage;
int usage_measure(const char *path, Usage *out, ProgressFn cancel, void *ctx);
typedef struct { char name[NAME_MAX + 1]; Usage usage; bool directory, complete; } UsageItem;
typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    bool initialized, started;
    atomic_bool cancel, done;
    char path[PATH_MAX];
    UsageItem *items;
    size_t count, completed;
    Usage total;
    int error;
} UsageScan;
int usage_start(UsageScan *scan, const char *path);
void usage_stop(UsageScan *scan);
void usage_destroy(UsageScan *scan);
#endif
