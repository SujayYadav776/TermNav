#define _GNU_SOURCE
#include "app.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void paths_free(char **paths, size_t count) { for (size_t i = 0; i < count; ++i) free(paths[i]); free(paths); }
double app_job_seconds(const App *a) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (double)(now.tv_sec - a->job.begun.tv_sec) + (double)(now.tv_nsec - a->job.begun.tv_nsec) / 1000000000.0;
    return elapsed > 0 ? elapsed : 0;
}
int app_enqueue(App *a, char **sources, size_t count, const char *destination, JobKind kind) {
    if (!count || a->queue_count >= QUEUE_MAX) { errno = count ? ENOSPC : EINVAL; return -1; }
    PendingOp *op = &a->queue[a->queue_count++]; memset(op, 0, sizeof(*op));
    op->sources = sources; op->count = count; op->kind = kind; op->id = ++a->next_op;
    snprintf(op->destination, sizeof(op->destination), "%s", destination ? destination : "");
    snprintf(op->label, sizeof(op->label), "%s", fs_basename(sources[0]));
    app_message(a, false, "%s #%u queued · %zu items · t for operations", job_name(kind), op->id, count);
    return 0;
}
void app_operations_tick(App *a) {
    if (a->job.started && atomic_load(&a->job.done)) {
        OperationLog completed = {0};
        completed.id = a->active_op; completed.kind = a->job.kind; completed.error = a->job.error;
        snprintf(completed.label, sizeof(completed.label), "%s", a->active_label);
        snprintf(completed.failed, sizeof(completed.failed), "%.255s", a->job.failed);
        completed.bytes = atomic_load(&a->job.bytes); completed.files = atomic_load(&a->job.files); completed.seconds = app_job_seconds(a);
        job_collect(&a->job);
        if (a->operation_count == OP_LOG_MAX) { memmove(a->operations, a->operations + 1, (OP_LOG_MAX - 1) * sizeof(*a->operations)); --a->operation_count; }
        a->operations[a->operation_count++] = completed;
        app_reload(a, NULL);
        if (completed.error) app_message(a, true, "%s #%u: %s · completed changes kept", job_name(completed.kind), completed.id, strerror(completed.error));
        else app_message(a, false, "%s #%u complete · %llu items%s", job_name(completed.kind), completed.id, (unsigned long long)completed.files, completed.kind == JOB_TRASH ? " · u to undo" : "");
        if (a->panel == PANEL_TRASH) trash_list(&a->trash);
    }
    if (!a->job.started && a->queue_count) {
        PendingOp op = a->queue[0]; memmove(a->queue, a->queue + 1, (--a->queue_count) * sizeof(*a->queue));
        if (job_start_kind(&a->job, op.sources, op.count, op.destination, op.kind) < 0) {
            paths_free(op.sources, op.count); app_message(a, true, "Could not start %s: %s", job_name(op.kind), strerror(errno));
        } else { a->active_op = op.id; snprintf(a->active_label, sizeof(a->active_label), "%s", op.label); }
    }
}
void app_operations_finish(App *a) {
    for (size_t i = 0; i < a->queue_count; ++i) paths_free(a->queue[i].sources, a->queue[i].count);
    a->queue_count = 0; job_finish(&a->job);
}
void app_undo(App *a) {
    TrashList list = {0};
    if (trash_list(&list) < 0) { app_message(a, true, "Trash: %s", strerror(errno)); return; }
    if (!list.count) { app_message(a, false, "Nothing in trash to restore"); trash_free(&list); return; }
    uint64_t batch = list.entries[0].batch; size_t count = 0;
    for (size_t i = 0; i < list.count; ++i) if (list.entries[i].batch == batch) ++count;
    char **paths = calloc(count, sizeof(*paths));
    if (!paths) { trash_free(&list); app_message(a, true, "Insufficient memory"); return; }
    size_t n = 0;
    for (size_t i = 0; i < list.count; ++i) if (list.entries[i].batch == batch) {
        paths[n] = strdup(list.entries[i].record);
        if (!paths[n]) { paths_free(paths, n); trash_free(&list); app_message(a, true, "Insufficient memory"); return; } ++n;
    }
    if (app_enqueue(a, paths, count, NULL, JOB_RESTORE) < 0) { paths_free(paths, count); app_message(a, true, "Restore: %s", strerror(errno)); }
    trash_free(&list);
}
void app_trash_open(App *a) {
    if (trash_list(&a->trash) < 0) { app_message(a, true, "Trash: %s", strerror(errno)); return; }
    a->panel = PANEL_TRASH; a->panel_cursor = a->panel_scroll = 0;
}
void app_usage_open(App *a, const char *path) {
    if (usage_start(&a->usage, path) < 0) { app_message(a, true, "Disk scan: %s", strerror(errno)); return; }
    a->panel = PANEL_USAGE; a->panel_cursor = a->panel_scroll = 0;
}
void app_history_jump(App *a, size_t position) {
    if (position >= a->history.count || position == a->history.position) return;
    HistoryEntry target = a->history.entries[position];
    a->history_replay = true;
    int result = app_navigate(a, target.path, target.selected);
    a->history_replay = false;
    if (result < 0) return;
    a->history.position = position;
    snprintf(a->query, sizeof(a->query), "%s", target.filter); app_filter(a);
    a->cursor = target.cursor < a->visible_count ? target.cursor : a->visible_count ? a->visible_count - 1 : 0;
    for (size_t i = 0; i < a->visible_count; ++i) if (!strcmp(a->current.entries[a->visible[i]].name, target.selected)) { a->cursor = i; break; }
    a->scroll = target.scroll < a->visible_count ? target.scroll : 0; app_preview(a, true);
}
