#define _GNU_SOURCE
#include "app.h"
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void stop(int signal_number) { (void)signal_number; stopping = 1; }
void app_message(App *a, bool error, const char *fmt, ...) {
    va_list args; va_start(args, fmt); vsnprintf(a->message, sizeof(a->message), fmt, args); va_end(args);
    a->error = error; a->message_until = time(NULL) + 6;
}
Entry *app_selected(App *a) {
    return a->visible_count && a->cursor < a->visible_count ? &a->current.entries[a->visible[a->cursor]] : NULL;
}
void app_filter(App *a) {
    size_t n = 0;
    for (size_t i = 0; i < a->current.count; ++i) if (fs_match(a->current.entries[i].name, a->query)) a->visible[n++] = i;
    a->visible_count = n; if (a->cursor >= n) a->cursor = n ? n - 1 : 0;
    if (a->scroll > a->cursor) a->scroll = a->cursor;
}
static int load_current(App *a, const char *path, const char *keep) {
    Listing next = {0};
    if (fs_list(&next, path, a->hidden, a->sort) < 0) return -1;
    size_t *v = malloc((next.count ? next.count : 1) * sizeof(*v));
    if (!v) { fs_free(&next); errno = ENOMEM; return -1; }
    /* Preserve marks across refreshes by name and inode. */
    bool same = !strcmp(a->current.path, next.path);
    if (same) for (size_t i = 0; i < a->current.count; ++i) if (a->current.entries[i].marked)
        for (size_t j = 0; j < next.count; ++j)
            if (next.entries[j].st.st_ino == a->current.entries[i].st.st_ino && !strcmp(next.entries[j].name, a->current.entries[i].name)) { next.entries[j].marked = true; break; }
    fs_free(&a->current); a->current = next; free(a->visible); a->visible = v;
    app_filter(a);
    if (keep) for (size_t i = 0; i < a->visible_count; ++i) if (!strcmp(a->current.entries[a->visible[i]].name, keep)) { a->cursor = i; break; }
    char parent[PATH_MAX]; fs_parent(parent, a->current.path);
    if (fs_list(&a->parent, parent, a->hidden, a->sort) < 0) fs_free(&a->parent);
    app_preview(a, true); a->last_refresh = time(NULL); return 0;
}
int app_reload(App *a, const char *keep) {
    char name[NAME_MAX + 1], path[PATH_MAX];
    Entry *e = app_selected(a);
    snprintf(name, sizeof(name), "%s", keep ? keep : e ? e->name : "");
    snprintf(path, sizeof(path), "%s", a->current.path);
    if (load_current(a, path, name) < 0) { app_message(a, true, "Refresh: %s", strerror(errno)); return -1; } return 0;
}
int app_navigate(App *a, const char *path, const char *keep) {
    if (a->history.count) {
        HistoryEntry *saved = &a->history.entries[a->history.position]; Entry *selected = app_selected(a);
        snprintf(saved->selected, sizeof(saved->selected), "%s", selected ? selected->name : "");
        snprintf(saved->filter, sizeof(saved->filter), "%s", a->query); saved->cursor = a->cursor; saved->scroll = a->scroll;
    }
    char query[sizeof(a->query)]; strcpy(query, a->query);
    size_t cursor = a->cursor, scroll = a->scroll;
    a->query[0] = 0; a->cursor = a->scroll = a->preview_scroll = 0;
    if (load_current(a, path, keep) < 0) {
        strcpy(a->query, query); a->cursor = cursor; a->scroll = scroll;
        app_message(a, true, "Cannot open directory: %s", strerror(errno)); return -1;
    }
    if (!a->history_replay) history_push(&a->history, a->current.path);
    return 0;
}
void app_preview(App *a, bool force) {
    Entry *e = app_selected(a); char path[PATH_MAX];
    if (!e) { rich_stop(&a->rich); preview_free(&a->preview); a->preview_scroll = a->preview_column = 0; return; }
    if (fs_join(path, a->current.path, e->name) < 0) return;
    bool same = !strcmp(path, a->preview.path);
    WINDOW *image_window=a->preview_full ? a->viewer : a->right;
    unsigned image_width=128, image_height=128;
    if (image_window) {
        image_width=(unsigned)(getmaxx(image_window)-6); image_height=(unsigned)(getmaxy(image_window)>7 ? getmaxy(image_window)-7 : 1);
        if (a->sixel) { image_width*=a->cell_width; image_height*=a->cell_height; }
        else { if (a->ascii) image_width/=2; image_height*=a->ascii ? 1 : 2; }
    }
    if (image_width>1280) image_width=1280;
    if (image_height>960) image_height=960;
    bool image=rich_mode(e) && !strcmp(rich_mode(e),"image");
    if (rich_mode(e) && same && !strcmp(path, a->rich.path) && (!image || (a->rich.width==image_width && a->rich.height==image_height && a->rich.sixel==a->sixel)) && e->st.st_size == a->rich.signature.st_size && e->st.st_mtim.tv_sec == a->rich.signature.st_mtim.tv_sec && e->st.st_mtim.tv_nsec == a->rich.signature.st_mtim.tv_nsec && e->st.st_ino == a->rich.signature.st_ino && e->st.st_dev == a->rich.signature.st_dev) return;
    if (!force && same) return;
    if (!same) a->preview_scroll = a->preview_column = 0;
    rich_stop(&a->rich);
    if (rich_mode(e) && !rich_start(&a->rich,path,e,image_width,image_height,a->sixel)) {
        preview_free(&a->preview); snprintf(a->preview.path,sizeof(a->preview.path),"%s",path); a->preview.kind=PREVIEW_LOADING;
        snprintf(a->preview.message,sizeof(a->preview.message),"Preparing rich preview..."); return;
    }
    preview_load(&a->preview, path, e, a->hidden);
    size_t rows = preview_rows(&a->preview);
    if (a->preview_scroll >= rows) a->preview_scroll = rows ? rows - 1 : 0;
}
void app_config(App *a) {
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME"); char path[PATH_MAX];
    if (xdg && xdg[0]) snprintf(path, sizeof(path), "%s/termnav/config", xdg);
    else if (home) snprintf(path, sizeof(path), "%s/.config/termnav/config", home); else return;
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[256], key[64], value[64];
    while (fgets(line, sizeof(line), f)) if (sscanf(line, " %63[^= \t] = %63s", key, value) == 2) {
        if (!strcmp(key, "show_hidden")) a->hidden = !strcmp(value, "true");
        else if (!strcmp(key, "ascii")) a->ascii = !strcmp(value, "true");
        else if (!strcmp(key, "sort")) a->sort = !strcmp(value, "size") ? 1 : !strcmp(value, "modified") ? 2 : 0;
    }
    fclose(f);
}
static void usage(void) {
    puts("TermNav " TERMNAV_VERSION " — a quiet place for your files\n\nUsage: termnav [options] [directory]\n\n  -a, --all       Show hidden files\n  --ascii         Plain ASCII symbols\n  --no-color      Disable color\n  --home          Start on the Home dashboard\n  -h, --help      Show this help\n  -v, --version   Show version\n\nIn the app: ? for keys, q to quit. Linux + ncursesw.");
}
int main(int argc, char **argv) {
    setlocale(LC_ALL, ""); App a = {0}; app_config(&a);
    a.no_color = getenv("NO_COLOR") != NULL;
    const char *path = "."; bool directory_given = false, start_home = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) { puts("termnav " TERMNAV_VERSION); return 0; }
        if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--all")) a.hidden = true;
        else if (!strcmp(argv[i], "--ascii")) a.ascii = true;
        else if (!strcmp(argv[i], "--no-color")) a.no_color = true;
        else if (!strcmp(argv[i], "--home")) start_home = true;
        else if (!strcmp(argv[i], "--")) { if (++i < argc) { if (directory_given) { usage(); return 2; } path = argv[i]; directory_given = true; } if (i + 1 < argc) { usage(); return 2; } break; }
        else if (argv[i][0] == '-') { fprintf(stderr, "termnav: unknown option: %s\n", argv[i]); return 2; }
        else { if (directory_given) { usage(); return 2; } path = argv[i]; directory_given = true; }
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fputs("termnav: interactive terminal required (use --help for usage)\n", stderr); return 2; }
    if (app_navigate(&a, path, NULL) < 0) { fprintf(stderr, "termnav: %s\n", a.message); return 1; }
    if (start_home || !directory_given) a.panel = PANEL_HOME;
    struct sigaction sa = {0}; sa.sa_handler = stop; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    if (ui_init(&a) < 0) { rich_stop(&a.rich); fs_free(&a.current); fs_free(&a.parent); preview_free(&a.preview); free(a.visible); return 1; }
    app_message(&a, false, "Welcome. Press ? to find your way around.");
    while (!a.quit && !stopping) {
        if (a.panel == PANEL_HOME && time(NULL) - a.drives_refreshed >= 5) {
            if (drives_read(&a.drives) < 0) app_message(&a, true, "Cannot read mounted drives");
            a.drives_refreshed = time(NULL);
            if (a.home_drive >= a.drives.count) a.home_drive = 0;
        }
        if (rich_collect(&a.rich,&a.preview)) ++a.image_revision;
        app_operations_tick(&a);
        /* Periodic refresh notices changes made by other programs without losing selection. */
        if (a.mode == NORMAL && !a.help && !a.preview_full && (a.panel == PANEL_NONE || a.panel == PANEL_HOME) && !a.job.started && time(NULL) - a.last_refresh >= 2) app_reload(&a, NULL);
        ui_render(&a); wint_t key; int r = get_wch(&key);
        if (r != ERR) app_input(&a, key, r == KEY_CODE_YES);
        ++a.ticks;
    }
    app_operations_finish(&a); rich_stop(&a.rich); usage_destroy(&a.usage); search_destroy(&a.search); trash_free(&a.trash); ui_shutdown(&a);
    fs_free(&a.current); fs_free(&a.parent); preview_free(&a.preview); free(a.visible);
    fs_paths_free(a.clipboard, a.clipboard_count);
    return 0;
}
