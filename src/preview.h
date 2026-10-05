#ifndef TERMNAV_PREVIEW_H
#define TERMNAV_PREVIEW_H
#include "fs_ops.h"
#define PREVIEW_BYTES (64 * 1024)
#define IMAGE_PREVIEW_BYTES (8 * 1024 * 1024)
typedef enum { PREVIEW_EMPTY, PREVIEW_TEXT, PREVIEW_DIRECTORY, PREVIEW_BINARY, PREVIEW_SPECIAL, PREVIEW_ERROR, PREVIEW_LINK, PREVIEW_IMAGE, PREVIEW_LOADING } PreviewKind;
typedef enum { SYNTAX_PLAIN, SYNTAX_CODE, SYNTAX_HASH, SYNTAX_JSON, SYNTAX_MARKDOWN } PreviewSyntax;
typedef struct {
    char path[PATH_MAX], message[256], target[PATH_MAX];
    PreviewKind kind;
    char *data;
    size_t bytes, lines;
    size_t *line_offsets;
    PreviewSyntax syntax;
    bool truncated;
    char format[32];
    unsigned image_width, image_height, original_width, original_height;
    bool image_sixel;
    Listing directory;
} Preview;
void preview_load(Preview *p, const char *path, const Entry *entry, bool hidden);
void preview_free(Preview *p);
size_t preview_rows(const Preview *p);
const char *preview_line(const Preview *p, size_t row, size_t *length);
const char *preview_format(const Preview *p);
int preview_index(Preview *p);
#endif
