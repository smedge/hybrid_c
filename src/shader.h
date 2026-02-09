#ifndef SHADER_H
#define SHADER_H

#include <OpenGL/gl3.h>
#include "mat4.h"

typedef struct {
	GLuint program;
	GLint u_projection;
	GLint u_view;
} ShaderProgram;

typedef struct {
	ShaderProgram color_shader;
	ShaderProgram text_shader;
	GLint text_u_texture;
} Shaders;

void Shaders_initialize(Shaders *shaders);
void Shaders_cleanup(Shaders *shaders);
void Shader_set_matrices(const ShaderProgram *sp, const Mat4 *projection, const Mat4 *view);

#endif
