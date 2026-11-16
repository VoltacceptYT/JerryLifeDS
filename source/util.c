#include "util.h"

static uint32_t state = 0x9E3779B9u;

void util_seed(uint32_t s) { state = s ? s : 0x9E3779B9u; }

uint32_t util_rand32(void) {  // xorshift32
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state = x;
}
