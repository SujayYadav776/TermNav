#define _GNU_SOURCE
#include "app.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void put(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0); assert(write(fd, "test", 4) == 4); assert(!close(fd));
}
static void await_job(App *a) {
    for (unsigned i = 0; !atomic_load(&a->job.done); ++i) { assert(i < 10000); usleep(1000); }
    app_operations_tick(a);
}
int main(void) {
    char root[] = "/tmp/termnav-app-XXXXXX"; assert(mkdtemp(root));
    char first[PATH_MAX], second[PATH_MAX], data[PATH_MAX];
    assert(!fs_join(first, root, "a.txt")); assert(!fs_join(second, root, "b.txt"));
    assert(!fs_join(data, root, "data")); assert(!setenv("XDG_DATA_HOME", data, 1));
    put(first); put(second);
    App *a = calloc(1, sizeof(*a)); assert(a); assert(!app_navigate(a, root, NULL));
    assert(!strcmp(app_selected(a)->name, "a.txt"));
    char **paths = calloc(1, sizeof(*paths)); assert(paths); paths[0] = strdup(first); assert(paths[0]);
    assert(!app_enqueue(a, paths, 1, NULL, JOB_DELETE));
    /* The selected entry disappears while its next confirmation is open. */
    a->mode = TRASH_INPUT; strcpy(a->input, "trash");
    app_operations_tick(a); await_job(a);
    assert(a->reload_pending && !strcmp(app_selected(a)->name, "a.txt"));
    app_input(a, '\n', false); app_operations_tick(a); await_job(a);
    assert(a->operations[a->operation_count - 1].error == ENOENT);
    assert(!access(second, F_OK)); /* The confirmation must not target b.txt. */
    /* A job can start between opening a rename prompt and submitting it. */
    a->job.started = true; a->mode = RENAME_INPUT; strcpy(a->input, "renamed.txt");
    app_input(a, '\n', false); assert(!access(second, F_OK)); assert(a->error);
    a->job.started = false;
    for (Panel panel = PANEL_NONE; panel <= PANEL_HOME; ++panel) {
        a->panel = panel; a->mode = NORMAL; a->help = true; a->preview_full = true;
        app_input(a, KEY_F(1), true);
        assert(a->panel == PANEL_HOME && !a->help && !a->preview_full);
    }
    a->panel = PANEL_NONE; a->mode = RENAME_INPUT;
    app_input(a, KEY_F(1), true);
    assert(a->panel == PANEL_NONE && a->mode == RENAME_INPUT);
    a->mode = NORMAL;
    char nested[PATH_MAX]; assert(!fs_join(nested, root, "nested"));
    assert(!fs_mkdir(root, "nested")); assert(!app_reload(a, "nested"));
    a->panel = PANEL_HOME; a->home_focus = 1;
    app_input(a, KEY_RIGHT, true);
    assert(a->panel == PANEL_NONE && !strcmp(a->current.path, nested));
    a->panel = PANEL_HOME;
    app_input(a, KEY_LEFT, true);
    assert(a->panel == PANEL_NONE && !strcmp(a->current.path, root));
    a->panel = PANEL_HOME; a->mode = GOTO_INPUT; strcpy(a->input, nested);
    app_input(a, '\n', false);
    assert(a->panel == PANEL_NONE && !strcmp(a->current.path, nested));
    assert(!drives_read(&a->drives) && a->drives.count);
    for (size_t i = 0; i < a->drives.count; ++i) {
        assert(a->drives.items[i].total > 0 && a->drives.items[i].available <= a->drives.items[i].total);
        assert(!access(a->drives.items[i].path, F_OK));
    }
    app_operations_finish(a); rich_stop(&a->rich); usage_destroy(&a->usage); trash_free(&a->trash);
    fs_free(&a->current); fs_free(&a->parent); preview_free(&a->preview); free(a->visible); free(a);
    assert(!fs_delete(root, NULL, NULL));
    puts("PASS: confirmation targets, active-job rename protection and Home directory navigation");
    return 0;
}
