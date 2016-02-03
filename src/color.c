#include "color.h"
#include <stdio.h>

static float rgb_to_float_value(unsigned char value);

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