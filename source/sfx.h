// Sound effects: sampled sounds only (Jerry voices and the pop/coin/heart/bad effects).
#ifndef SFX_H
#include <stdbool.h>
#include <stdint.h>
#define SFX_H

// One sampled sound (8-bit signed PCM), generated into sfxdata.c by tools/convert_sfx.py.
typedef struct { const char *name; const int8_t *pcm; unsigned len; } SfxSample;
extern const SfxSample sfx_samples[];
extern const int sfx_sample_count;
extern const unsigned sfx_sample_rate;

void sfx_init(void);
void sfx_tick(void);  // call once per frame
void sfx_set_enabled(bool on);

#endif
