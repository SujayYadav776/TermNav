#include "sixel.h"
#include "preview.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    unsigned char palette[768]={0},pixels[16*7]; size_t length;
    palette[3]=255;
    for (size_t i=0;i<sizeof(pixels);++i) pixels[i]=(unsigned char)(i%2);
    char *data=sixel_encode(palette,pixels,16,7,&length);
    assert(data && length>20 && length<IMAGE_PREVIEW_BYTES);
    assert(!memcmp(data,"\033P0;1q",6) && !memcmp(data+length-2,"\033\\",2));
    assert(strstr(data,"\"1;1;16;7") && strstr(data,"#1;2;100;0;0")); free(data);
    assert(!sixel_encode(palette,pixels,0,7,&length) && errno==EINVAL);
    assert(!sixel_encode(palette,pixels,1281,7,&length) && errno==EINVAL);
    assert(!sixel_encode(palette,pixels,16,961,&length) && errno==EINVAL);
    puts("PASS: bounded Sixel encoding, palette, raster header and termination"); return 0;
}
