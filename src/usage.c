#define _GNU_SOURCE
#include "usage.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int measure_at(int parent, const char *name, Usage *out, int depth, ProgressFn cancel, void *ctx) {
    if (cancel && cancel(0, 0, ctx)) { errno = ECANCELED; return -1; }
    if (depth > 128) { ++out->errors; return 0; }
    struct stat st;
    if (fstatat(parent, name, &st, AT_SYMLINK_NOFOLLOW) < 0) { ++out->errors; return 0; }
    out->allocated += (uint64_t)(st.st_blocks > 0 ? st.st_blocks : 0) * 512;
    if (!S_ISDIR(st.st_mode)) {
        ++out->files;
        if (S_ISREG(st.st_mode)) out->bytes += (uint64_t)(st.st_size > 0 ? st.st_size : 0);
        return 0;
    }
    ++out->directories;
    int fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { ++out->errors; return 0; }
    DIR *dir = fdopendir(fd); if (!dir) { close(fd); ++out->errors; return 0; }
    int result = 0;
    for (;;) {
        errno = 0; struct dirent *item = readdir(dir);
        if (!item) { if (errno) ++out->errors; break; }
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (measure_at(fd, item->d_name, out, depth + 1, cancel, ctx) < 0) { result = -1; break; }
    }
    int e = errno; closedir(dir); errno = e; return result;
}
int usage_measure(const char *path, Usage *out, ProgressFn cancel, void *ctx) {
    memset(out, 0, sizeof(*out));
    return measure_at(AT_FDCWD, path, out, 0, cancel, ctx);
}
static int canceled(uint64_t bytes, uint64_t files, void *ctx) { (void)bytes; (void)files; return atomic_load(&((UsageScan *)ctx)->cancel); }
static int largest(const void *a, const void *b) {
    const UsageItem *x = a, *y = b;
    if (x->usage.allocated != y->usage.allocated) return x->usage.allocated > y->usage.allocated ? -1 : 1;
    return strcmp(x->name, y->name);
}
static void *scan_worker(void *ctx) {
    UsageScan *scan = ctx; Listing listing = {0};
    if (fs_list(&listing, scan->path, true, 0) < 0) { scan->error = errno; atomic_store(&scan->done, true); return NULL; }
    UsageItem *items = calloc(listing.count ? listing.count : 1, sizeof(*items));
    if (!items) { fs_free(&listing); scan->error = ENOMEM; atomic_store(&scan->done, true); return NULL; }
    for (size_t i = 0; i < listing.count; ++i) {
        snprintf(items[i].name, sizeof(items[i].name), "%s", listing.entries[i].name);
        items[i].directory = S_ISDIR(listing.entries[i].st.st_mode);
    }
    pthread_mutex_lock(&scan->mutex); scan->items = items; scan->count = listing.count; pthread_mutex_unlock(&scan->mutex);
    for (size_t i = 0; i < listing.count && !atomic_load(&scan->cancel); ++i) {
        char path[PATH_MAX]; Usage result = {0};
        if (fs_join(path, scan->path, listing.entries[i].name) < 0) ++result.errors;
        else if (usage_measure(path, &result, canceled, scan) < 0) break;
        pthread_mutex_lock(&scan->mutex);
        items[i].usage = result; items[i].complete = true; ++scan->completed;
        scan->total.bytes += result.bytes; scan->total.allocated += result.allocated;
        scan->total.files += result.files; scan->total.directories += result.directories; scan->total.errors += result.errors;
        pthread_mutex_unlock(&scan->mutex);
    }
    pthread_mutex_lock(&scan->mutex);
    if (scan->count) qsort(items, scan->count, sizeof(*items), largest);
    pthread_mutex_unlock(&scan->mutex);
    fs_free(&listing); atomic_store(&scan->done, true); return NULL;
}
void usage_stop(UsageScan *scan) {
    if (scan->started) { atomic_store(&scan->cancel, true); pthread_join(scan->thread, NULL); scan->started = false; }
    free(scan->items); scan->items = NULL; scan->count = scan->completed = 0; scan->total = (Usage){0};
}
void usage_destroy(UsageScan *scan) { usage_stop(scan); if (scan->initialized) pthread_mutex_destroy(&scan->mutex); scan->initialized = false; }
int usage_start(UsageScan *scan, const char *path) {
    char real[PATH_MAX]; if (!realpath(path, real)) return -1;
    usage_stop(scan);
    if (!scan->initialized) { int e = pthread_mutex_init(&scan->mutex, NULL); if (e) { errno = e; return -1; } scan->initialized = true; }
    snprintf(scan->path, sizeof(scan->path), "%s", real); scan->error = 0;
    atomic_store(&scan->cancel, false); atomic_store(&scan->done, false);
    pthread_attr_t attr; int e = pthread_attr_init(&attr);
    if (!e) { e = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024); if (!e) e = pthread_create(&scan->thread, &attr, scan_worker, scan); pthread_attr_destroy(&attr); }
    if (e) { errno = e; return -1; } scan->started = true; return 0;
}
