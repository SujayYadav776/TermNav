#define _GNU_SOURCE
#include "app.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static void free_paths(char **paths, size_t n) { for (size_t i = 0; i < n; ++i) free(paths[i]); free(paths); }
static char **targets(App *a, size_t *count) {
    *count = 0;
    for (size_t i = 0; i < a->current.count; ++i) if (a->current.entries[i].marked) ++*count;
    if (!*count && app_selected(a)) *count = 1;
    if (!*count) return NULL;
    char **paths = calloc(*count, sizeof(*paths)); if (!paths) return NULL;
    size_t n = 0; bool marked = false;
    for (size_t i = 0; i < a->current.count; ++i) if (a->current.entries[i].marked) {
        char path[PATH_MAX]; marked = true;
        if (fs_join(path, a->current.path, a->current.entries[i].name) < 0 || !(paths[n] = strdup(path))) { free_paths(paths, n); return NULL; } ++n;
    }
    if (!marked) {
        char path[PATH_MAX];
        if (fs_join(path, a->current.path, app_selected(a)->name) < 0 || !(paths[0] = strdup(path))) { free(paths); return NULL; }
    }
    return paths;
}
static void clipboard(App *a) {
    size_t n; char **paths = targets(a, &n);
    if (!paths) { app_message(a, true, "No selection or insufficient memory"); return; }
    free_paths(a->clipboard, a->clipboard_count); a->clipboard = paths; a->clipboard_count = n;
    app_message(a, false, "Copied %zu %s to clipboard · p to paste", n, n == 1 ? "path" : "paths");
}
static void start_copy(App *a, const char *destination, bool from_clipboard) {
    char resolved[PATH_MAX];
    if (!realpath(destination, resolved)) { app_message(a, true, "Destination: %s", strerror(errno)); return; }
    struct stat st;
    if (stat(resolved, &st) < 0 || !S_ISDIR(st.st_mode)) { app_message(a, true, "Choose an existing destination directory"); return; }
    char **paths; size_t count;
    if (from_clipboard) {
        count = a->clipboard_count;
        if (!count) { app_message(a, true, "Clipboard is empty · select files and press y"); return; }
        paths = calloc(count, sizeof(*paths)); if (!paths) { app_message(a, true, "Insufficient memory"); return; }
        for (size_t i = 0; i < count; ++i) if (!(paths[i] = strdup(a->clipboard[i]))) { free_paths(paths, i); app_message(a, true, "Insufficient memory"); return; }
    } else paths = targets(a, &count);
    if (!paths) { app_message(a, true, "No selection or insufficient memory"); return; }
    if (app_enqueue(a, paths, count, resolved, JOB_COPY) < 0) { free_paths(paths, count); app_message(a, true, "Copy: %s", strerror(errno)); }
}
static void prompt(App *a, InputMode mode, const char *initial) {
    a->mode = mode; snprintf(a->input, sizeof(a->input), "%s", initial ? initial : "");
}
static void submit(App *a) {
    InputMode mode = a->mode; a->mode = NORMAL; Entry *e = app_selected(a);
    if (mode == FILTER) { return; }
    if (mode == DELETE_INPUT || mode == TRASH_INPUT) {
        if (strcmp(a->input, mode == TRASH_INPUT ? "trash" : "delete")) { app_message(a, false, "Operation cancelled"); return; }
        size_t count; char **paths = targets(a, &count);
        if (!paths) { app_message(a, true, "No selection or insufficient memory"); return; }
        if (app_enqueue(a, paths, count, NULL, mode == TRASH_INPUT ? JOB_TRASH : JOB_DELETE) < 0) { free_paths(paths, count); app_message(a, true, "Operation: %s", strerror(errno)); }
        return;
    }
    if (mode == QUIT_INPUT) {
        if (!strcmp(a->input, "q")) { a->quit = true; atomic_store(&a->job.cancel, true); }
        return;
    }
    if (!a->input[0]) return;
    if (mode == MKDIR_INPUT) {
        if (fs_mkdir(a->current.path, a->input) < 0) app_message(a, true, "Create directory: %s", strerror(errno));
        else { app_reload(a, a->input); app_message(a, false, "Directory created"); }
    } else if (mode == RENAME_INPUT && e) {
        if (fs_rename(a->current.path, e->name, a->input) < 0) app_message(a, true, "Rename: %s", strerror(errno));
        else { app_reload(a, a->input); app_message(a, false, "Renamed"); }
    } else if (mode == GOTO_INPUT || mode == COPY_INPUT) {
        char path[PATH_MAX];
        if (a->input[0] == '~' && (!a->input[1] || a->input[1] == '/')) {
            const char *home = getenv("HOME");
            int n = snprintf(path, sizeof(path), "%s%s", home ? home : "/", a->input + 1);
            if (n < 0 || n >= PATH_MAX) { app_message(a, true, "Path too long"); return; }
        } else if (a->input[0] == '/') snprintf(path, sizeof(path), "%s", a->input);
        else if (fs_join(path, a->current.path, a->input) < 0) { app_message(a, true, "Path too long"); return; }
        if (mode == GOTO_INPUT) app_navigate(a, path, NULL); else start_copy(a, path, false);
    }
}
static void backspace(char *text) {
    size_t n = strlen(text); if (!n) return;
    --n; while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n; text[n] = 0;
}
static void viewer_input(App *a, wint_t key, bool special) {
    bool g = a->pending_g; a->pending_g = false;
    size_t rows = preview_rows(&a->preview);
    size_t page = LINES > 12 ? (size_t)(LINES - 12) : 1;
    if (key == 27 || key == 'q' || key == '\t') { a->preview_full = false; a->preview_column = 0; return; }
    if (key == 'j' || key == 'J' || (special && key == KEY_DOWN)) {
        if (a->preview_scroll + 1 < rows) ++a->preview_scroll;
    } else if (key == 'k' || key == 'K' || (special && key == KEY_UP)) {
        if (a->preview_scroll) --a->preview_scroll;
    } else if (key == 4 || key == ' ' || (special && key == KEY_NPAGE)) {
        size_t next = a->preview_scroll + page;
        a->preview_scroll = rows ? (next < rows ? next : rows - 1) : 0;
    } else if (key == 21 || (special && key == KEY_PPAGE)) {
        a->preview_scroll = a->preview_scroll > page ? a->preview_scroll - page : 0;
    } else if (key == 'g') {
        if (g) a->preview_scroll = 0; else a->pending_g = true;
    } else if (special && key == KEY_HOME) a->preview_scroll = 0;
    else if (key == 'G' || (special && key == KEY_END)) a->preview_scroll = rows ? rows - 1 : 0;
    else if (key == 'h' || (special && key == KEY_LEFT)) a->preview_column = a->preview_column > 4 ? a->preview_column - 4 : 0;
    else if (key == 'l' || (special && key == KEY_RIGHT)) {
        if (a->preview_column < PREVIEW_BYTES * 4) a->preview_column += 4;
    } else if (key == '0') a->preview_column = 0;
    else if (key == 'R' || key == 12) { rich_stop(&a->rich); a->rich.path[0]=0; app_reload(a, NULL); }
}
static void palette_input(App *a, wint_t key, bool special) {
    size_t matches[64], count = command_matches(a->input, matches, 64);
    if (key == 27) { a->mode = NORMAL; return; }
    if (special && key == KEY_UP) { if (a->palette_cursor) --a->palette_cursor; return; }
    if (special && key == KEY_DOWN) { if (a->palette_cursor + 1 < count) ++a->palette_cursor; return; }
    if (key == '\n' || key == '\r' || (special && key == KEY_ENTER)) {
        if (!count) return;
        size_t index = a->palette_cursor < count ? a->palette_cursor : 0;
        int action = commands[matches[index]].key; a->mode = NORMAL;
        if (a->panel == PANEL_USAGE) usage_stop(&a->usage);
        a->panel = PANEL_NONE; a->preview_full = false; a->help = false;
        app_input(a, (wint_t)action, false); return;
    }
    if (key == 127 || key == 8 || (special && key == KEY_BACKSPACE)) backspace(a->input);
    else if (key == 21) a->input[0] = 0;
    else if (!special && iswprint(key)) {
        char encoded[MB_LEN_MAX]; mbstate_t state = {0}; size_t n = wcrtomb(encoded, (wchar_t)key, &state), len = strlen(a->input);
        if (n != (size_t)-1 && len + n < sizeof(a->input)) { memcpy(a->input + len, encoded, n); a->input[len + n] = 0; }
    }
    a->palette_cursor = 0;
}
static void panel_input(App *a, wint_t key, bool special) {
    if (key == 27 || key == 'q' || (a->panel == PANEL_OPERATIONS && key == 't') || (a->panel == PANEL_TRASH && key == 'T') || (a->panel == PANEL_HISTORY && key == 'H') || (a->panel == PANEL_USAGE && key == 'D')) {
        if (a->panel == PANEL_USAGE) usage_stop(&a->usage);
        a->panel = PANEL_NONE; return;
    }
    size_t count = a->panel == PANEL_HISTORY ? a->history.count : a->panel == PANEL_TRASH ? a->trash.count : a->queue_count + a->operation_count + (a->job.started ? 1 : 0);
    if (a->panel == PANEL_USAGE) { pthread_mutex_lock(&a->usage.mutex); count = a->usage.count; pthread_mutex_unlock(&a->usage.mutex); }
    if (a->panel_cursor >= count) a->panel_cursor = count ? count - 1 : 0;
    if (key == 'j' || (special && key == KEY_DOWN)) { if (a->panel_cursor + 1 < count) ++a->panel_cursor; }
    else if (key == 'k' || (special && key == KEY_UP)) { if (a->panel_cursor) --a->panel_cursor; }
    else if (key == 'G') a->panel_cursor = count ? count - 1 : 0;
    else if (key == 'g') a->panel_cursor = 0;
    else if (a->panel == PANEL_OPERATIONS) {
        if (a->job.started && !a->panel_cursor && key == ' ') atomic_store(&a->job.paused, !atomic_load(&a->job.paused));
        else if (key == 'x' || key == 'c') {
            if (a->job.started && !a->panel_cursor) atomic_store(&a->job.cancel, true);
            else {
                size_t queued = a->panel_cursor - (a->job.started ? 1 : 0);
                if (queued < a->queue_count) {
                    free_paths(a->queue[queued].sources, a->queue[queued].count);
                    memmove(a->queue + queued, a->queue + queued + 1, (a->queue_count - queued - 1) * sizeof(*a->queue)); --a->queue_count;
                    app_message(a, false, "Queued operation cancelled");
                }
            }
        }
    } else if (a->panel == PANEL_HISTORY && (key == '\n' || (special && key == KEY_ENTER))) { size_t selected = a->panel_cursor; a->panel = PANEL_NONE; app_history_jump(a, selected); }
    else if (a->panel == PANEL_TRASH) {
        if ((key == '\n' || (special && key == KEY_ENTER)) && count) {
            char **paths = calloc(1, sizeof(*paths));
            if (!paths || !(paths[0] = strdup(a->trash.entries[a->panel_cursor].record))) { free(paths); app_message(a, true, "Insufficient memory"); return; }
            if (app_enqueue(a, paths, 1, NULL, JOB_RESTORE) < 0) { free_paths(paths, 1); app_message(a, true, "Restore: %s", strerror(errno)); }
        } else if (key == 'u') app_undo(a);
        else if (key == 'R') trash_list(&a->trash);
    } else if (a->panel == PANEL_USAGE) {
        if (key == 'h' || key == 127 || (special && key == KEY_LEFT)) { char parent[PATH_MAX]; fs_parent(parent, a->usage.path); app_usage_open(a, parent); }
        else if (key == 'R') { char path[PATH_MAX]; snprintf(path, sizeof(path), "%s", a->usage.path); app_usage_open(a, path); }
        else if (count && (key == '\n' || key == 'l' || key == 'o' || (special && key == KEY_ENTER))) {
            pthread_mutex_lock(&a->usage.mutex); UsageItem item = a->usage.items[a->panel_cursor]; pthread_mutex_unlock(&a->usage.mutex);
            char path[PATH_MAX]; if (fs_join(path, a->usage.path, item.name) < 0) return;
            if (key == 'o') {
                char parent[PATH_MAX]; snprintf(parent, sizeof(parent), "%s", a->usage.path);
                usage_stop(&a->usage); a->panel = PANEL_NONE; app_navigate(a, item.directory ? path : parent, item.directory ? NULL : item.name);
            } else if (item.directory) app_usage_open(a, path);
        }
    }
}
void app_input(App *a, wint_t key, bool special) {
    if (special && key == KEY_RESIZE) return;
    if (a->mode == PALETTE_INPUT) { palette_input(a, key, special); return; }
    if (a->mode == NORMAL && key == ':') { prompt(a, PALETTE_INPUT, ""); a->palette_cursor = 0; return; }
    if (a->panel != PANEL_NONE) { panel_input(a, key, special); return; }
    if (a->preview_full) { viewer_input(a, key, special); return; }
    if (a->help) { if (key == '?' || key == 27 || key == 'q' || key == '\n') a->help = false; return; }
    if (a->mode != NORMAL) {
        char *text = a->mode == FILTER ? a->query : a->input;
        size_t capacity = a->mode == FILTER ? sizeof(a->query) : sizeof(a->input);
        if (key == 27) {
            if (a->mode == FILTER) { a->query[0] = 0; app_filter(a); app_preview(a, false); }
            a->mode = NORMAL; return;
        }
        if (key == '\n' || key == '\r' || (special && key == KEY_ENTER)) { submit(a); return; }
        if (key == 21) text[0] = 0;
        else if (key == 127 || key == 8 || (special && key == KEY_BACKSPACE)) backspace(text);
        else if (!special && iswprint(key)) {
            char encoded[MB_LEN_MAX]; mbstate_t state = {0};
            size_t n = wcrtomb(encoded, (wchar_t)key, &state), len = strlen(text);
            if (n != (size_t)-1 && len + n < capacity) { memcpy(text + len, encoded, n); text[len + n] = 0; }
        }
        if (a->mode == FILTER) { a->cursor = a->scroll = 0; app_filter(a); app_preview(a, false); }
        return;
    }
    bool g = a->pending_g; a->pending_g = false;
    Entry *e = app_selected(a);
    int page = a->center ? getmaxy(a->center) - 5 : 10; if (page < 1) page = 1;
    if (key == '\t') { if (e) a->preview_full = true; }
    else if (key == 'q') { if (a->job.started || a->queue_count) prompt(a, QUIT_INPUT, ""); else a->quit = true; }
    else if (key == '?') a->help = true;
    else if (key == 27) {
        if (a->job.started) { atomic_store(&a->job.cancel, true); app_message(a, false, "Cancellation requested · completed work is kept"); }
        else { a->query[0] = 0; for (size_t i = 0; i < a->current.count; ++i) a->current.entries[i].marked = false; app_filter(a); }
    }
    else if (key == 'j' || (special && key == KEY_DOWN)) { if (a->cursor + 1 < a->visible_count) ++a->cursor; }
    else if (key == 'k' || (special && key == KEY_UP)) { if (a->cursor) --a->cursor; }
    else if (key == 4 || (special && key == KEY_NPAGE)) { size_t next = a->cursor + (size_t)page; a->cursor = a->visible_count ? (next < a->visible_count ? next : a->visible_count - 1) : 0; }
    else if (key == 21 || (special && key == KEY_PPAGE)) a->cursor = a->cursor > (size_t)page ? a->cursor - (size_t)page : 0;
    else if (key == 'g') { if (g) a->cursor = 0; else a->pending_g = true; }
    else if (special && key == KEY_HOME) a->cursor = 0;
    else if (key == 'G' || (special && key == KEY_END)) a->cursor = a->visible_count ? a->visible_count - 1 : 0;
    else if (key == 'h' || key == 127 || key == 8 || (special && (key == KEY_LEFT || key == KEY_BACKSPACE))) {
        char parent[PATH_MAX], keep[NAME_MAX + 1]; fs_parent(parent, a->current.path); snprintf(keep, sizeof(keep), "%s", fs_basename(a->current.path)); app_navigate(a, parent, keep);
    }
    else if (key == 'l' || key == '\n' || (special && (key == KEY_RIGHT || key == KEY_ENTER))) {
        if (e && e->directory) { char path[PATH_MAX]; if (!fs_join(path, a->current.path, e->name)) app_navigate(a, path, NULL); }
        else if (e) a->preview_full = true;
    }
    else if (key == 'J') { if (a->preview_scroll + 1 < preview_rows(&a->preview)) ++a->preview_scroll; }
    else if (key == 'K') { if (a->preview_scroll) --a->preview_scroll; }
    else if (key == '/') { a->query[0] = 0; a->cursor = a->scroll = 0; app_filter(a); prompt(a, FILTER, ""); }
    else if (key == '.') { a->hidden = !a->hidden; app_reload(a, NULL); }
    else if (key == 's') { a->sort = (a->sort + 1) % 3; app_reload(a, NULL); app_message(a, false, "Sorted by %s", a->sort == 1 ? "size" : a->sort == 2 ? "modified" : "name"); }
    else if (key == 'R' || key == 12) { rich_stop(&a->rich); a->rich.path[0]=0; app_reload(a, NULL); }
    else if (key == ' ' && e) { e->marked = !e->marked; if (a->cursor + 1 < a->visible_count) ++a->cursor; }
    else if (key == 'v') { bool all = true; for (size_t i = 0; i < a->visible_count; ++i) if (!a->current.entries[a->visible[i]].marked) all = false; for (size_t i = 0; i < a->visible_count; ++i) a->current.entries[a->visible[i]].marked = !all; }
    else if (key == 'y') clipboard(a);
    else if (key == 'u') app_undo(a);
    else if (key == 'T') app_trash_open(a);
    else if (key == 't') { a->panel = PANEL_OPERATIONS; a->panel_cursor = a->panel_scroll = 0; }
    else if (key == 'D') app_usage_open(a, a->current.path);
    else if (key == 'H') { a->panel = PANEL_HISTORY; a->panel_cursor = a->history.position; a->panel_scroll = 0; }
    else if (key == 'b') { if (a->history.position) app_history_jump(a, a->history.position - 1); }
    else if (key == 'f') { if (a->history.position + 1 < a->history.count) app_history_jump(a, a->history.position + 1); }
    else if (key == '~') { const char *home = getenv("HOME"); if (home) app_navigate(a, home, NULL); }
    else if (key == 'o') prompt(a, GOTO_INPUT, "");
    else if (key == 'a' || key == 'r' || key == 'd' || key == 'X' || key == 'c' || key == 'p' || key == 'e') {
        if (a->job.started && (key == 'a' || key == 'r' || key == 'e')) { app_message(a, true, "Wait for the current job, or Esc to cancel it"); return; }
        if (key == 'a') prompt(a, MKDIR_INPUT, "");
        else if (key == 'r' && e) prompt(a, RENAME_INPUT, e->name);
        else if (key == 'd' && e) prompt(a, TRASH_INPUT, "");
        else if (key == 'X' && e) prompt(a, DELETE_INPUT, "");
        else if (key == 'c' && e) prompt(a, COPY_INPUT, "");
        else if (key == 'p') start_copy(a, a->current.path, true);
        /* Editor integration is handled by an argv-based child, never a shell command. */
        else if (key == 'e' && e && !e->directory) {
            char path[PATH_MAX]; if (fs_join(path, a->current.path, e->name) < 0) return;
            const char *editor = getenv("EDITOR"); if (!editor || !editor[0]) editor = "vi";
            if (strpbrk(editor, " \t")) { app_message(a, true, "EDITOR must be an executable name/path without arguments"); return; }
            def_prog_mode(); endwin();
            int result = app_edit(editor, path);
            reset_prog_mode(); refresh(); curs_set(0); app_reload(a, NULL);
            if (result) app_message(a, true, "Editor exited with status %d", result);
        }
    }
    app_preview(a, false);
}
