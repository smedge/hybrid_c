#ifndef MAT4_H
#define MAT4_H

typedef struct {
	float m[16]; /* column-major order */
} Mat4;

Mat4 Mat4_identity(void);
Mat4 Mat4_ortho(float left, float right, float bottom, float top, float near, float far);
Mat4 Mat4_translate(float x, float y, float z);
Mat4 Mat4_rotate_z(float degrees);
Mat4 Mat4_scale(float sx, float sy, float sz);
Mat4 Mat4_multiply(const Mat4 *a, const Mat4 *b);
Mat4 Mat4_inverse(const Mat4 *m);
void Mat4_transform_point(const Mat4 *m, float in_x, float in_y, float *out_x, float *out_y);

#endif
