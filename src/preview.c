#define _GNU_SOURCE
#include "preview.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

void preview_free(Preview *p) { free(p->data); free(p->line_offsets); fs_free(&p->directory); memset(p, 0, sizeof(*p)); }
size_t preview_rows(const Preview *p) {
    if (p->kind == PREVIEW_TEXT) return p->lines;
    if (p->kind == PREVIEW_DIRECTORY) return p->directory.count;
    if (p->kind == PREVIEW_BINARY) return (p->bytes + 15) / 16;
    return 0;
}
const char *preview_line(const Preview *p, size_t row, size_t *length) {
    *length = 0;
    if (p->kind != PREVIEW_TEXT || row >= p->lines || !p->line_offsets) return NULL;
    size_t start = p->line_offsets[row];
    size_t end = row + 1 < p->lines ? p->line_offsets[row + 1] : p->bytes;
    if (end > start && p->data[end - 1] == '\n') --end;
    if (end > start && p->data[end - 1] == '\r') --end;
    *length = end - start; return p->data + start;
}
static bool extension(const Preview *p, const char *ext) {
    const char *dot = strrchr(fs_basename(p->path), '.'); return dot && !strcasecmp(dot, ext);
}
const char *preview_format(const Preview *p) {
    if (p->format[0]) return p->format;
    if (p->kind == PREVIEW_DIRECTORY) return "DIRECTORY";
    if (p->kind == PREVIEW_BINARY) return "BINARY / HEX";
    if (p->kind == PREVIEW_LINK) return "SYMBOLIC LINK";
    if (p->kind != PREVIEW_TEXT) return "DETAILS";
    if (extension(p, ".c") || extension(p, ".h")) return "C";
    if (extension(p, ".cpp") || extension(p, ".hpp") || extension(p, ".cc")) return "C++";
    if (extension(p, ".py")) return "PYTHON";
    if (extension(p, ".json")) return "JSON";
    if (extension(p, ".md") || extension(p, ".markdown")) return "MARKDOWN";
    if (extension(p, ".js") || extension(p, ".mjs") || extension(p, ".ts") || extension(p, ".tsx") || extension(p, ".jsx")) return "JAVASCRIPT / TYPESCRIPT";
    if (extension(p, ".rs")) return "RUST";
    if (extension(p, ".go")) return "GO";
    if (extension(p, ".sh")) return "SHELL";
    return "TEXT";
}
int preview_index(Preview *p) {
    free(p->line_offsets); p->line_offsets = NULL;
    p->lines = 0;
    for (size_t i = 0; i < p->bytes; ++i) if (p->data[i] == '\n') ++p->lines;
    if (p->bytes && p->data[p->bytes - 1] != '\n') ++p->lines;
    p->line_offsets = malloc((p->lines ? p->lines : 1) * sizeof(*p->line_offsets));
    if (!p->line_offsets) return -1;
    size_t row = 0;
    if (p->lines) p->line_offsets[row++] = 0;
    for (size_t i = 0; i < p->bytes && row < p->lines; ++i) if (p->data[i] == '\n') p->line_offsets[row++] = i + 1;
    return 0;
}
static PreviewSyntax syntax(const Preview *p) {
    if (extension(p, ".json")) return SYNTAX_JSON;
    if (extension(p, ".md") || extension(p, ".markdown")) return SYNTAX_MARKDOWN;
    if (extension(p, ".py") || extension(p, ".sh") || extension(p, ".yaml") || extension(p, ".yml") || extension(p, ".toml") || (p->bytes >= 2 && !memcmp(p->data, "#!", 2))) return SYNTAX_HASH;
    if (strcmp(preview_format(p), "TEXT")) return SYNTAX_CODE;
    return SYNTAX_PLAIN;
}
static void failure(Preview *p) { p->kind = PREVIEW_ERROR; snprintf(p->message, sizeof(p->message), "%s", strerror(errno)); }
void preview_load(Preview *p, const char *path, const Entry *entry, bool hidden) {
    preview_free(p); snprintf(p->path, sizeof(p->path), "%s", path);
    if (entry->symlink) {
        ssize_t n = readlink(path, p->target, sizeof(p->target) - 1);
        if (n < 0) { failure(p); return; } p->target[n] = 0;
        p->kind = PREVIEW_LINK; snprintf(p->message, sizeof(p->message), "Symbolic link"); return;
    }
    if (entry->directory) {
        if (fs_list(&p->directory, path, hidden, 0) < 0) { failure(p); return; }
        p->kind = PREVIEW_DIRECTORY; return;
    }
    if (!S_ISREG(entry->st.st_mode)) {
        p->kind = PREVIEW_SPECIAL; snprintf(p->message, sizeof(p->message), "Device, socket or named pipe"); return;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) { failure(p); return; }
    struct stat actual;
    if (fstat(fd, &actual) < 0 || !S_ISREG(actual.st_mode)) { close(fd); errno = EINVAL; failure(p); return; }
    p->data = malloc(PREVIEW_BYTES + 1);
    if (!p->data) { close(fd); errno = ENOMEM; failure(p); return; }
    while (p->bytes < PREVIEW_BYTES) {
        ssize_t n = read(fd, p->data + p->bytes, PREVIEW_BYTES - p->bytes);
        if (n < 0) { if (errno == EINTR) continue; int e = errno; close(fd); errno = e; failure(p); return; }
        if (!n) break;
        p->bytes += (size_t)n;
    }
    close(fd); p->data[p->bytes] = 0;
    p->truncated = actual.st_size > (off_t)p->bytes;
    size_t control = 0;
    for (size_t i = 0; i < p->bytes; ++i) {
        unsigned char c = (unsigned char)p->data[i];
        if (c == 0) { p->kind = PREVIEW_BINARY; return; }
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') ++control;
    }
    p->kind = control > p->bytes / 100 ? PREVIEW_BINARY : PREVIEW_TEXT;
    if (p->kind == PREVIEW_TEXT) {
        p->syntax = syntax(p);
        if (preview_index(p) < 0) failure(p);
    }
}
