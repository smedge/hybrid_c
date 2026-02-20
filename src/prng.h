#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

typedef struct {
	uint32_t state[4];
} Prng;

void     Prng_seed(Prng *rng, uint32_t seed);
uint32_t Prng_next(Prng *rng);
int      Prng_range(Prng *rng, int min, int max);
float    Prng_float(Prng *rng);
double   Prng_double(Prng *rng);

#endif
