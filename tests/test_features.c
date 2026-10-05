#define _GNU_SOURCE
#include "trash.h"
#include "usage.h"
#include "history.h"
#include "commands.h"
#include "async_ops.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int canceled(uint64_t bytes,uint64_t files,void *ctx) { (void)bytes; (void)files; (void)ctx; return 1; }
static void write_file(const char *path) { int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0600); assert(fd>=0); assert(write(fd,"original",8)==8); close(fd); }
int main(void) {
    char root[]="/tmp/termnav-features-XXXXXX"; assert(mkdtemp(root));
    char data[PATH_MAX],file[PATH_MAX],link[PATH_MAX],outside[PATH_MAX],record[PATH_MAX],restored[PATH_MAX];
    assert(!fs_join(data,root,"data")); assert(!setenv("XDG_DATA_HOME",data,1));
    assert(!fs_join(file,root,"original.txt")); write_file(file);
    assert(!trash_put(file,42)); assert(access(file,F_OK)<0);
    TrashList list={0}; assert(!trash_list(&list)); assert(list.count==1 && list.entries[0].batch==42);
    strcpy(record,list.entries[0].record); write_file(file);
    assert(trash_restore(record,restored)<0 && errno==EEXIST);
    assert(!access(list.entries[0].stored,F_OK)); assert(!unlink(file));
    trash_free(&list); assert(!trash_list(&list)); assert(list.count==1);
    assert(!trash_restore(list.entries[0].record,restored)); assert(!strcmp(restored,file));
    trash_free(&list); assert(!trash_list(&list)); assert(!list.count);
    assert(!fs_join(outside,root,"outside")); assert(!mkdir(outside,0700));
    char payload[PATH_MAX]; assert(!fs_join(payload,outside,"payload")); write_file(payload);
    assert(!fs_join(link,root,"link")); assert(!symlink(outside,link));
    Usage usage; assert(!usage_measure(link,&usage,NULL,NULL)); assert(usage.files==1 && !usage.directories && !usage.bytes);
    assert(usage_measure(root,&usage,canceled,NULL)<0 && errno==ECANCELED);
    assert(!trash_put(link,99)); assert(!trash_list(&list)); assert(list.count==1);
    assert(!trash_restore(list.entries[0].record,restored)); struct stat st; assert(!lstat(link,&st) && S_ISLNK(st.st_mode));
    assert(!access(payload,F_OK)); trash_free(&list);
    assert(trash_put("/",1)<0); assert(trash_put("relative",1)<0);
    History history={0}; history_push(&history,"/one"); history_push(&history,"/two"); history_push(&history,"/three");
    history.position=1; history_push(&history,"/branch"); assert(history.count==3 && !strcmp(history.entries[2].path,"/branch"));
    for (int i=0;i<100;++i) { char name[64]; snprintf(name,sizeof(name),"/directory-%d",i); history_push(&history,name); }
    assert(history.count==64 && history.position==63);
    size_t matches[64]; assert(command_matches("disk",matches,64)==1); assert(command_matches("zzzzzz",matches,64)==0);
    UsageScan scan={0}; assert(!usage_start(&scan,root));
    while (!atomic_load(&scan.done)) usleep(1000);
    assert(!scan.error && scan.completed==scan.count && scan.total.bytes>=16); usage_destroy(&scan);
    Job job={0}; char **paths=calloc(1,sizeof(*paths)); assert(paths); paths[0]=strdup(file); assert(paths[0]);
    assert(!job_start_kind(&job,paths,1,NULL,JOB_TRASH));
    while (!atomic_load(&job.done)) usleep(1000);
    assert(!job.error); assert(job_collect(&job)); assert(!trash_list(&list) && list.count==1);
    paths=calloc(1,sizeof(*paths)); assert(paths); paths[0]=strdup(list.entries[0].record); assert(paths[0]);
    assert(!job_start_kind(&job,paths,1,NULL,JOB_RESTORE));
    while (!atomic_load(&job.done)) usleep(1000);
    assert(!job.error); assert(job_collect(&job)); trash_free(&list);
    /* Pause/cancel a recursive job before it can traverse the whole fixture. */
    char tree[PATH_MAX],destination[PATH_MAX]; assert(!fs_join(tree,root,"large-tree")); assert(!mkdir(tree,0700));
    assert(!fs_join(destination,root,"destination")); assert(!mkdir(destination,0700));
    for (int i=0;i<2000;++i) { char name[32]; snprintf(name,sizeof(name),"file-%04d",i); assert(!fs_join(payload,tree,name)); write_file(payload); }
    paths=calloc(1,sizeof(*paths)); assert(paths); paths[0]=strdup(tree); assert(paths[0]);
    assert(!job_start_kind(&job,paths,1,destination,JOB_COPY)); atomic_store(&job.paused,true);
    usleep(80000); assert(!atomic_load(&job.done)); uint64_t copied=atomic_load(&job.files);
    usleep(80000); assert(atomic_load(&job.files)==copied);
    atomic_store(&job.paused,false); usleep(1000); atomic_store(&job.cancel,true);
    while (!atomic_load(&job.done)) usleep(1000);
    assert(job.error==ECANCELED); assert(job_collect(&job));
    assert(!fs_delete(root,NULL,NULL));
    puts("PASS: persistent trash, collision protection, symlink recovery, undo workers, bounded history, command search, disk scan and cancellation"); return 0;
}
