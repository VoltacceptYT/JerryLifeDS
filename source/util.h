// Small shared helpers: random numbers and clamping (portable C).
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

void util_seed(uint32_t s);
uint32_t util_rand32(void);

static inline float rnd01(void) { return (float)(util_rand32() >> 8) * (1.0f / 16777216.0f); }
static inline float R(float a, float b) { return a + rnd01() * (b - a); }
static inline int RI(int a, int b) { return a + (int)(rnd01() * (float)(b - a + 1)); }  // inclusive

static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

#endif
