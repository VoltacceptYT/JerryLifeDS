// Sound effects for Jerry Life: sfx_play() (declared in jl.h) is implemented here for the DS.
// Every kind is a real recorded sample (assets/sfx/*.ogg -> sfxdata.c, see tools/convert_sfx.py); each
// kind plays a random sample from its family (prefix). Samples use their own hardware voice, the music keeps its
// ADPCM voice (audio.c), so effects never interrupt it.
#include <nds.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "jl.h"
#include "sfx.h"

// sample family (file name without jerry_ / .ogg, minus the variant digit) and volume per kind
static const struct { const char *prefix; u8 vol; } SMP[SFX_KINDS] = {
    [SFX_POP]    = {"pop", 110},
    [SFX_COIN]   = {"coin", 100},
    [SFX_HEART]  = {"heart", 110},
    [SFX_BAD]    = {"bad", 100},
    [SFX_LEVEL]  = {"levelup", 110},
    [SFX_CLICK]  = {"click", 110},
    [SFX_YES]    = {"yes", 100},
    [SFX_NO]     = {"no", 100},
    [SFX_HAGGLE] = {"haggle", 100},
    [SFX_HIT]    = {"hit", 100},
    [SFX_IDLE]   = {"idle", 90},
};

static bool enabled = true;

void sfx_init(void) {
    soundEnable();
    for (int i = 0; i < sfx_sample_count; i++)   // the ARM7 reads samples straight from RAM
        DC_FlushRange(sfx_samples[i].pcm, sfx_samples[i].len);
}

void sfx_set_enabled(bool on) { enabled = on; }

// pick a random sample whose name is the prefix, optionally followed by digits ("yes" -> yes1/yes2/yes3)
static const SfxSample *pick_sample(const char *prefix) {
    size_t n = strlen(prefix);
    const SfxSample *hit[8];
    int cnt = 0;
    for (int i = 0; i < sfx_sample_count && cnt < 8; i++) {
        const char *nm = sfx_samples[i].name;
        if (strncmp(nm, prefix, n) != 0) continue;
        const char *r = nm + n;
        while (*r >= '0' && *r <= '9') r++;
        if (*r == 0) hit[cnt++] = &sfx_samples[i];
    }
    return cnt ? hit[rand() % cnt] : NULL;
}

void sfx_play(int k) {
    if (!enabled || k < 0 || k >= SFX_KINDS || !SMP[k].prefix) return;
    const SfxSample *s = pick_sample(SMP[k].prefix);
    if (s) soundPlaySample(s->pcm, SoundFormat_8Bit, s->len, (u16)sfx_sample_rate, SMP[k].vol, 64, false, 0);
}

void sfx_tick(void) {}   // samples play on their own; kept so main.c's per-frame call still links
