#include "commands.h"
#include "fs_ops.h"
#include <stdio.h>
const Command commands[] = {
    {"Preview selected entry", "Full-screen text, images, PDF and archives", '\t'},
    {"Operations dashboard", "Queue, progress, pause, cancel and completed jobs", 't'},
    {"Disk usage explorer", "Scan folder sizes in the background", 'D'},
    {"Directory history", "Jump to a recently visited folder", 'H'},
    {"History back", "Return to the previous directory", 'b'},
    {"History forward", "Revisit the next directory", 'f'},
    {"Move selection to trash", "Recover selected or marked files later", 'd'},
    {"Undo last trash operation", "Restore the newest trash batch", 'u'},
    {"Browse trash", "Restore an individual item", 'T'},
    {"Permanently delete selection", "Irreversible; requires explicit confirmation", 'X'},
    {"Copy to directory", "Queue a copy to an existing folder", 'c'},
    {"Paste clipboard here", "Queue clipboard files into this directory", 'p'},
    {"Copy paths to clipboard", "Remember marked or selected paths", 'y'},
    {"Create directory", "Create a folder in this location", 'a'},
    {"Rename selected entry", "Rename without overwriting another file", 'r'},
    {"Go to directory", "Jump to an absolute or relative path", 'o'},
    {"Go home", "Open your home directory", '~'},
    {"Filter files", "Live fuzzy filename filtering", '/'},
    {"Toggle hidden files", "Reveal or hide dotfiles", '.'},
    {"Cycle sort order", "Name, size or modified time", 's'},
    {"Refresh directory", "Reload current files and previews", 'R'},
    {"Keyboard help", "Show all shortcuts", '?'},
    {"Quit TermNav", "Close safely after confirming active jobs", 'q'}
};
const size_t command_count = sizeof(commands) / sizeof(*commands);
size_t command_matches(const char *query, size_t *indices, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < command_count && count < capacity; ++i) {
        char combined[256]; snprintf(combined, sizeof(combined), "%s %s", commands[i].name, commands[i].description);
        if (fs_match(combined, query)) indices[count++] = i;
    }
    return count;
}
