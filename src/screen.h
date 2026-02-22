#ifndef SCREEN_H
#define SCREEN_H

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
	float norm_w;   /* Normalized world-projection width  (<=1440) */
	float norm_h;   /* Normalized world-projection height (<=900)  */
} Screen;

#endif