#ifndef TERMNAV_SEARCH_H
#define TERMNAV_SEARCH_H
#include "fs_ops.h"
#include <pthread.h>
#include <stdatomic.h>
#define SEARCH_MAX 1000
typedef struct { char *path; bool directory; } SearchItem;
typedef struct {
    pthread_t thread; pthread_mutex_t mutex;
    bool initialized, started, hidden;
    atomic_bool cancel, done;
    char path[PATH_MAX], query[256];
    SearchItem items[SEARCH_MAX]; size_t count, skipped;
    int error; bool limited;
} Search;
int search_start(Search *search, const char *path, const char *query, bool hidden);
void search_stop(Search *search);
void search_destroy(Search *search);
#endif
