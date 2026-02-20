#ifndef NOISE_H
#define NOISE_H

#include <stdint.h>

/* Single-evaluation 2D simplex noise. Returns [-1.0, 1.0]. */
double Noise_simplex2d(double x, double y);

/* Multi-octave fractal Brownian motion. Returns [-1.0, 1.0] (normalized).
 * seed offsets each octave for per-seed uniqueness. */
double Noise_fbm(double x, double y, int octaves, double frequency,
                 double lacunarity, double persistence, uint32_t seed);

#endif
