#ifndef TERMNAV_SIXEL_H
#define TERMNAV_SIXEL_H
#include <stddef.h>
/* Bounded encoder for an indexed raster with a 256-entry RGB palette. */
char *sixel_encode(const unsigned char *palette,const unsigned char *pixels,unsigned width,unsigned height,size_t *length);
#endif
