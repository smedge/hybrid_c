#include "prng.h"

/* SplitMix32 — expands a single seed into 4-word state */
static uint32_t splitmix32(uint32_t *state)
{
	uint32_t z = (*state += 0x9E3779B9u);
	z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
	z = (z ^ (z >> 13)) * 0xC2B2AE35u;
	return z ^ (z >> 16);
}

static uint32_t rotl(uint32_t x, int k)
{
	return (x << k) | (x >> (32 - k));
}

void Prng_seed(Prng *rng, uint32_t seed)
{
	uint32_t sm = seed;
	rng->state[0] = splitmix32(&sm);
	rng->state[1] = splitmix32(&sm);
	rng->state[2] = splitmix32(&sm);
	rng->state[3] = splitmix32(&sm);
}

/* xoshiro128** */
uint32_t Prng_next(Prng *rng)
{
	uint32_t *s = rng->state;
	uint32_t result = rotl(s[1] * 5, 7) * 9;
	uint32_t t = s[1] << 9;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;
	s[3] = rotl(s[3], 11);

	return result;
}

int Prng_range(Prng *rng, int min, int max)
{
	if (min >= max) return min;
	uint32_t range = (uint32_t)(max - min + 1);
	return min + (int)(Prng_next(rng) % range);
}

float Prng_float(Prng *rng)
{
	return (Prng_next(rng) >> 8) / 16777216.0f;
}

double Prng_double(Prng *rng)
{
	return (Prng_next(rng) >> 8) / 16777216.0;
}
