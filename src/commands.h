#ifndef TERMNAV_COMMANDS_H
#define TERMNAV_COMMANDS_H
#include <stddef.h>
typedef struct { const char *name, *description; int key; } Command;
extern const Command commands[];
extern const size_t command_count;
size_t command_matches(const char *query, size_t *indices, size_t capacity);
#endif
