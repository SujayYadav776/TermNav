#define _GNU_SOURCE
#include "search.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void await_search(Search *s) {
    for(unsigned i=0;!atomic_load(&s->done);++i) { assert(i<10000); usleep(1000); }
}
static void put(const char *directory,const char *name) {
    char path[PATH_MAX]; assert(!fs_join(path,directory,name));
    int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0600); assert(fd>=0); assert(!close(fd));
}
int main(void) {
    char root[]="/tmp/termnav-search-XXXXXX"; assert(mkdtemp(root));
    char nested[PATH_MAX],hidden[PATH_MAX],link[PATH_MAX];
    assert(!fs_mkdir(root,"nested")); assert(!fs_join(nested,root,"nested"));
    assert(!fs_mkdir(root,".hidden")); assert(!fs_join(hidden,root,".hidden"));
    put(nested,"Report.TXT"); put(hidden,"report-secret.txt");
    assert(!fs_join(link,nested,"cycle")); assert(!symlink(root,link));
    Search *s=calloc(1,sizeof(*s)); assert(s);
    assert(!search_start(s,root,"report",false)); await_search(s);
    assert(!s->error && s->count==1 && strstr(s->items[0].path,"Report.TXT"));
    assert(!search_start(s,root,"report",true)); await_search(s); assert(s->count==2);
    assert(!search_start(s,root,"nested",false)); await_search(s); assert(s->count==1 && s->items[0].directory);
    assert(!search_start(s,root,"missing",false)); await_search(s); assert(!s->count);
    for(int i=0;i<SEARCH_MAX+1;++i) { char name[32]; snprintf(name,sizeof(name),"limit-%04d",i); put(nested,name); }
    assert(!search_start(s,root,"limit",false)); await_search(s); assert(s->count==SEARCH_MAX && s->limited);
    assert(!search_start(s,root,"limit",false)); search_stop(s); assert(!s->started && !s->count);
    assert(!search_start(s,root,"Report",false)); await_search(s); assert(s->count==1);
    search_destroy(s); free(s); assert(!fs_delete(root,NULL,NULL));
    puts("PASS: recursive search, case-insensitive matching, hidden folders, symlink loops, directories, result limit, cancellation and restart");
}
