#ifndef TERMNAV_RICH_PREVIEW_H
#define TERMNAV_RICH_PREVIEW_H
#include "preview.h"
#include <pthread.h>
#include <stdatomic.h>
typedef struct {
    pthread_t thread;
    atomic_bool cancel, done;
    bool started;
    char path[PATH_MAX], mode[16];
    struct stat signature;
    unsigned width, height;
    bool sixel;
    Preview result;
} RichPreview;
const char *rich_mode(const Entry *entry);
int rich_start(RichPreview *job, const char *path, const Entry *entry, unsigned width, unsigned height, bool sixel);
void rich_stop(RichPreview *job);
bool rich_collect(RichPreview *job, Preview *preview);
#endif
