#define _GNU_SOURCE
#include "rich_preview.h"
#include "sixel.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
const char *rich_mode(const Entry *entry) {
    if (!S_ISREG(entry->st.st_mode) || entry->symlink) return NULL;
    const char *dot = strrchr(entry->name, '.'); if (!dot) return NULL;
    if (!strcasecmp(dot,".png") || !strcasecmp(dot,".jpg") || !strcasecmp(dot,".jpeg") || !strcasecmp(dot,".webp") || !strcasecmp(dot,".gif") || !strcasecmp(dot,".bmp")) return "image";
    if (!strcasecmp(dot,".pdf")) return "pdf";
    if (!strcasecmp(dot,".zip") || !strcasecmp(dot,".tar") || !strcasecmp(dot,".tgz") || !strcasecmp(dot,".gz") || !strcasecmp(dot,".bz2") || !strcasecmp(dot,".xz")) return "archive";
    return NULL;
}
static void *worker(void *context) {
    RichPreview *job = context; Preview *p = &job->result;
    snprintf(p->path, sizeof(p->path), "%s", job->path); p->kind = PREVIEW_ERROR;
    snprintf(p->message, sizeof(p->message), "Rich preview unavailable (install Python 3 and preview helper)");
    char exe[PATH_MAX], helper[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe)-1);
    if (n < 0) goto done;
    exe[n] = 0; char *slash = strrchr(exe, '/'); if (!slash) goto done; *slash = 0;
    if (snprintf(helper, sizeof(helper), "%s/scripts/preview_helper.py", exe) >= (int)sizeof(helper)) goto done;
    if (access(helper, R_OK)) {
        if (snprintf(helper, sizeof(helper), "%s/../share/termnav/preview_helper.py", exe) >= (int)sizeof(helper) || access(helper,R_OK)) goto done;
    }
    int source = open(job->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK); struct stat st;
    if (source < 0) goto done;
    if (fstat(source,&st) || !S_ISREG(st.st_mode) || st.st_ino != job->signature.st_ino || st.st_dev != job->signature.st_dev) { close(source); goto done; }
    /* All spawn source descriptors exceed the fixed child descriptor 3. */
    int inherited = fcntl(source, F_DUPFD_CLOEXEC, 10); close(source); if (inherited < 0) goto done;
    int pipes[2]; if (pipe2(pipes, O_CLOEXEC)) { close(inherited); goto done; }
    posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,inherited,3);
    posix_spawn_file_actions_adddup2(&actions,pipes[1],STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    posix_spawnattr_t attr; posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP); posix_spawnattr_setpgroup(&attr,0);
    char width[16],height[16]; snprintf(width,sizeof(width),"%u",job->width); snprintf(height,sizeof(height),"%u",job->height);
    char *argv[] = {"python3",helper,job->mode,"/proc/self/fd/3",width,height,job->sixel ? "sixel" : "blocks",NULL}; pid_t pid;
    int err = posix_spawnp(&pid,"python3",&actions,&attr,argv,environ);
    posix_spawn_file_actions_destroy(&actions); posix_spawnattr_destroy(&attr); close(inherited); close(pipes[1]);
    if (err) { close(pipes[0]); goto done; }
    fcntl(pipes[0],F_SETFL,O_NONBLOCK);
    size_t limit=!strcmp(job->mode,"image") ? IMAGE_PREVIEW_BYTES : PREVIEW_BYTES;
    char *buffer = malloc(limit + 1); size_t bytes = 0; int status = 0; bool ended = false;
    struct timespec start, now; clock_gettime(CLOCK_MONOTONIC,&start);
    while (buffer) {
        clock_gettime(CLOCK_MONOTONIC,&now);
        if (atomic_load(&job->cancel) || now.tv_sec - start.tv_sec >= 5 || bytes == limit) break;
        struct pollfd fd = {pipes[0],POLLIN,0}; poll(&fd,1,20);
        ssize_t got = read(pipes[0],buffer+bytes,limit-bytes);
        if (got > 0) bytes += (size_t)got;
        else if (!got) { ended = true; break; }
        else if (errno != EAGAIN && errno != EINTR) break;
    }
    if (!ended) kill(-pid,SIGKILL);
    close(pipes[0]); while (waitpid(pid,&status,0) < 0 && errno == EINTR) {}
    if (!buffer) goto done;
    buffer[bytes] = 0;
    if (atomic_load(&job->cancel)) { free(buffer); goto done; }
    if (!ended && bytes < limit) { snprintf(p->message,sizeof(p->message),"Preview timed out after 5 seconds"); free(buffer); goto done; }
    if (bytes>=8 && !memcmp(buffer,"TNINDEX ",8)) {
        char *newline=memchr(buffer,'\n',bytes); unsigned w,h,ow,oh;
        if (ended && WIFEXITED(status) && !WEXITSTATUS(status) && newline && sscanf(buffer,"TNINDEX %u %u %u %u",&w,&h,&ow,&oh)==4 && w && h && w<=1280 && h<=960) {
            size_t offset=(size_t)(newline+1-buffer), size=bytes-offset;
            if (size==768+(size_t)w*h) {
                p->data=sixel_encode((const unsigned char*)buffer+offset,(const unsigned char*)buffer+offset+768,w,h,&p->bytes); free(buffer);
                if (!p->data) { snprintf(p->message,sizeof(p->message),"Image encoding failed: %s",strerror(errno)); goto done; }
                p->image_width=w; p->image_height=h; p->original_width=ow; p->original_height=oh; p->image_sixel=true; p->kind=PREVIEW_IMAGE; strcpy(p->format,"IMAGE / SIXEL"); goto done;
            }
        }
    }
    if (bytes >= 6 && !memcmp(buffer,"TNIMG ",6)) {
        char *newline = memchr(buffer,'\n',bytes); unsigned w,h,ow,oh;
        if (ended && WIFEXITED(status) && !WEXITSTATUS(status) && newline && sscanf(buffer,"TNIMG %u %u %u %u",&w,&h,&ow,&oh)==4 && w && h && w<=1280 && h<=960 && bytes-(size_t)(newline+1-buffer)==(size_t)w*h*3) {
            size_t offset=(size_t)(newline+1-buffer); memmove(buffer,buffer+offset,bytes-offset); p->data=buffer; p->bytes=bytes-offset;
            p->image_width=w; p->image_height=h; p->original_width=ow; p->original_height=oh; p->kind=PREVIEW_IMAGE; strcpy(p->format,"IMAGE"); goto done;
        }
    } else if ((WIFEXITED(status) && !WEXITSTATUS(status)) || (bytes == PREVIEW_BYTES && strcmp(job->mode,"image"))) {
        p->data=buffer; p->bytes=bytes; p->kind=PREVIEW_TEXT; p->truncated=!ended;
        strcpy(p->format,!strcmp(job->mode,"pdf") ? "PDF / FIRST 5 PAGES" : "ARCHIVE / CONTENTS");
        if (preview_index(p)) { p->kind=PREVIEW_ERROR; strcpy(p->message,"Out of memory"); }
        goto done;
    } else if (bytes) snprintf(p->message,sizeof(p->message),"%.240s",buffer);
    free(buffer);
done:
    atomic_store(&job->done,true); return NULL;
}
void rich_stop(RichPreview *job) {
    if (job->started) { atomic_store(&job->cancel,true); pthread_join(job->thread,NULL); }
    preview_free(&job->result); job->started=false;
}
int rich_start(RichPreview *job,const char *path,const Entry *entry,unsigned width,unsigned height,bool sixel) {
    rich_stop(job); const char *mode=rich_mode(entry); if (!mode) return -1;
    snprintf(job->path,sizeof(job->path),"%s",path); strcpy(job->mode,mode); job->signature=entry->st;
    job->width=width; job->height=height; job->sixel=sixel;
    atomic_store(&job->done,false); atomic_store(&job->cancel,false);
    int err=pthread_create(&job->thread,NULL,worker,job); if (err) { errno=err; return -1; }
    job->started=true; return 0;
}
bool rich_collect(RichPreview *job,Preview *p) {
    if (!job->started || !atomic_load(&job->done)) return false;
    pthread_join(job->thread,NULL); job->started=false; preview_free(p); *p=job->result; memset(&job->result,0,sizeof(job->result)); return true;
}
