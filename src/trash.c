#define _GNU_SOURCE
#include "trash.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

typedef struct { char magic[8]; uint64_t batch, when; uint32_t original_len, stored_len; } Record;
static atomic_uint serial;
static int mkdirs(const char *path) {
    char copy[PATH_MAX]; snprintf(copy, sizeof(copy), "%s", path);
    for (char *p = copy + 1; *p; ++p) if (*p == '/') { *p = 0; if (mkdir(copy, 0700) < 0 && errno != EEXIST) return -1; *p = '/'; }
    if (mkdir(copy, 0700) < 0 && errno != EEXIST) return -1;
    return 0;
}
static int private_dir(const char *path, bool create) {
    if (create && mkdir(path, 0700) < 0 && errno != EEXIST) return -1;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_uid != geteuid() || (st.st_mode & 0077)) { close(fd); errno = EACCES; return -1; }
    return fd;
}
static int registry(char root[PATH_MAX], bool create) {
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    int n;
    if (xdg && xdg[0] == '/') n = snprintf(root, PATH_MAX, "%s/termnav/trash", xdg);
    else if (home && home[0] == '/') n = snprintf(root, PATH_MAX, "%s/.local/share/termnav/trash", home);
    else { errno = EINVAL; return -1; }
    if (n < 0 || n >= PATH_MAX) { errno = ENAMETOOLONG; return -1; }
    if (create && mkdirs(root) < 0) return -1;
    int fd = private_dir(root, false); char canonical[PATH_MAX];
    if (fd < 0) return -1;
    if (!realpath(root, canonical)) { int e = errno; close(fd); errno = e; return -1; }
    strcpy(root, canonical); return fd;
}
static int lock_registry(int fd) {
    int lock = openat(fd, ".lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lock < 0) return -1;
    struct stat st;
    if (fstat(lock, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() || flock(lock, LOCK_EX) < 0) { int e = errno ? errno : EACCES; close(lock); errno = e; return -1; }
    return lock;
}
static int transfer(int fd, void *data, size_t n, bool writing) {
    char *p = data;
    while (n) {
        ssize_t done = writing ? write(fd, p, n) : read(fd, p, n);
        if (done < 0 && errno == EINTR) continue;
        if (done <= 0) { if (!done) errno = EINVAL; return -1; }
        p += done; n -= (size_t)done;
    }
    return 0;
}
static int bin_path(char bin[PATH_MAX], const char *original) {
    char parent[PATH_MAX], name[64]; fs_parent(parent, original);
    snprintf(name, sizeof(name), ".termnav-trash-%lu", (unsigned long)geteuid());
    return fs_join(bin, parent, name);
}
static int read_record(int dir, const char *name, TrashEntry *entry) {
    int fd = openat(dir, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC); if (fd < 0) return -1;
    struct stat st; Record header; int r = 0;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() || st.st_size > 2 * PATH_MAX + 64) { errno = EINVAL; r = -1; }
    if (!r && transfer(fd, &header, sizeof(header), false) < 0) r = -1;
    if (!r && (memcmp(header.magic, "TNTRASH1", 8) || !header.original_len || header.original_len >= PATH_MAX || !header.stored_len || header.stored_len >= PATH_MAX)) { errno = EINVAL; r = -1; }
    if (!r && (transfer(fd, entry->original, header.original_len, false) < 0 || transfer(fd, entry->stored, header.stored_len, false) < 0)) r = -1;
    int e = errno; close(fd); errno = e; if (r) return -1;
    entry->original[header.original_len] = entry->stored[header.stored_len] = 0;
    if (strlen(entry->original) != header.original_len || strlen(entry->stored) != header.stored_len || entry->original[0] != '/') { errno = EINVAL; return -1; }
    char expected_bin[PATH_MAX], expected[PATH_MAX];
    if (bin_path(expected_bin, entry->original) < 0 || fs_join(expected, expected_bin, name) < 0) return -1;
    if (strcmp(expected, entry->stored)) { errno = EINVAL; return -1; }
    entry->batch = header.batch; entry->when = header.when; return 0;
}
int trash_put(const char *path, uint64_t batch) {
    if (path[0] != '/' || !strcmp(path, "/") || strstr(path, "/.termnav-trash-")) { errno = EINVAL; return -1; }
    const char *name = fs_basename(path);
    if (strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) { errno = EINVAL; return -1; }
    char parent[PATH_MAX], canonical[PATH_MAX], original[PATH_MAX], bin[PATH_MAX], root[PATH_MAX], stored[PATH_MAX];
    fs_parent(parent, path);
    if (!realpath(parent, canonical) || fs_join(original, canonical, name) < 0 || bin_path(bin, original) < 0) return -1;
    int src = open(canonical, O_RDONLY | O_DIRECTORY | O_CLOEXEC); if (src < 0) return -1;
    int dest = private_dir(bin, true); if (dest < 0) { int e = errno; close(src); errno = e; return -1; }
    int reg = registry(root, true); if (reg < 0) { int e = errno; close(src); close(dest); errno = e; return -1; }
    /* Moving the registry or an ancestor would make its records unreachable. */
    struct stat st; size_t length = strlen(original);
    if (fstatat(src, name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode) &&
        !strncmp(root, original, length) && (!root[length] || root[length] == '/')) {
        close(src); close(dest); close(reg); errno = EINVAL; return -1;
    }
    int lock = lock_registry(reg); if (lock < 0) { int e = errno; close(src); close(dest); close(reg); errno = e; return -1; }
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    char id[96]; snprintf(id, sizeof(id), "%lld-%09ld-%ld-%u", (long long)now.tv_sec, now.tv_nsec, (long)getpid(), atomic_fetch_add(&serial, 1));
    int r = fs_join(stored, bin, id), e = errno;
    int metadata = r ? -1 : openat(reg, id, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (metadata < 0) { r = -1; e = errno; }
    if (!r) {
        Record header = { .magic = {'T','N','T','R','A','S','H','1'}, .batch = batch, .when = (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec, .original_len = (uint32_t)strlen(original), .stored_len = (uint32_t)strlen(stored) };
        if (transfer(metadata, &header, sizeof(header), true) < 0 || transfer(metadata, original, header.original_len, true) < 0 || transfer(metadata, stored, header.stored_len, true) < 0 || fsync(metadata) < 0 || fsync(reg) < 0) { r = -1; e = errno; }
    }
    if (metadata >= 0) close(metadata);
    if (!r && syscall(SYS_renameat2, src, name, dest, id, 1u) < 0) { r = -1; e = errno; }
    if (r) { if (metadata >= 0) unlinkat(reg, id, 0); }
    else { fsync(src); fsync(dest); }
    close(lock); close(reg); close(dest); close(src); errno = e; return r;
}
int trash_restore(const char *record, char original[PATH_MAX]) {
    char root[PATH_MAX], parent[PATH_MAX], bin[PATH_MAX];
    int reg = registry(root, false); if (reg < 0) return -1;
    fs_parent(parent, record);
    if (strcmp(root, parent)) { close(reg); errno = EINVAL; return -1; }
    int lock = lock_registry(reg); if (lock < 0) { int e = errno; close(reg); errno = e; return -1; }
    TrashEntry entry = {0}; const char *id = fs_basename(record);
    int r = read_record(reg, id, &entry), e = errno;
    int src = -1, dest = -1;
    if (!r) {
        fs_parent(bin, entry.stored); fs_parent(parent, entry.original);
        src = private_dir(bin, false); dest = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (src < 0 || dest < 0) { r = -1; e = errno; }
    }
    if (!r && syscall(SYS_renameat2, src, id, dest, fs_basename(entry.original), 1u) < 0) { r = -1; e = errno; }
    if (!r) {
        snprintf(original, PATH_MAX, "%s", entry.original);
        unlinkat(reg, id, 0); fsync(src); fsync(dest); fsync(reg);
    }
    if (src >= 0) close(src);
    if (dest >= 0) close(dest);
    close(lock); close(reg); errno = e; return r;
}
void trash_free(TrashList *list) { free(list->entries); memset(list, 0, sizeof(*list)); }
static int newest(const void *a, const void *b) { const TrashEntry *x = a, *y = b; return x->when > y->when ? -1 : x->when < y->when ? 1 : 0; }
int trash_list(TrashList *list) {
    char root[PATH_MAX]; int reg = registry(root, false);
    if (reg < 0) { if (errno == ENOENT) { trash_free(list); return 0; } return -1; }
    int lock = lock_registry(reg); if (lock < 0) { int e = errno; close(reg); errno = e; return -1; }
    DIR *dir = fdopendir(reg); if (!dir) { int e = errno; close(lock); close(reg); errno = e; return -1; }
    TrashList next = {0}; size_t cap = 0; int r = 0;
    for (;;) {
        errno=0; struct dirent *item=readdir(dir);
        if (!item) { if (errno) r=-1; break; }
        if (item->d_name[0] == '.') continue;
        TrashEntry entry = {0}; struct stat st;
        if (read_record(reg, item->d_name, &entry) < 0 || lstat(entry.stored, &st) < 0) continue;
        if (fs_join(entry.record, root, item->d_name) < 0) continue;
        if (next.count == cap) {
            cap = cap ? cap * 2 : 32; TrashEntry *v = realloc(next.entries, cap * sizeof(*v));
            if (!v) { r = -1; break; } next.entries = v;
        }
        next.entries[next.count++] = entry;
    }
    int e = errno; closedir(dir); close(lock);
    if (r) { trash_free(&next); errno = e; return -1; }
    if (next.count) qsort(next.entries, next.count, sizeof(*next.entries), newest);
    trash_free(list); *list = next; return 0;
}
