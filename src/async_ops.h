#ifndef TERMNAV_ASYNC_H
#define TERMNAV_ASYNC_H
#include "fs_ops.h"
#include <pthread.h>
#include <stdatomic.h>
typedef enum { JOB_COPY, JOB_DELETE, JOB_TRASH, JOB_RESTORE } JobKind;
typedef struct {
    pthread_t thread;
    atomic_bool cancel, done, paused, planning;
    atomic_uint_fast64_t bytes, files;
    atomic_uint_fast64_t total_bytes, total_files;
    char **sources;
    size_t count;
    char destination[PATH_MAX];
    bool started;
    JobKind kind;
    uint64_t batch;
    struct timespec begun;
    int error;
    char failed[PATH_MAX];
} Job;
int job_start(Job *job, char **sources, size_t count, const char *destination, bool deleting);
int job_start_kind(Job *job, char **sources, size_t count, const char *destination, JobKind kind);
const char *job_name(JobKind kind);
bool job_collect(Job *job);
void job_finish(Job *job);
#endif
