#include "color.h"

static float rgb_to_float_value(unsigned char value);

ColorFloat Color_rgb_to_float(const ColorRGB *rgbColor)
{
	ColorFloat colorFloat = {0.0, 0.0, 0.0, 0.0};
	colorFloat.red = rgb_to_float_value(colorFloat.red);
	colorFloat.green = rgb_to_float_value(colorFloat.green);
	colorFloat.blue = rgb_to_float_value(colorFloat.blue);
	colorFloat.alpha = rgb_to_float_value(colorFloat.alpha);
	return colorFloat;
}

static float rgb_to_float_value(unsigned char value)
{
	return value / 255.0f;
}