#include "sixel.h"
#include "preview.h"
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *bytes; size_t used; bool failed; } Output;
static void append(Output *out,const char *format,...) {
    if (out->failed) return;
    va_list args; va_start(args,format);
    int n=vsnprintf(out->bytes+out->used,IMAGE_PREVIEW_BYTES-out->used,format,args); va_end(args);
    if (n<0 || (size_t)n>=IMAGE_PREVIEW_BYTES-out->used) out->failed=true;
    else out->used+=(size_t)n;
}
static void run(Output *out,unsigned char value,unsigned count) {
    if (count>3) append(out,"!%u%c",count,value+63);
    else for (unsigned i=0;i<count;++i) append(out,"%c",value+63);
}
char *sixel_encode(const unsigned char *palette,const unsigned char *pixels,unsigned width,unsigned height,size_t *length) {
    *length=0;
    if (!width || !height || width>1280 || height>960) { errno=EINVAL; return NULL; }
    Output out={.bytes=malloc(IMAGE_PREVIEW_BYTES)};
    unsigned char *planes=calloc(256,width); bool used[256]={false};
    if (!out.bytes || !planes) { free(out.bytes); free(planes); errno=ENOMEM; return NULL; }
    for (size_t i=0;i<(size_t)width*height;++i) used[pixels[i]]=true;
    append(&out,"\033P0;1q\"1;1;%u;%u",width,height);
    for (unsigned color=0;color<256;++color) if (used[color]) append(&out,"#%u;2;%u;%u;%u",color,(palette[color*3]*100u+127)/255,(palette[color*3+1]*100u+127)/255,(palette[color*3+2]*100u+127)/255);
    for (unsigned y=0;y<height && !out.failed;y+=6) {
        memset(planes,0,(size_t)width*256); bool present[256]={false};
        for (unsigned bit=0;bit<6 && y+bit<height;++bit) for (unsigned x=0;x<width;++x) {
            unsigned color=pixels[(size_t)(y+bit)*width+x]; present[color]=true; planes[(size_t)color*width+x]|=(unsigned char)(1u<<bit);
        }
        for (unsigned color=0;color<256 && !out.failed;++color) if (present[color]) {
            const unsigned char *plane=planes+(size_t)color*width; unsigned end=width;
            while (end && !plane[end-1]) --end;
            append(&out,"#%u",color);
            for (unsigned x=0;x<end;) {
                unsigned count=1; while (x+count<end && plane[x+count]==plane[x]) ++count;
                run(&out,plane[x],count); x+=count;
            }
            append(&out,"$");
        }
        if (y+6<height) append(&out,"-");
    }
    append(&out,"\033\\"); free(planes);
    if (out.failed) { free(out.bytes); errno=EFBIG; return NULL; }
    *length=out.used; return out.bytes;
}
