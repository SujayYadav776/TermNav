#define _GNU_SOURCE
#include "search.h"
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void skipped(Search *s) { pthread_mutex_lock(&s->mutex); ++s->skipped; pthread_mutex_unlock(&s->mutex); }
/* Own each directory descriptor; do not follow directory symlinks or mounts. */
static void walk(Search *s,int fd,const char *path,dev_t device,unsigned depth) {
    DIR *dir=fdopendir(fd);
    if(!dir) { close(fd); skipped(s); return; }
    while(!atomic_load(&s->cancel) && !s->limited && !s->error) {
        errno=0; struct dirent *entry=readdir(dir);
        if(!entry) { if(errno) skipped(s); break; }
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..") || (!s->hidden && entry->d_name[0]=='.')) continue;
        struct stat st; char child[PATH_MAX];
        if(fstatat(fd,entry->d_name,&st,AT_SYMLINK_NOFOLLOW)<0 || fs_join(child,path,entry->d_name)<0) { skipped(s); continue; }
        bool directory=S_ISDIR(st.st_mode);
        if(fs_match(entry->d_name,s->query)) {
            char *copy=strdup(child);
            if(!copy) { s->error=ENOMEM; break; }
            pthread_mutex_lock(&s->mutex);
            s->items[s->count++]=(SearchItem){copy,directory}; s->limited=s->count==SEARCH_MAX;
            pthread_mutex_unlock(&s->mutex);
        }
        if(directory && !s->limited && !atomic_load(&s->cancel)) {
            if(depth>=128 || st.st_dev!=device) { skipped(s); continue; }
            int next=openat(fd,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(next<0) { skipped(s); continue; }
            struct stat opened;
            if(fstat(next,&opened)<0 || opened.st_dev!=device) { close(next); skipped(s); continue; }
            walk(s,next,child,device,depth+1);
        }
    }
    closedir(dir);
}
static void *worker(void *ctx) {
    Search *s=ctx; struct stat st;
    int fd=open(s->path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0) s->error=errno;
    else if(fstat(fd,&st)<0) { s->error=errno; close(fd); }
    else walk(s,fd,s->path,st.st_dev,0);
    atomic_store(&s->done,true); return NULL;
}
void search_stop(Search *s) {
    if (s->started) { atomic_store(&s->cancel,true); pthread_join(s->thread,NULL); s->started = false; }
    for (size_t i=0;i<s->count;++i) free(s->items[i].path);
    s->count = s->skipped = 0;
}
void search_destroy(Search *s) { search_stop(s); if(s->initialized) pthread_mutex_destroy(&s->mutex); s->initialized = false; }
int search_start(Search *s, const char *path, const char *query, bool hidden) {
    if (!*query || strlen(query)>=sizeof(s->query)) { errno = EINVAL; return -1; }
    char real[PATH_MAX]; if(!realpath(path,real)) return -1;
    search_stop(s);
    if (!s->initialized) { int e=pthread_mutex_init(&s->mutex,NULL); if(e) { errno=e; return -1; } s->initialized=true; }
    snprintf(s->path,sizeof(s->path),"%s",real); snprintf(s->query,sizeof(s->query),"%s",query);
    s->hidden=hidden; s->error=0; s->limited=false;
    atomic_store(&s->cancel,false); atomic_store(&s->done,false);
    pthread_attr_t attr; int e=pthread_attr_init(&attr);
    if(!e) { e=pthread_attr_setstacksize(&attr,2*1024*1024); if(!e) e=pthread_create(&s->thread,&attr,worker,s); pthread_attr_destroy(&attr); }
    if(e) { s->error=e; atomic_store(&s->done,true); errno=e; return -1; } s->started=true; return 0;
}
