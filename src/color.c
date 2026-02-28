#include "color.h"
#include <stdio.h>

static float rgb_to_float_value(unsigned char value);

ColorHSV Color_rgb_to_hsv(float r, float g, float b)
{
	ColorHSV hsv = {0.0f, 0.0f, 0.0f};
	float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
	float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
	float delta = max - min;

	hsv.v = max;
	if (max > 0.0f)
		hsv.s = delta / max;
	else
		return hsv;

	if (delta < 0.00001f) {
		hsv.h = 0.0f;
		return hsv;
	}

	if (r >= max)
		hsv.h = (g - b) / delta;
	else if (g >= max)
		hsv.h = 2.0f + (b - r) / delta;
	else
		hsv.h = 4.0f + (r - g) / delta;

	hsv.h *= 60.0f;
	if (hsv.h < 0.0f) hsv.h += 360.0f;
	return hsv;
}

void Color_hsv_to_rgb(ColorHSV hsv, float *r, float *g, float *b)
{
	if (hsv.s <= 0.0f) {
		*r = *g = *b = hsv.v;
		return;
	}

	float hh = hsv.h;
	if (hh >= 360.0f) hh = 0.0f;
	hh /= 60.0f;
	int i = (int)hh;
	float ff = hh - (float)i;
	float p = hsv.v * (1.0f - hsv.s);
	float q = hsv.v * (1.0f - hsv.s * ff);
	float t = hsv.v * (1.0f - hsv.s * (1.0f - ff));

	switch (i) {
	case 0: *r = hsv.v; *g = t; *b = p; break;
	case 1: *r = q; *g = hsv.v; *b = p; break;
	case 2: *r = p; *g = hsv.v; *b = t; break;
	case 3: *r = p; *g = q; *b = hsv.v; break;
	case 4: *r = t; *g = p; *b = hsv.v; break;
	default: *r = hsv.v; *g = p; *b = q; break;
	}
}

ColorFloat Color_rgb_to_float(const ColorRGB *rgbColor)
{
	ColorFloat colorFloat = {0.0, 0.0, 0.0, 0.0};
	colorFloat.red = rgb_to_float_value(rgbColor->red);
	colorFloat.green = rgb_to_float_value(rgbColor->green);
	colorFloat.blue = rgb_to_float_value(rgbColor->blue);
	colorFloat.alpha = rgb_to_float_value(rgbColor->alpha);
	return colorFloat;
}

static float rgb_to_float_value(unsigned char value)
{
	return value / 255.0f;	
}