#include "map_window.h"

#include <OpenGL/gl3.h>
#include <stdlib.h>
#include <stdio.h>

#include "map.h"
#include "zone.h"
#include "render.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "ship.h"
#include "color.h"
#include "fog_of_war.h"

#include <math.h>

/* Shader source */
static const char *vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec2 a_texcoord;\n"
	"uniform mat4 u_projection;\n"
	"out vec2 v_texcoord;\n"
	"void main() {\n"
	"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
	"    v_texcoord = a_texcoord;\n"
	"}\n";

static const char *frag_src =
	"#version 330 core\n"
	"in vec2 v_texcoord;\n"
	"out vec4 fragColor;\n"
	"uniform sampler2D u_texture;\n"
	"void main() {\n"
	"    fragColor = texture(u_texture, v_texcoord);\n"
	"}\n";

/* GL resources */
static GLuint shaderProgram;
static GLint u_projection_loc;
static GLint u_texture_loc;
static GLuint texture;
static GLuint vao, vbo;

/* State */
static bool isOpen = false;
static int texSize = 0;

static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);

	GLint ok;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(s, 512, NULL, log);
		printf("MapWindow shader compile error: %s\n", log);
	}
	return s;
}

void MapWindow_initialize(void)
{
	/* Compile shader program */
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vs);
	glAttachShader(shaderProgram, fs);
	glLinkProgram(shaderProgram);

	GLint ok;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, log);
		printf("MapWindow shader link error: %s\n", log);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	u_projection_loc = glGetUniformLocation(shaderProgram, "u_projection");
	u_texture_loc = glGetUniformLocation(shaderProgram, "u_texture");

	/* Create texture (data uploaded on toggle-open) */
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Create VAO/VBO for quad */
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	/* 6 vertices * 4 floats (x, y, u, v) — uploaded each frame */
	glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
		(void *)(2 * sizeof(float)));
	glBindVertexArray(0);

	isOpen = false;
	texSize = 0;
}

