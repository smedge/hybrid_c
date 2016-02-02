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

#endif