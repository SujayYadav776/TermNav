#include "async_ops.h"
#include "trash.h"
#include "usage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *job_name(JobKind kind) { return kind == JOB_DELETE ? "Delete" : kind == JOB_TRASH ? "Trash" : kind == JOB_RESTORE ? "Restore" : "Copy"; }
static int should_cancel(Job *job) {
    while (atomic_load(&job->paused) && !atomic_load(&job->cancel)) { struct timespec pause = {0, 20000000}; nanosleep(&pause, NULL); }
    return atomic_load(&job->cancel);
}
static int planning_update(uint64_t bytes, uint64_t files, void *ctx) { (void)bytes; (void)files; return should_cancel(ctx); }

static int update(uint64_t bytes, uint64_t files, void *ctx) {
    Job *job = ctx;
    atomic_fetch_add(&job->bytes, bytes); atomic_fetch_add(&job->files, files);
    return should_cancel(job);
}
static void *worker(void *ctx) {
    Job *job = ctx;
    atomic_store(&job->planning, true);
    for (size_t i = 0; i < job->count && !should_cancel(job); ++i) {
        if (job->kind == JOB_COPY || job->kind == JOB_DELETE) {
            Usage usage;
            if (usage_measure(job->sources[i], &usage, planning_update, job) < 0) break;
            atomic_fetch_add(&job->total_bytes, usage.bytes); atomic_fetch_add(&job->total_files, usage.files + usage.directories);
        } else atomic_fetch_add(&job->total_files, 1);
    }
    atomic_store(&job->planning, false);
    for (size_t i = 0; i < job->count; ++i) {
        char dst[PATH_MAX]; int r;
        if (should_cancel(job)) { job->error = ECANCELED; break; }
        if (job->kind == JOB_DELETE) r = fs_delete(job->sources[i], update, job);
        else if (job->kind == JOB_TRASH) { r = trash_put(job->sources[i], job->batch); if (!r) update(0, 1, job); }
        else if (job->kind == JOB_RESTORE) { r = trash_restore(job->sources[i], dst); if (!r) update(0, 1, job); }
        else {
            r = fs_join(dst, job->destination, fs_basename(job->sources[i]));
            if (!r) r = fs_copy(job->sources[i], dst, update, job);
        }
        if (r < 0) {
            job->error = errno; snprintf(job->failed, sizeof(job->failed), "%s", fs_basename(job->sources[i])); break;
        }
    }
    atomic_store(&job->done, true); return NULL;
}
int job_start(Job *job, char **sources, size_t count, const char *destination, bool deleting) {
    return job_start_kind(job, sources, count, destination, deleting ? JOB_DELETE : JOB_COPY);
}
int job_start_kind(Job *job, char **sources, size_t count, const char *destination, JobKind kind) {
    if (job->started || !count) { errno = EBUSY; return -1; }
    job->sources = sources; job->count = count; job->kind = kind; job->deleting = kind == JOB_DELETE;
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now); job->batch = (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
    clock_gettime(CLOCK_MONOTONIC, &job->begun);
    snprintf(job->destination, sizeof(job->destination), "%s", destination ? destination : "");
    job->error = 0; job->failed[0] = 0;
    atomic_store(&job->bytes, 0); atomic_store(&job->files, 0);
    atomic_store(&job->total_bytes, 0); atomic_store(&job->total_files, 0); atomic_store(&job->paused, false); atomic_store(&job->planning, false);
    atomic_store(&job->cancel, false); atomic_store(&job->done, false); atomic_store(&job->running, true);
    /* musl's default thread stack is small; bound recursion and reserve 2 MiB. */
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    if (!err) {
        err = pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
        if (!err) err = pthread_create(&job->thread, &attr, worker, job);
        pthread_attr_destroy(&attr);
    }
    if (err) { atomic_store(&job->running, false); job->sources = NULL; job->count = 0; errno = err; return -1; }
    job->started = true; return 0;
}
bool job_collect(Job *job) {
    if (!job->started || !atomic_load(&job->done)) return false;
    pthread_join(job->thread, NULL);
    for (size_t i = 0; i < job->count; ++i) free(job->sources[i]);
    free(job->sources); job->sources = NULL; job->count = 0;
    job->started = false; atomic_store(&job->running, false); return true;
}
void job_finish(Job *job) {
    if (!job->started) return;
    atomic_store(&job->cancel, true); pthread_join(job->thread, NULL);
    for (size_t i = 0; i < job->count; ++i) free(job->sources[i]);
    free(job->sources); job->sources = NULL; job->started = false;
}
