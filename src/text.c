#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	float x, y;
	float r, g, b, a;
	float u, v;
} TextVertex;

void Text_initialize(TextRenderer *tr, const char *font_path, float font_size)
{
	tr->font_size = font_size;
	tr->atlas_width = 512;
	tr->atlas_height = 512;

	/* Read font file */
	FILE *f = fopen(font_path, "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open font: %s\n", font_path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	if (fsize < 0) {
		fclose(f);
		return;
	}
	fseek(f, 0, SEEK_SET);
	unsigned char *font_buffer = malloc(fsize);
	if (!font_buffer) {
		fclose(f);
		return;
	}
	size_t read_count = fread(font_buffer, 1, fsize, f);
	fclose(f);
	if ((long)read_count != fsize) {
		free(font_buffer);
		return;
	}

	/* Bake font atlas */
	unsigned char *atlas = calloc(tr->atlas_width * tr->atlas_height, 1);
	stbtt_bakedchar cdata[96];
	int bake_result = stbtt_BakeFontBitmap(font_buffer, 0, font_size,
		atlas, tr->atlas_width, tr->atlas_height, 32, 96, cdata);
	if (bake_result < 0)
		fprintf(stderr, "WARNING: font atlas too small, some glyphs may be missing\n");
	free(font_buffer);

	/* Store glyph metrics */
	for (int i = 0; i < 96; i++) {
		tr->char_data[i][0] = (float)cdata[i].x0;
		tr->char_data[i][1] = (float)cdata[i].y0;
		tr->char_data[i][2] = (float)cdata[i].x1;
		tr->char_data[i][3] = (float)cdata[i].y1;
		tr->char_data[i][4] = cdata[i].xoff;
		tr->char_data[i][5] = cdata[i].yoff;
		tr->char_data[i][6] = cdata[i].xadvance;
	}

	/* Create GL texture */
	glGenTextures(1, &tr->texture);
	glBindTexture(GL_TEXTURE_2D, tr->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
		tr->atlas_width, tr->atlas_height, 0,
		GL_RED, GL_UNSIGNED_BYTE, atlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	free(atlas);

	/* Create VAO/VBO for text quads */
	glGenVertexArrays(1, &tr->vao);
	glGenBuffers(1, &tr->vbo);
	glBindVertexArray(tr->vao);
	glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);

	/* position: location 0 */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
		sizeof(TextVertex), (void *)0);

	/* color: location 1 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
		sizeof(TextVertex), (void *)(2 * sizeof(float)));

	/* texcoord: location 2 */
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
		sizeof(TextVertex), (void *)(6 * sizeof(float)));

	glBindVertexArray(0);
}

void Text_cleanup(TextRenderer *tr)
{
	glDeleteTextures(1, &tr->texture);
	glDeleteBuffers(1, &tr->vbo);
	glDeleteVertexArrays(1, &tr->vao);
}

#define TEXT_MAX_CHARS 1024

float Text_measure_width(const TextRenderer *tr, const char *text)
{
	float w = 0.0f;
	for (int i = 0; text[i]; i++) {
		int ch = (unsigned char)text[i];
		if (ch < 32 || ch > 127)
			continue;
		w += tr->char_data[ch - 32][6];
	}
	return w;
}

void Text_render(TextRenderer *tr, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view,
	const char *text, float x, float y,
	float r, float g, float b, float a)
{
	if (!text || !text[0])
		return;

	int len = (int)strlen(text);
	if (len > TEXT_MAX_CHARS)
		len = TEXT_MAX_CHARS;
	static TextVertex verts[TEXT_MAX_CHARS * 6];
	int vert_count = 0;

	float cursor_x = x;
	float inv_w = 1.0f / tr->atlas_width;
	float inv_h = 1.0f / tr->atlas_height;

	for (int i = 0; i < len; i++) {
		int ch = (unsigned char)text[i];
		if (ch < 32 || ch > 127)
			continue;
		int idx = ch - 32;

		float tx0 = tr->char_data[idx][0];
		float ty0 = tr->char_data[idx][1];
		float tx1 = tr->char_data[idx][2];
		float ty1 = tr->char_data[idx][3];
		float xoff = tr->char_data[idx][4];
		float yoff = tr->char_data[idx][5];
		float xadv = tr->char_data[idx][6];

		float gw = tx1 - tx0;
		float gh = ty1 - ty0;

		float px = cursor_x + xoff;
		float py = y + yoff;

		float u0 = tx0 * inv_w;
		float v0 = ty0 * inv_h;
		float u1 = tx1 * inv_w;
		float v1 = ty1 * inv_h;

		/* Two triangles per quad */
		TextVertex *v = &verts[vert_count];
		v[0] = (TextVertex){px,      py,      r, g, b, a, u0, v0};
		v[1] = (TextVertex){px + gw, py,      r, g, b, a, u1, v0};
		v[2] = (TextVertex){px + gw, py + gh, r, g, b, a, u1, v1};
		v[3] = (TextVertex){px,      py,      r, g, b, a, u0, v0};
		v[4] = (TextVertex){px + gw, py + gh, r, g, b, a, u1, v1};
		v[5] = (TextVertex){px,      py + gh, r, g, b, a, u0, v1};
		vert_count += 6;

		cursor_x += xadv;
	}

	if (vert_count == 0)
		return;

	/* Draw */
	Shader_set_matrices(&shaders->text_shader, projection, view);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tr->texture);
	glUniform1i(shaders->text_u_texture, 0);

	glBindVertexArray(tr->vao);
	glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
	glBufferData(GL_ARRAY_BUFFER,
		(GLsizeiptr)(vert_count * sizeof(TextVertex)),
		verts, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, vert_count);
	glBindVertexArray(0);
}
