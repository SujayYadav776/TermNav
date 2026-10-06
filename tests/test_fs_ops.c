#define _GNU_SOURCE
#include "fs_ops.h"
#include "preview.h"
#include "async_ops.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s (errno=%d %s)\n", __FILE__, __LINE__, #expr, errno, strerror(errno)); exit(1); } } while (0)
static void join(char out[PATH_MAX], const char *root, const char *name) { CHECK(fs_join(out, root, name) == 0); }
static void put(const char *path, const void *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0640); CHECK(fd >= 0);
    const char *p = data; while (len) { ssize_t n = write(fd, p, len); CHECK(n > 0); p += n; len -= (size_t)n; } CHECK(close(fd) == 0);
}
static Entry *find(Listing *l, const char *name) { for (size_t i = 0; i < l->count; ++i) if (!strcmp(l->entries[i].name, name)) return &l->entries[i]; return NULL; }
static int cancel(uint64_t bytes, uint64_t files, void *ctx) { (void)files; uint64_t *sum = ctx; *sum += bytes; return *sum > 0; }
static int cancel_immediate(uint64_t bytes, uint64_t files, void *ctx) { (void)bytes; (void)files; (void)ctx; return 1; }
int main(void) {
    setlocale(LC_ALL, "C.UTF-8");
    char root[] = "/tmp/termnav-test-XXXXXX", guard[] = "/tmp/termnav-guard-XXXXXX";
    CHECK(mkdtemp(root)); CHECK(mkdtemp(guard));
    char text[PATH_MAX], binary[PATH_MAX], hidden[PATH_MAX], folder[PATH_MAX], link[PATH_MAX], dst[PATH_MAX], nested[PATH_MAX];
    join(text, root, "hello.txt"); put(text, "hello\nworld\n", 12);
    join(binary, root, "binary"); put(binary, "\x7f" "ELF\0\x01", 6);
    join(hidden, root, ".hidden"); put(hidden, "secret", 6);
    CHECK(fs_mkdir(root, "folder") == 0); join(folder, root, "folder");
    join(nested, folder, "inside.txt"); put(nested, "nested", 6);
    join(link, root, "link"); CHECK(symlink(guard, link) == 0);
    char protected[PATH_MAX]; join(protected, guard, "keep"); put(protected, "untouched", 9);
    Listing l = {0}; CHECK(fs_list(&l, root, false, 0) == 0); CHECK(l.count == 4);
    CHECK(l.entries[0].directory); CHECK(find(&l, "hello.txt")->st.st_size == 12);
    CHECK(find(&l, "link")->symlink && find(&l, "link")->directory);
    CHECK(fs_list(&l, root, true, 0) == 0); CHECK(l.count == 5); CHECK(find(&l, ".hidden"));
    CHECK(fs_list(&l, "/nonexistent-termnav", false, 0) == -1); CHECK(l.count == 5);
    CHECK(fs_match("Terminal_File_Manager", "tfm")); CHECK(fs_match("README.md", "read"));
    CHECK(fs_match("anything", "")); CHECK(!fs_match("cat", "cats"));
    CHECK(fs_match("日本語.txt", "日語")); CHECK(!fs_match("漢嗎入", "日"));
    char permissions[11]; fs_permissions(permissions, S_IFREG | 0640); CHECK(!strcmp(permissions, "-rw-r-----"));
    fs_permissions(permissions, S_IFREG | 04755); CHECK(!strcmp(permissions, "-rwsr-xr-x"));
    char size[32]; fs_size(size, sizeof(size), 1024); CHECK(!strcmp(size, "1.0 KiB"));
    CHECK(fs_mkdir(root, "../escape") == -1 && errno == EINVAL);
    CHECK(fs_mkdir(root, ".") == -1); CHECK(fs_mkdir(root, "") == -1);
    CHECK(fs_rename(root, "hello.txt", "binary") == -1 && errno == EEXIST);
    CHECK(fs_rename(root, "hello.txt", "hello.txt") == 0);
    CHECK(fs_rename(root, "hello.txt", "renamed.txt") == 0); join(text, root, "renamed.txt"); CHECK(access(text, F_OK) == 0);
    CHECK(fs_rename(root, "renamed.txt", "../escape") == -1);
    join(dst, root, "copy.txt"); CHECK(fs_copy(text, dst, NULL, NULL) == 0);
    int fd = open(dst, O_RDONLY); CHECK(fd >= 0); char data[16] = {0}; CHECK(read(fd, data, 16) == 12); close(fd); CHECK(!strcmp(data, "hello\nworld\n"));
    struct stat st; CHECK(stat(dst, &st) == 0 && (st.st_mode & 0777) == 0640);
    CHECK(fs_copy(binary, dst, NULL, NULL) == -1 && errno == EEXIST); CHECK(stat(dst, &st) == 0 && st.st_size == 12);
    char treecopy[PATH_MAX]; join(treecopy, root, "tree-copy"); CHECK(fs_copy(folder, treecopy, NULL, NULL) == 0);
    join(nested, treecopy, "inside.txt"); CHECK(access(nested, F_OK) == 0);
    char bad[PATH_MAX]; join(bad, folder, "descendant"); CHECK(fs_copy(folder, bad, NULL, NULL) == -1 && errno == EINVAL); CHECK(access(bad, F_OK) == -1);
    char parentlink[PATH_MAX]; join(parentlink, root, "alias"); CHECK(symlink(folder, parentlink) == 0);
    join(bad, parentlink, "bad"); CHECK(fs_copy(folder, bad, NULL, NULL) == -1 && errno == EINVAL);
    char linkcopy[PATH_MAX]; join(linkcopy, root, "copied-link"); CHECK(fs_copy(link, linkcopy, NULL, NULL) == 0);
    CHECK(lstat(linkcopy, &st) == 0 && S_ISLNK(st.st_mode));
    CHECK(fs_delete(linkcopy, NULL, NULL) == 0); CHECK(access(protected, F_OK) == 0);
    Preview p = {0}; CHECK(fs_list(&l, root, true, 0) == 0);
    preview_load(&p, text, find(&l, "renamed.txt"), true); CHECK(p.kind == PREVIEW_TEXT && p.lines == 2 && p.bytes == 12);
    size_t line_length;
    CHECK(preview_rows(&p) == 2);
    const char *line = preview_line(&p, 1, &line_length);
    CHECK(line && line_length == 5 && !memcmp(line, "world", 5));
    CHECK(preview_index(&p) == 0 && p.lines == 2);
    line = preview_line(&p, 1, &line_length);
    CHECK(line && line_length == 5 && !memcmp(line, "world", 5));
    CHECK(!preview_line(&p, 2, &line_length) && !line_length);
    preview_load(&p, binary, find(&l, "binary"), true); CHECK(p.kind == PREVIEW_BINARY);
    CHECK(preview_rows(&p) == 1 && !strcmp(preview_format(&p), "BINARY / HEX"));
    preview_load(&p, folder, find(&l, "folder"), true); CHECK(p.kind == PREVIEW_DIRECTORY && p.directory.count == 1);
    preview_load(&p, link, find(&l, "link"), true); CHECK(p.kind == PREVIEW_LINK && !strcmp(p.target, guard));
    if (geteuid() != 0) {
        CHECK(chmod(text, 0000) == 0);
        preview_load(&p, text, find(&l, "renamed.txt"), true);
        CHECK(p.kind == PREVIEW_ERROR && strstr(p.message, "Permission denied"));
        CHECK(chmod(text, 0640) == 0);
    }
    char fifo[PATH_MAX]; join(fifo, root, "fifo"); CHECK(mkfifo(fifo, 0600) == 0); CHECK(fs_list(&l, root, true, 0) == 0);
    preview_load(&p, fifo, find(&l, "fifo"), true); CHECK(p.kind == PREVIEW_SPECIAL);
    join(bad, root, "fifo-copy"); CHECK(fs_copy(fifo, bad, NULL, NULL) == -1 && errno == ENOTSUP);
    char big[PATH_MAX]; join(big, root, "large.txt");
    size_t len = 300000; char *large = malloc(len); CHECK(large); memset(large, 'a', len); put(big, large, len); free(large);
    CHECK(fs_list(&l, root, true, 0) == 0); preview_load(&p, big, find(&l, "large.txt"), true);
    CHECK(p.kind == PREVIEW_TEXT && p.bytes == PREVIEW_BYTES && p.truncated);
    CHECK(preview_line(&p, 0, &line_length) && line_length == PREVIEW_BYTES);
    char code[PATH_MAX]; join(code, root, "example.json"); put(code, "{\r\n  \"enabled\": true\r\n}\r\n", 25);
    CHECK(fs_list(&l, root, true, 0) == 0); preview_load(&p, code, find(&l, "example.json"), true);
    CHECK(p.kind == PREVIEW_TEXT && p.syntax == SYNTAX_JSON && !strcmp(preview_format(&p), "JSON"));
    line = preview_line(&p, 0, &line_length); CHECK(line && line_length == 1 && *line == '{');
    char blank[PATH_MAX]; join(blank, root, "blank.txt"); put(blank, "\n\nlast", 6);
    CHECK(fs_list(&l, root, true, 0) == 0); preview_load(&p, blank, find(&l, "blank.txt"), true);
    CHECK(p.lines == 3 && preview_rows(&p) == 3);
    line = preview_line(&p, 0, &line_length); CHECK(line && line_length == 0);
    line = preview_line(&p, 2, &line_length); CHECK(line && line_length == 4 && !memcmp(line, "last", 4));
    join(bad, root, "cancelled"); uint64_t copied = 0;
    CHECK(fs_copy(big, bad, cancel, &copied) == -1 && errno == ECANCELED); CHECK(access(bad, F_OK) == -1);
    CHECK(fs_delete(folder, cancel_immediate, NULL) == -1 && errno == ECANCELED); CHECK(access(folder, F_OK) == 0);
    CHECK(fs_delete("/", NULL, NULL) == -1 && errno == EINVAL); CHECK(fs_delete(".", NULL, NULL) == -1);
    /* Dynamic listings support more than the PRD's original 1024 entry cap. */
    char many[PATH_MAX]; CHECK(fs_mkdir(root, "many") == 0); join(many, root, "many");
    for (int i = 0; i < 1200; ++i) { char name[32]; snprintf(name, sizeof(name), "%04d", i); join(bad, many, name); put(bad, "x", 1); }
    CHECK(fs_list(&l, many, false, 0) == 0 && l.count == 1200);
    CHECK(fs_list(&l, root, true, 0) == 0);
    for (size_t i = 0; i < l.count; ++i) CHECK(strncmp(l.entries[i].name, ".termnav-copy-", 14));
    char asyncdir[PATH_MAX]; CHECK(fs_mkdir(root, "async") == 0); join(asyncdir, root, "async");
    Job job = {0}; char **sources = calloc(1, sizeof(*sources)); CHECK(sources); sources[0] = strdup(big); CHECK(sources[0]);
    CHECK(job_start(&job, sources, 1, asyncdir, false) == 0);
    while (!job_collect(&job)) usleep(1000);
    CHECK(job.error == 0 && atomic_load(&job.bytes) == len && atomic_load(&job.files) == 1);
    join(bad, asyncdir, "large.txt"); CHECK(stat(bad, &st) == 0 && st.st_size == (off_t)len);
    preview_free(&p); fs_free(&l);
    CHECK(fs_delete(root, NULL, NULL) == 0); CHECK(access(protected, F_OK) == 0); CHECK(fs_delete(guard, NULL, NULL) == 0);
    printf("PASS: %d filesystem, preview and worker checks\n", checks); return 0;
}
