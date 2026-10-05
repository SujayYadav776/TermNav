#ifndef TERMNAV_IMAGE_TERMINAL_H
#define TERMNAV_IMAGE_TERMINAL_H
#include <stdbool.h>
/* Probe only terminal capabilities; never infer graphics from TERM's name. */
bool image_terminal_probe(unsigned *cell_width, unsigned *cell_height);
#endif
