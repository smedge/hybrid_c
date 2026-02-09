#ifndef TEXT_H
#define TEXT_H

#include <OpenGL/gl3.h>
#include "shader.h"
#include "mat4.h"

typedef struct {
	GLuint texture;
	GLuint vao;
	GLuint vbo;
	int atlas_width;
	int atlas_height;
	float char_data[96][7]; /* x0,y0,x1,y1, xoff,yoff,xadvance per ASCII 32-127 */
	float font_size;
} TextRenderer;

void Text_initialize(TextRenderer *tr, const char *font_path, float font_size);
void Text_cleanup(TextRenderer *tr);
void Text_render(TextRenderer *tr, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view,
	const char *text, float x, float y,
	float r, float g, float b, float a);

#endif
