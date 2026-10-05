#define _POSIX_C_SOURCE 200809L
#include "image_terminal.h"
#include <curses.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

bool image_terminal_probe(unsigned *width,unsigned *height) {
    const char *choice=getenv("TERMNAV_IMAGE");
    *width=8; *height=16;
    if (choice && !strcmp(choice,"blocks")) return false;
    /* Multiplexers require their own passthrough configuration. */
    if (getenv("TMUX") && !(choice && !strcmp(choice,"sixel"))) return false;
    fputs("\033[c\033[16t",stdout); fflush(stdout);
    char input[1024]; size_t count=0; bool sixel=false, da=false, pixels=false;
    struct timespec start,now; clock_gettime(CLOCK_MONOTONIC,&start);
    while (count<sizeof(input)-1) {
        clock_gettime(CLOCK_MONOTONIC,&now);
        long elapsed=(now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000;
        if (elapsed>=250 || (da && pixels)) break;
        struct pollfd fd={STDIN_FILENO,POLLIN,0};
        if (poll(&fd,1,(int)(250-elapsed))<=0) break;
        ssize_t n=read(STDIN_FILENO,input+count,sizeof(input)-1-count);
        if (n<=0) break;
        count+=(size_t)n; input[count]=0;
        for (size_t i=0;i+2<count;++i) if (input[i]=='\033' && input[i+1]=='[') {
            char *end=input+i+2; while (*end && ((*end>='0' && *end<='9') || *end==';' || *end=='?')) ++end;
            if (*end=='c' && input[i+2]=='?') {
                char *p=input+i+3; bool first=true;
                while (p<end) { char *next; long value=strtol(p,&next,10); if (next==p) break; if (!first && value==4) sixel=true; first=false; p=next+(*next==';'); }
                da=true; memset(input+i,0,(size_t)(end-(input+i))+1);
            } else if (*end=='t') {
                unsigned h,w; if (sscanf(input+i,"\033[6;%u;%ut",&h,&w)==2 && w>=4 && w<=64 && h>=8 && h<=128) { *width=w; *height=h; pixels=true; memset(input+i,0,(size_t)(end-(input+i))+1); }
            }
        }
    }
    /* Preserve input typed during the brief probe, in original order. */
    for (size_t i=count;i>0;--i) if (input[i-1]) ungetch((unsigned char)input[i-1]);
    return (choice && !strcmp(choice,"sixel")) || sixel;
}
