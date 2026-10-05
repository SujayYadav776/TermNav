#ifndef TERMNAV_TRASH_H
#define TERMNAV_TRASH_H
#include "fs_ops.h"
typedef struct {
    char record[PATH_MAX], original[PATH_MAX], stored[PATH_MAX];
    uint64_t batch, when;
} TrashEntry;
typedef struct { TrashEntry *entries; size_t count; } TrashList;
int trash_put(const char *path, uint64_t batch);
int trash_restore(const char *record, char original[PATH_MAX]);
int trash_list(TrashList *list);
void trash_free(TrashList *list);
#endif
