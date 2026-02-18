#ifndef PARTICLE_INSTANCE_H
#define PARTICLE_INSTANCE_H

#include <stdbool.h>
#include "mat4.h"

typedef struct {
	float x, y;        /* world position */
	float size;         /* quad half-extent */
	float rotation;     /* degrees */
	float stretch;      /* elongation along local X (1.0 = square) */
	float r, g, b, a;  /* color */
} ParticleInstanceData;

void ParticleInstance_initialize(void);  /* lazy, auto-called on first draw */
void ParticleInstance_cleanup(void);
void ParticleInstance_draw(const ParticleInstanceData *data, int count,
	const Mat4 *projection, const Mat4 *view, bool soft_circle);

#endif
