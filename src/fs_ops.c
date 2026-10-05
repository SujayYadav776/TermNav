#define _GNU_SOURCE
#include "fs_ops.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <wchar.h>

int fs_join(char out[PATH_MAX], const char *dir, const char *name) {
    int n = snprintf(out, PATH_MAX, "%s%s%s", dir, strcmp(dir, "/") ? "/" : "", name);
    if (n < 0 || n >= PATH_MAX) { errno = ENAMETOOLONG; return -1; }
    return 0;
}
const char *fs_basename(const char *path) {
    const char *p = strrchr(path, '/');
    return p && p[1] ? p + 1 : path;
}
void fs_parent(char out[PATH_MAX], const char *path) {
    snprintf(out, PATH_MAX, "%s", path);
    char *p = strrchr(out, '/');
    if (!p || p == out) strcpy(out, "/"); else *p = 0;
}
void fs_free(Listing *l) {
    for (size_t i = 0; i < l->count; ++i) free(l->entries[i].name);
    free(l->entries); memset(l, 0, sizeof(*l));
}
static int by_name(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->directory != y->directory) return x->directory ? -1 : 1;
    int r = strcasecmp(x->name, y->name);
    return r ? r : strcmp(x->name, y->name);
}
static int by_size(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->directory != y->directory) return x->directory ? -1 : 1;
    if (x->st.st_size != y->st.st_size) return x->st.st_size > y->st.st_size ? -1 : 1;
    return by_name(a, b);
}
static int by_time(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->directory != y->directory) return x->directory ? -1 : 1;
    if (x->st.st_mtime != y->st.st_mtime) return x->st.st_mtime > y->st.st_mtime ? -1 : 1;
    return by_name(a, b);
}
int fs_list(Listing *out, const char *path, bool hidden, int sort) {
    Listing next = {0}; size_t cap = 0;
    if (!realpath(path, next.path)) return -1;
    DIR *d = opendir(next.path); if (!d) return -1;
    int fd = dirfd(d), saved = 0;
    for (;;) {
        errno = 0; struct dirent *e = readdir(d);
        if (!e) { saved = errno; break; }
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (!hidden && e->d_name[0] == '.') continue;
        struct stat st;
        if (fstatat(fd, e->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) continue;
        if (next.count == cap) {
            cap = cap ? cap * 2 : 128;
            Entry *v = realloc(next.entries, cap * sizeof(*v));
            if (!v) { saved = ENOMEM; break; } next.entries = v;
        }
        Entry *item = &next.entries[next.count]; memset(item, 0, sizeof(*item));
        item->name = strdup(e->d_name); if (!item->name) { saved = ENOMEM; break; }
        item->st = st; item->symlink = S_ISLNK(st.st_mode); item->directory = S_ISDIR(st.st_mode);
        if (item->symlink && fstatat(fd, e->d_name, &st, 0) == 0) item->directory = S_ISDIR(st.st_mode);
        ++next.count;
    }
    closedir(d);
    if (saved) { fs_free(&next); errno = saved; return -1; }
    if (next.count) qsort(next.entries, next.count, sizeof(Entry), sort == 1 ? by_size : sort == 2 ? by_time : by_name);
    fs_free(out); *out = next; return 0;
}
static bool valid_name(const char *name) {
    return name[0] && strcmp(name, ".") && strcmp(name, "..") && !strchr(name, '/');
}
int fs_mkdir(const char *dir, const char *name) {
    if (!valid_name(name)) { errno = EINVAL; return -1; }
    int fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (fd < 0) return -1;
    int r = mkdirat(fd, name, 0777), e = errno; close(fd); errno = e; return r;
}
int fs_rename(const char *dir, const char *old_name, const char *new_name) {
    if (!valid_name(old_name) || !valid_name(new_name)) { errno = EINVAL; return -1; }
    if (!strcmp(old_name, new_name)) return 0;
    int fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (fd < 0) return -1;
    /* Atomic no-replace rename: never silently replace an existing entry. */
    int r = (int)syscall(SYS_renameat2, fd, old_name, fd, new_name, 1u), e = errno;
    close(fd); errno = e; return r;
}
static int progress(ProgressFn fn, void *ctx, uint64_t bytes, uint64_t files) {
    if (fn && fn(bytes, files, ctx)) { errno = ECANCELED; return -1; } return 0;
}
static int copy_at(int sfd, const char *src, int dfd, const char *dst, int depth, ProgressFn fn, void *ctx) {
    if (depth > 128) { errno = ELOOP; return -1; }
    if (progress(fn, ctx, 0, 0) < 0) return -1;
    struct stat st; if (fstatat(sfd, src, &st, AT_SYMLINK_NOFOLLOW) < 0) return -1;
    struct stat existing;
    if (fstatat(dfd, dst, &existing, AT_SYMLINK_NOFOLLOW) == 0) { errno = EEXIST; return -1; }
    if (errno != ENOENT) return -1;
    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX]; ssize_t n = readlinkat(sfd, src, target, sizeof(target) - 1);
        if (n < 0) return -1;
        if (n == (ssize_t)sizeof(target) - 1) { errno = ENAMETOOLONG; return -1; }
        target[n] = 0; if (symlinkat(target, dfd, dst) < 0) return -1;
        return progress(fn, ctx, 0, 1);
    }
    if (S_ISDIR(st.st_mode)) {
        int in = openat(sfd, src, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (in < 0) return -1;
        DIR *d = fdopendir(in); if (!d) { int e = errno; close(in); errno = e; return -1; }
        if (mkdirat(dfd, dst, 0700) < 0) { int e = errno; closedir(d); errno = e; return -1; }
        int out = openat(dfd, dst, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (out < 0) { int e = errno; closedir(d); errno = e; return -1; }
        int r = 0, e = 0;
        for (;;) {
            errno = 0; struct dirent *item = readdir(d);
            if (!item) { if (errno) { r = -1; e = errno; } break; }
            if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
            if (copy_at(in, item->d_name, out, item->d_name, depth + 1, fn, ctx) < 0) { r = -1; e = errno; break; }
        }
        struct timespec times[2] = {st.st_atim, st.st_mtim};
        if (!r && (fchmod(out, st.st_mode & 0777) < 0 || futimens(out, times) < 0 || fsync(out) < 0)) { r = -1; e = errno; }
        close(out); closedir(d);
        if (r) { errno = e; return -1; }
        return progress(fn, ctx, 0, 1);
    }
    if (!S_ISREG(st.st_mode)) { errno = ENOTSUP; return -1; }
    int in = openat(sfd, src, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC); if (in < 0) return -1;
    struct stat actual;
    if (fstat(in, &actual) < 0 || !S_ISREG(actual.st_mode)) { close(in); errno = EINVAL; return -1; }
    /* Exclusive temporary file, then link into place atomically without replacement. */
    char tmp[96]; int out = -1;
    for (unsigned i = 0; i < 100; ++i) {
        snprintf(tmp, sizeof(tmp), ".termnav-copy-%ld-%u", (long)getpid(), i);
        out = openat(dfd, tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (out >= 0 || errno != EEXIST) break;
    }
    if (out < 0) { int e = errno; close(in); errno = e; return -1; }
    char *buffer = malloc(128 * 1024); int r = 0, saved = 0;
    if (!buffer) { close(out); close(in); unlinkat(dfd, tmp, 0); errno = ENOMEM; return -1; }
    for (;;) {
        ssize_t n = read(in, buffer, 128 * 1024);
        if (n < 0) { if (errno == EINTR) continue; r = -1; saved = errno; break; }
        if (!n) break;
        ssize_t used = 0;
        while (used < n) {
            ssize_t w = write(out, buffer + used, (size_t)(n - used));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) { r = -1; saved = w ? errno : EIO; break; } used += w;
        }
        if (r) break;
        if (progress(fn, ctx, (uint64_t)n, 0) < 0) { r = -1; saved = errno; break; }
    }
    free(buffer);
    struct timespec times[2] = {actual.st_atim, actual.st_mtim};
    if (!r && (fchmod(out, actual.st_mode & 0777) < 0 || futimens(out, times) < 0 || fsync(out) < 0)) { r = -1; saved = errno; }
    if (close(out) < 0 && !r) { r = -1; saved = errno; } close(in);
    if (!r && linkat(dfd, tmp, dfd, dst, 0) < 0) { r = -1; saved = errno; }
    unlinkat(dfd, tmp, 0);
    if (!r && fsync(dfd) < 0) { r = -1; saved = errno; }
    if (r) { errno = saved; return -1; }
    return progress(fn, ctx, 0, 1);
}
int fs_copy(const char *src, const char *dst, ProgressFn fn, void *ctx) {
    char sp[PATH_MAX], dp[PATH_MAX], real_src[PATH_MAX], real_dst[PATH_MAX], final[PATH_MAX];
    if (src[0] != '/' || dst[0] != '/') { errno = EINVAL; return -1; }
    fs_parent(sp, src); fs_parent(dp, dst);
    if (!valid_name(fs_basename(src)) || !valid_name(fs_basename(dst))) { errno = EINVAL; return -1; }
    char source_parent[PATH_MAX];
    if (!realpath(sp, source_parent) || !realpath(dp, real_dst)) return -1;
    struct stat st;
    if (lstat(src, &st) < 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (!realpath(src, real_src) || fs_join(final, real_dst, fs_basename(dst)) < 0) return -1;
        size_t n = strlen(real_src);
        if (!strcmp(real_src, final) || (!strncmp(real_src, final, n) && final[n] == '/')) { errno = EINVAL; return -1; }
    }
    int in = open(source_parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (in < 0) return -1;
    int out = open(real_dst, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (out < 0) { int e = errno; close(in); errno = e; return -1; }
    int r = copy_at(in, fs_basename(src), out, fs_basename(dst), 0, fn, ctx), e = errno;
    close(in); close(out); errno = e; return r;
}
static int delete_at(int parent, const char *name, int depth, ProgressFn fn, void *ctx) {
    if (depth > 128) { errno = ELOOP; return -1; }
    if (progress(fn, ctx, 0, 0) < 0) return -1;
    struct stat st; if (fstatat(parent, name, &st, AT_SYMLINK_NOFOLLOW) < 0) return -1;
    if (!S_ISDIR(st.st_mode)) {
        if (unlinkat(parent, name, 0) < 0) return -1;
        return progress(fn, ctx, 0, 1);
    }
    int fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (fd < 0) return -1;
    DIR *d = fdopendir(fd); if (!d) { int e = errno; close(fd); errno = e; return -1; }
    int r = 0, e = 0;
    for (;;) {
        errno = 0; struct dirent *item = readdir(d);
        if (!item) { if (errno) { r = -1; e = errno; } break; }
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (delete_at(fd, item->d_name, depth + 1, fn, ctx) < 0) { r = -1; e = errno; break; }
    }
    closedir(d);
    if (r) { errno = e; return -1; }
    if (unlinkat(parent, name, AT_REMOVEDIR) < 0) return -1;
    return progress(fn, ctx, 0, 1);
}
int fs_delete(const char *path, ProgressFn fn, void *ctx) {
    char parent[PATH_MAX]; const char *name = fs_basename(path);
    if (path[0] != '/' || !strcmp(path, "/") || !valid_name(name)) { errno = EINVAL; return -1; }
    fs_parent(parent, path);
    int fd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (fd < 0) return -1;
    int r = delete_at(fd, name, 0, fn, ctx), e = errno; close(fd); errno = e; return r;
}
static wchar_t next_character(const char **text) {
    mbstate_t state = {0}; wchar_t ch;
    size_t n = mbrtowc(&ch, *text, MB_CUR_MAX, &state);
    if (n == (size_t)-1 || n == (size_t)-2) { ch = (unsigned char)**text; n = 1; }
    if (n) *text += n;
    return ch < 128 ? (wchar_t)tolower((unsigned char)ch) : ch;
}
bool fs_match(const char *name, const char *query) {
    /* Match whole characters; do not match UTF-8 fragments across filenames. */
    if (!*query) return true;
    wchar_t wanted = next_character(&query);
    while (*name) if (next_character(&name) == wanted) {
        if (!*query) return true;
        wanted = next_character(&query);
    }
    return false;
}
void fs_size(char *out, size_t cap, off_t size) {
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double n = (double)size; size_t u = 0;
    while (n >= 1024 && u < 5) { n /= 1024; ++u; }
    if (!u) snprintf(out, cap, "%lld B", (long long)size); else snprintf(out, cap, "%.1f %s", n, units[u]);
}
void fs_permissions(char out[11], mode_t mode) {
    strcpy(out, "----------");
    out[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : S_ISCHR(mode) ? 'c' : S_ISBLK(mode) ? 'b' : '-';
    const mode_t bits[] = {S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};
    for (int i = 0; i < 9; ++i) if (mode & bits[i]) out[i + 1] = "rwx"[i % 3];
    if (mode & S_ISUID) out[3] = mode & S_IXUSR ? 's' : 'S';
    if (mode & S_ISGID) out[6] = mode & S_IXGRP ? 's' : 'S';
    if (mode & S_ISVTX) out[9] = mode & S_IXOTH ? 't' : 'T';
}
