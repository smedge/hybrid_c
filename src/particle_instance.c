#include "particle_instance.h"

#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>

/* GL resources */
static GLuint piShaderProgram;
static GLint piLocProjection;
static GLint piLocView;
static GLint piLocSoftCircle;
static GLuint piVAO;
static GLuint piTemplateVBO;
static GLuint piInstanceVBO;
static bool piInitialized;

/* Embedded shaders */
static const char *pi_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_vertex;\n"
	"layout(location = 1) in vec2 a_position;\n"
	"layout(location = 2) in float a_size;\n"
	"layout(location = 3) in float a_rotation;\n"
	"layout(location = 4) in float a_stretch;\n"
	"layout(location = 5) in vec4 a_color;\n"
	"uniform mat4 u_projection;\n"
	"uniform mat4 u_view;\n"
	"out vec2 v_uv;\n"
	"out vec4 v_color;\n"
	"void main() {\n"
	"    vec2 scaled = vec2(a_vertex.x * a_stretch, a_vertex.y) * a_size;\n"
	"    float rad = a_rotation * 3.14159265 / 180.0;\n"
	"    float c = cos(rad);\n"
	"    float s = sin(rad);\n"
	"    vec2 rotated = vec2(scaled.x * c - scaled.y * s,\n"
	"                        scaled.x * s + scaled.y * c);\n"
	"    vec2 world = rotated + a_position;\n"
	"    gl_Position = u_projection * u_view * vec4(world, 0.0, 1.0);\n"
	"    v_uv = a_vertex;\n"
	"    v_color = a_color;\n"
	"}\n";

static const char *pi_frag_src =
	"#version 330 core\n"
	"in vec2 v_uv;\n"
	"in vec4 v_color;\n"
	"uniform bool u_soft_circle;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    if (u_soft_circle) {\n"
	"        float d = length(v_uv);\n"
	"        float alpha = smoothstep(1.0, 0.2, d);\n"
	"        fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
	"    } else {\n"
	"        fragColor = v_color;\n"
	"    }\n"
	"}\n";

static GLuint pi_compile_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "ParticleInstance shader compile error: %s\n", log);
		exit(1);
	}
	return shader;
}

void ParticleInstance_initialize(void)
{
	if (piInitialized) return;
	piInitialized = true;

	/* Compile & link shader program */
	GLuint vert = pi_compile_shader(GL_VERTEX_SHADER, pi_vert_src);
	GLuint frag = pi_compile_shader(GL_FRAGMENT_SHADER, pi_frag_src);

	piShaderProgram = glCreateProgram();
	glAttachShader(piShaderProgram, vert);
	glAttachShader(piShaderProgram, frag);
	glLinkProgram(piShaderProgram);

	GLint ok;
	glGetProgramiv(piShaderProgram, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(piShaderProgram, sizeof(log), NULL, log);
		fprintf(stderr, "ParticleInstance shader link error: %s\n", log);
		exit(1);
	}
	glDeleteShader(vert);
	glDeleteShader(frag);

	piLocProjection = glGetUniformLocation(piShaderProgram, "u_projection");
	piLocView = glGetUniformLocation(piShaderProgram, "u_view");
	piLocSoftCircle = glGetUniformLocation(piShaderProgram, "u_soft_circle");

	/* Template quad: unit quad from (-1,-1) to (1,1), 6 vertices */
	float quad[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};

	glGenBuffers(1, &piTemplateVBO);
	glBindBuffer(GL_ARRAY_BUFFER, piTemplateVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	/* Instance data VBO (dynamic) */
	glGenBuffers(1, &piInstanceVBO);

	/* VAO */
	glGenVertexArrays(1, &piVAO);
	glBindVertexArray(piVAO);

	/* Attribute 0: template quad vertex (per-vertex) */
	glBindBuffer(GL_ARRAY_BUFFER, piTemplateVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glVertexAttribDivisor(0, 0);

	/* Attributes 1-5: per-instance data */
	glBindBuffer(GL_ARRAY_BUFFER, piInstanceVBO);
	int stride = sizeof(ParticleInstanceData);

	/* location 1: a_position (vec2) — offset 0 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
	glVertexAttribDivisor(1, 1);

	/* location 2: a_size (float) — offset 8 */
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(float)));
	glVertexAttribDivisor(2, 1);

	/* location 3: a_rotation (float) — offset 12 */
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
	glVertexAttribDivisor(3, 1);

	/* location 4: a_stretch (float) — offset 16 */
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));
	glVertexAttribDivisor(4, 1);

	/* location 5: a_color (vec4) — offset 20 */
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));
	glVertexAttribDivisor(5, 1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleInstance_cleanup(void)
{
	if (!piInitialized) return;
	glDeleteProgram(piShaderProgram);
	glDeleteVertexArrays(1, &piVAO);
	glDeleteBuffers(1, &piTemplateVBO);
	glDeleteBuffers(1, &piInstanceVBO);
	piInitialized = false;
}

void ParticleInstance_draw(const ParticleInstanceData *data, int count,
	const Mat4 *projection, const Mat4 *view, bool soft_circle)
{
	if (count <= 0) return;

	ParticleInstance_initialize();

	glUseProgram(piShaderProgram);
	glUniformMatrix4fv(piLocProjection, 1, GL_FALSE, projection->m);
	glUniformMatrix4fv(piLocView, 1, GL_FALSE, view->m);
	glUniform1i(piLocSoftCircle, soft_circle ? 1 : 0);

	glBindVertexArray(piVAO);
	glBindBuffer(GL_ARRAY_BUFFER, piInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, count * (int)sizeof(ParticleInstanceData),
		data, GL_DYNAMIC_DRAW);

	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);

	glBindVertexArray(0);
	glUseProgram(0);
}
