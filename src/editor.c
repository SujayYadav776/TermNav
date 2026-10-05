#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
int app_edit(const char *editor, const char *path) {
    pid_t pid; char *args[] = {(char *)editor, (char *)path, NULL};
    int err = posix_spawnp(&pid, editor, NULL, NULL, args, environ);
    if (err) return 127;
    int status;
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
