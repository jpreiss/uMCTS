#pragma once

#include <cmath>

// computing the natural logarithm is a bottleneck of UCB.
// we take advantage of fast approximate logarithm calculations.
static float const log_radix = 10e6f;
static float const log_of_radix = log2f(log_radix);

static inline float fastlog2 (float x)
{
	union { float f; uint32_t i; } vx = { x };
	union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFF) | 0x3f000000 };
	float y = vx.i;
	y *= 1.1920928955078125e-7f;

	float result = y - 124.22551499f
		- 1.498030302f * mx.f
		- 1.72587999f / (0.3520887068f + mx.f);

	return result;
}

static inline float
fastlog (float x)
{
	return 0.69314718f * (fastlog2 (x/log_radix) + log_of_radix);
}

