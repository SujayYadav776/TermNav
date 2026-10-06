#ifndef TERMNAV_APP_H
#define TERMNAV_APP_H
#include "async_ops.h"
#include "preview.h"
#include "rich_preview.h"
#include "trash.h"
#include "usage.h"
#include "history.h"
#include "commands.h"
#include "drives.h"
#include <curses.h>
#include <time.h>
#define TERMNAV_VERSION "1.1.0"
#define QUEUE_MAX 16
#define OP_LOG_MAX 32
typedef enum { NORMAL, FILTER, MKDIR_INPUT, RENAME_INPUT, GOTO_INPUT, COPY_INPUT, DELETE_INPUT, QUIT_INPUT, TRASH_INPUT, PALETTE_INPUT } InputMode;
typedef enum { PANEL_NONE, PANEL_OPERATIONS, PANEL_HISTORY, PANEL_USAGE, PANEL_TRASH, PANEL_HOME } Panel;
typedef struct { char **sources; size_t count; char destination[PATH_MAX], label[NAME_MAX + 1]; JobKind kind; unsigned id; } PendingOp;
typedef struct { char label[NAME_MAX + 1], failed[NAME_MAX + 1]; JobKind kind; unsigned id; int error; uint64_t bytes, files; double seconds; } OperationLog;
typedef struct {
    Listing current, parent;
    Preview preview;
    RichPreview rich;
    bool sixel, native_image_visible;
    unsigned cell_width, cell_height;
    uint64_t image_revision, image_shown_revision;
    Job job;
    PendingOp queue[QUEUE_MAX]; size_t queue_count; unsigned next_op, active_op;
    char active_label[NAME_MAX + 1];
    OperationLog operations[OP_LOG_MAX]; size_t operation_count;
    History history; bool history_replay;
    TrashList trash;
    UsageScan usage;
    Drives drives; time_t drives_refreshed;
    unsigned home_focus; size_t home_folder, home_drive;
    Panel panel, layout_panel; size_t panel_cursor, panel_scroll, palette_cursor;
    size_t *visible, visible_count, cursor, scroll, preview_scroll, preview_column;
    char query[256], input[PATH_MAX], message[512];
    char **clipboard;
    size_t clipboard_count;
    InputMode mode;
    bool hidden, ascii, no_color, help, quit, pending_g, error, preview_full, layout_preview, reload_pending;
    int sort;
    time_t message_until, last_refresh;
    WINDOW *left, *center, *right, *viewer;
    int layout_rows, layout_cols;
    unsigned ticks;
} App;
void app_message(App *a, bool error, const char *fmt, ...);
Entry *app_selected(App *a);
void app_filter(App *a);
int app_reload(App *a, const char *keep);
int app_navigate(App *a, const char *path, const char *keep);
void app_preview(App *a, bool force);
void app_input(App *a, wint_t key, bool special);
void app_config(App *a);
const char *app_quick_folder(size_t index);
int app_quick_path(char path[PATH_MAX], size_t index);
int app_enqueue(App *a, char **sources, size_t count, const char *destination, JobKind kind);
void app_operations_tick(App *a);
void app_operations_finish(App *a);
void app_history_jump(App *a, size_t position);
void app_undo(App *a);
void app_trash_open(App *a);
void app_usage_open(App *a, const char *path);
double app_job_seconds(const App *a);
int app_edit(const char *editor, const char *path);
int ui_init(App *a);
void ui_render(App *a);
void ui_shutdown(App *a);
#endif
