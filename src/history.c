#include "history.h"
#include <stdio.h>
#include <string.h>
void history_push(History *h, const char *path) {
    if (h->count && !strcmp(h->entries[h->position].path, path)) return;
    if (h->count) h->count = h->position + 1;
    if (h->count == HISTORY_MAX) { memmove(h->entries, h->entries + 1, (HISTORY_MAX - 1) * sizeof(*h->entries)); --h->count; }
    HistoryEntry *entry = &h->entries[h->count]; memset(entry, 0, sizeof(*entry));
    snprintf(entry->path, sizeof(entry->path), "%s", path); h->position = h->count++;
}
