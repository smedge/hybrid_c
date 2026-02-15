#ifndef BLOOM_H
#define BLOOM_H

#include <stdbool.h>
#include <OpenGL/gl3.h>

typedef struct {
	bool valid;
	GLuint source_fbo, source_tex;
	GLuint ping_fbo, ping_tex;
	GLuint pong_fbo, pong_tex;

	GLuint quad_vao, quad_vbo;

	GLuint blur_program;
	GLint blur_u_image;
	GLint blur_u_horizontal;
	GLint blur_u_texel_size;

	GLuint composite_program;
	GLint composite_u_image;
	GLint composite_u_intensity;

	int width, height;
	int divisor;
	float intensity;
	int blur_passes;
} Bloom;

void Bloom_initialize(Bloom *bloom, int draw_w, int draw_h);
void Bloom_cleanup(Bloom *bloom);
void Bloom_resize(Bloom *bloom, int draw_w, int draw_h);
void Bloom_begin_source(Bloom *bloom);
void Bloom_end_source(Bloom *bloom, int draw_w, int draw_h);
void Bloom_blur(Bloom *bloom);
void Bloom_composite(Bloom *bloom, int draw_w, int draw_h);

#endif