void MapWindow_cleanup(void)
{
	glDeleteProgram(shaderProgram);
	glDeleteTextures(1, &texture);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

static void generate_texture(void)
{
	const Zone *z = Zone_get();
	int size = z->size;
	if (size <= 0) size = MAP_SIZE;
	texSize = size;

	unsigned char *pixels = malloc(size * size * 4);
	if (!pixels) return;

	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			int idx = (y * size + x) * 4;

			if (!FogOfWar_is_revealed(x, y)) {
				/* Unrevealed = black */
				pixels[idx + 0] = 0;
				pixels[idx + 1] = 0;
				pixels[idx + 2] = 0;
				pixels[idx + 3] = 255;
				continue;
			}

			/* Check for soft edge (revealed cell next to unrevealed) */
			bool edge = false;
			for (int dy = -1; dy <= 1 && !edge; dy++) {
				for (int dx = -1; dx <= 1 && !edge; dx++) {
					if (dx == 0 && dy == 0) continue;
					if (!FogOfWar_is_revealed(x + dx, y + dy))
						edge = true;
				}
			}

			const MapCell *cell = Map_get_cell(x, y);

			if (cell && !cell->empty) {
				/* Brighten for visibility (same 8x as minimap) */
				int r = cell->primaryColor.red * 8;
				int g = cell->primaryColor.green * 8;
				int b = cell->primaryColor.blue * 8;
				if (edge) { r /= 2; g /= 2; b /= 2; }
				pixels[idx + 0] = (unsigned char)(r > 255 ? 255 : r);
				pixels[idx + 1] = (unsigned char)(g > 255 ? 255 : g);
				pixels[idx + 2] = (unsigned char)(b > 255 ? 255 : b);
				pixels[idx + 3] = 255;
			} else {
				if (edge) {
					pixels[idx + 0] = 5;
					pixels[idx + 1] = 5;
					pixels[idx + 2] = 7;
				} else {
					pixels[idx + 0] = 10;
					pixels[idx + 1] = 10;
					pixels[idx + 2] = 15;
				}
				pixels[idx + 3] = 255;
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(pixels);
}

void MapWindow_toggle(void)
{
	isOpen = !isOpen;
	if (isOpen)
		generate_texture();
}

bool MapWindow_is_open(void)
{
	return isOpen;
}

void MapWindow_update(const Input *input)
{
	if (!isOpen) return;

	if (input->keyEsc)
		isOpen = false;
}

void MapWindow_render(const Screen *screen)
{
	if (!isOpen) return;
	if (texSize <= 0) return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	/* Window sizing: 70% of shortest dimension, square, centered */
	float sw = (float)screen->width;
	float sh = (float)screen->height;
	float shortest = sw < sh ? sw : sh;
	float win_size = shortest * 0.7f;

	float pad = 8.0f;

	float pw = win_size + pad * 2.0f;
	float ph = pw;

	float px = (sw - pw) * 0.5f;
	float py = (sh - ph) * 0.5f;

	/* Panel background */
	Render_quad_absolute(px, py, px + pw, py + ph,
		0.08f, 0.08f, 0.12f, 0.95f);

	/* Panel border */
	float brc = 0.3f;
	Render_thick_line(px, py, px + pw, py, 1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px, py + ph, px + pw, py + ph,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px, py, px, py + ph, 1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px + pw, py, px + pw, py + ph,
		1.0f, brc, brc, brc, 0.8f);

	/* Map texture quad coordinates (inside padding) */
	float qx = px + pad;
	float qy = py + pad;
	float qw = win_size;
	float qh = win_size;

	/* Flush panel geometry before switching shaders */
	Render_flush(&proj, &ident);

	/* Title floating above window border (same style as settings/catalog) */
	Text_render(tr, shaders, &proj, &ident,
		"MAP", px + pw * 0.5f - 13.0f, py - 5.0f,
		0.7f, 0.7f, 1.0f, 0.9f);

	/*
	 * Y-flip: UI projection is Y-down (0=top). Texture V=0 is bottom row.
	 * To show north=up matching world orientation, flip V coords:
	 * top-left of quad gets V=1, bottom-left gets V=0.
	 */
	float verts[] = {
		/* x,          y,      u,    v */
		qx,       qy,      0.0f, 1.0f,  /* top-left */
		qx + qw,  qy,      1.0f, 1.0f,  /* top-right */
		qx,       qy + qh, 0.0f, 0.0f,  /* bottom-left */

		qx + qw,  qy,      1.0f, 1.0f,  /* top-right */
		qx + qw,  qy + qh, 1.0f, 0.0f,  /* bottom-right */
		qx,       qy + qh, 0.0f, 0.0f,  /* bottom-left */
	};

	/* Draw textured quad with our shader */
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(u_projection_loc, 1, GL_FALSE, proj.m);
	glUniform1i(u_texture_loc, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Restore color shader so subsequent UI rendering works */
	glUseProgram(shaders->color_shader.program);

	/* Large grid divisions (every 16 cells) */
	{
		int step = 16;
		for (int g = step; g < texSize; g += step) {
			float frac = (float)g / (float)texSize;
			/* Vertical lines */
			float lx = qx + frac * qw;
			Render_thick_line(lx, qy, lx, qy + qh,
				1.0f, 0.3f, 0.3f, 0.4f, 0.15f);
			/* Horizontal lines */
			float ly = qy + frac * qh;
			Render_thick_line(qx, ly, qx + qw, ly,
				1.0f, 0.3f, 0.3f, 0.4f, 0.15f);
		}
	}

	/*
	 * Landmark dots — rendered as batched quads on top of the texture.
	 * Grid coords → screen coords:
	 *   sx = qx + (grid_x / texSize) * qw
	 *   sy = qy + (1.0 - grid_y / texSize) * qh  (Y-flipped)
	 */
	float scale = qw / (float)texSize;

	/* Savepoints — cyan dots (0.0, 0.9, 0.9) matching minimap */
	{
		const Zone *z = Zone_get();
		float ds = 3.0f;
		for (int i = 0; i < z->savepoint_count; i++) {
			if (!FogOfWar_is_revealed(z->savepoints[i].grid_x,
				z->savepoints[i].grid_y))
				continue;
			float sx = qx + z->savepoints[i].grid_x * scale;
			float sy = qy + (1.0f - z->savepoints[i].grid_y / (float)texSize) * qh;
			Render_quad_absolute(sx - ds, sy - ds, sx + ds, sy + ds,
				0.0f, 0.9f, 0.9f, 1.0f);
		}
	}

	/* Portals — white dots (1.0, 1.0, 1.0) matching minimap */
	{
		const Zone *z = Zone_get();
		float ds = 3.0f;
		for (int i = 0; i < z->portal_count; i++) {
			if (!FogOfWar_is_revealed(z->portals[i].grid_x,
				z->portals[i].grid_y))
				continue;
			float sx = qx + z->portals[i].grid_x * scale;
			float sy = qy + (1.0f - z->portals[i].grid_y / (float)texSize) * qh;
			Render_quad_absolute(sx - ds, sy - ds, sx + ds, sy + ds,
				1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	/* Player — red dot (1.0, 0.3, 0.3) matching minimap */
	{
		Position ship = Ship_get_position();
		float gx = (float)(ship.x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
		float gy = (float)(ship.y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
		float sx = qx + (gx / (float)texSize) * qw;
		float sy = qy + (1.0f - gy / (float)texSize) * qh;
		float ds = 3.0f;
		Render_quad_absolute(sx - ds, sy - ds, sx + ds, sy + ds,
			1.0f, 0.3f, 0.3f, 1.0f);
	}

	/* Flush landmark dots then render text */
	Render_flush(&proj, &ident);

	/* Help text */
	Text_render(tr, shaders, &proj, &ident,
		"[M] Close    [ESC] Close",
		px + 10.0f, py + ph + 15.0f,
		0.6f, 0.6f, 0.65f, 0.9f);
}
