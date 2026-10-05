#ifndef TERMNAV_FS_OPS_H
#define TERMNAV_FS_OPS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <limits.h>

typedef struct {
    char *name;
    struct stat st;
    bool directory, symlink, marked;
} Entry;
typedef struct {
    char path[PATH_MAX];
    Entry *entries;
    size_t count;
} Listing;
typedef int (*ProgressFn)(uint64_t bytes, uint64_t files, void *ctx);

int fs_join(char out[PATH_MAX], const char *dir, const char *name);
void fs_parent(char out[PATH_MAX], const char *path);
const char *fs_basename(const char *path);
int fs_list(Listing *out, const char *path, bool hidden, int sort);
void fs_free(Listing *listing);
int fs_mkdir(const char *dir, const char *name);
int fs_rename(const char *dir, const char *old_name, const char *new_name);
/* Copy/delete take absolute paths. Recursive operations never follow symlinks. */
int fs_copy(const char *src, const char *dst, ProgressFn fn, void *ctx);
int fs_delete(const char *path, ProgressFn fn, void *ctx);
bool fs_match(const char *name, const char *query);
void fs_size(char *out, size_t cap, off_t size);
void fs_permissions(char out[11], mode_t mode);
#endif
