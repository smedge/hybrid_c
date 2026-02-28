#ifndef COLOR_H
#define COLOR_H

typedef struct {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char alpha;
} ColorRGB;

typedef struct {
	float red;
	float green;
	float blue;
	float alpha;
} ColorFloat;

ColorFloat Color_rgb_to_float(const ColorRGB *rgbColor);

typedef struct {
	float h; /* 0-360 */
	float s; /* 0-1 */
	float v; /* 0-1 */
} ColorHSV;

ColorHSV Color_rgb_to_hsv(float r, float g, float b);
void Color_hsv_to_rgb(ColorHSV hsv, float *r, float *g, float *b);

#endif