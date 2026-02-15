#include "mat4.h"
#include <math.h>
#include <string.h>

/* Column-major layout:
   m[0] m[4] m[8]  m[12]
   m[1] m[5] m[9]  m[13]
   m[2] m[6] m[10] m[14]
   m[3] m[7] m[11] m[15] */

Mat4 Mat4_identity(void)
{
	Mat4 r;
	memset(r.m, 0, sizeof(r.m));
	r.m[0] = 1.0f;
	r.m[5] = 1.0f;
	r.m[10] = 1.0f;
	r.m[15] = 1.0f;
	return r;
}

Mat4 Mat4_ortho(float left, float right, float bottom, float top,
	float near, float far)
{
	Mat4 r;
	memset(r.m, 0, sizeof(r.m));
	r.m[0]  =  2.0f / (right - left);
	r.m[5]  =  2.0f / (top - bottom);
	r.m[10] = -2.0f / (far - near);
	r.m[12] = -(right + left) / (right - left);
	r.m[13] = -(top + bottom) / (top - bottom);
	r.m[14] = -(far + near) / (far - near);
	r.m[15] =  1.0f;
	return r;
}

Mat4 Mat4_translate(float x, float y, float z)
{
	Mat4 r = Mat4_identity();
	r.m[12] = x;
	r.m[13] = y;
	r.m[14] = z;
	return r;
}

Mat4 Mat4_rotate_z(float degrees)
{
	float rad = degrees * (float)(M_PI / 180.0);
	float c = cosf(rad);
	float s = sinf(rad);
	Mat4 r = Mat4_identity();
	r.m[0] = c;
	r.m[1] = s;
	r.m[4] = -s;
	r.m[5] = c;
	return r;
}

Mat4 Mat4_scale(float sx, float sy, float sz)
{
	Mat4 r;
	memset(r.m, 0, sizeof(r.m));
	r.m[0]  = sx;
	r.m[5]  = sy;
	r.m[10] = sz;
	r.m[15] = 1.0f;
	return r;
}

Mat4 Mat4_multiply(const Mat4 *a, const Mat4 *b)
{
	Mat4 r;
	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			float sum = 0.0f;
			for (int k = 0; k < 4; k++) {
				sum += a->m[k * 4 + row] * b->m[col * 4 + k];
			}
			r.m[col * 4 + row] = sum;
		}
	}
	return r;
}

Mat4 Mat4_inverse(const Mat4 *m)
{
	/* General 4x4 matrix inverse using cofactor expansion */
	const float *s = m->m;
	float inv[16];

	inv[0]  =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15]
	          + s[9]*s[7]*s[14] + s[13]*s[6]*s[11] - s[13]*s[7]*s[10];
	inv[4]  = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15]
	          - s[8]*s[7]*s[14] - s[12]*s[6]*s[11] + s[12]*s[7]*s[10];
	inv[8]  =  s[4]*s[9]*s[15] - s[4]*s[11]*s[13] - s[8]*s[5]*s[15]
	          + s[8]*s[7]*s[13] + s[12]*s[5]*s[11] - s[12]*s[7]*s[9];
	inv[12] = -s[4]*s[9]*s[14] + s[4]*s[10]*s[13] + s[8]*s[5]*s[14]
	          - s[8]*s[6]*s[13] - s[12]*s[5]*s[10] + s[12]*s[6]*s[9];
	inv[1]  = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14] + s[9]*s[2]*s[15]
	          - s[9]*s[3]*s[14] - s[13]*s[2]*s[11] + s[13]*s[3]*s[10];
	inv[5]  =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14] - s[8]*s[2]*s[15]
	          + s[8]*s[3]*s[14] + s[12]*s[2]*s[11] - s[12]*s[3]*s[10];
	inv[9]  = -s[0]*s[9]*s[15] + s[0]*s[11]*s[13] + s[8]*s[1]*s[15]
	          - s[8]*s[3]*s[13] - s[12]*s[1]*s[11] + s[12]*s[3]*s[9];
	inv[13] =  s[0]*s[9]*s[14] - s[0]*s[10]*s[13] - s[8]*s[1]*s[14]
	          + s[8]*s[2]*s[13] + s[12]*s[1]*s[10] - s[12]*s[2]*s[9];
	inv[2]  =  s[1]*s[6]*s[15] - s[1]*s[7]*s[14] - s[5]*s[2]*s[15]
	          + s[5]*s[3]*s[14] + s[13]*s[2]*s[7] - s[13]*s[3]*s[6];
	inv[6]  = -s[0]*s[6]*s[15] + s[0]*s[7]*s[14] + s[4]*s[2]*s[15]
	          - s[4]*s[3]*s[14] - s[12]*s[2]*s[7] + s[12]*s[3]*s[6];
	inv[10] =  s[0]*s[5]*s[15] - s[0]*s[7]*s[13] - s[4]*s[1]*s[15]
	          + s[4]*s[3]*s[13] + s[12]*s[1]*s[7] - s[12]*s[3]*s[5];
	inv[14] = -s[0]*s[5]*s[14] + s[0]*s[6]*s[13] + s[4]*s[1]*s[14]
	          - s[4]*s[2]*s[13] - s[12]*s[1]*s[6] + s[12]*s[2]*s[5];
	inv[3]  = -s[1]*s[6]*s[11] + s[1]*s[7]*s[10] + s[5]*s[2]*s[11]
	          - s[5]*s[3]*s[10] - s[9]*s[2]*s[7] + s[9]*s[3]*s[6];
	inv[7]  =  s[0]*s[6]*s[11] - s[0]*s[7]*s[10] - s[4]*s[2]*s[11]
	          + s[4]*s[3]*s[10] + s[8]*s[2]*s[7] - s[8]*s[3]*s[6];
	inv[11] = -s[0]*s[5]*s[11] + s[0]*s[7]*s[9] + s[4]*s[1]*s[11]
	          - s[4]*s[3]*s[9] - s[8]*s[1]*s[7] + s[8]*s[3]*s[5];
	inv[15] =  s[0]*s[5]*s[10] - s[0]*s[6]*s[9] - s[4]*s[1]*s[10]
	          + s[4]*s[2]*s[9] + s[8]*s[1]*s[6] - s[8]*s[2]*s[5];

	float det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];

	Mat4 r;
	if (fabsf(det) < 1e-6f) {
		return Mat4_identity();
	}

	float inv_det = 1.0f / det;
	for (int i = 0; i < 16; i++)
		r.m[i] = inv[i] * inv_det;

	return r;
}

void Mat4_transform_point(const Mat4 *m, float in_x, float in_y,
	float *out_x, float *out_y)
{
	*out_x = m->m[0]*in_x + m->m[4]*in_y + m->m[12];
	*out_y = m->m[1]*in_x + m->m[5]*in_y + m->m[13];
}
