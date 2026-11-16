// Sound effects for Jerry Life: sfx_play() (declared in jl.h) is implemented here for the DS.
// SFX_POP..SFX_BAD are up to three PSG notes played back to back on one voice. The other kinds are
// Jerry samples (assets/sfx/jerry_*.ogg -> sfxdata.c, see tools/convert_sfx.py); each kind plays a
// random sample from its family (prefix). Samples use their own hardware voice, the music keeps its
// ADPCM voice (audio.c), so effects never interrupt it.
#include <nds.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "jl.h"
#include "sfx.h"

typedef struct { u16 freq; u8 frames; } Note;
#define NOTE_MAX 3

static const Note FX[][NOTE_MAX] = {
    [SFX_POP]   = {{880, 3}, {1175, 3}, {0, 0}},
    [SFX_COIN]  = {{1319, 3}, {1760, 6}, {0, 0}},
    [SFX_HEART] = {{784, 4}, {988, 4}, {1319, 7}},
    [SFX_BAD]   = {{220, 6}, {165, 9}, {0, 0}},
};

// sample family (file name without jerry_ / .ogg, minus the variant digit) and volume per kind
static const struct { const char *prefix; u8 vol; } SMP[SFX_KINDS] = {
    [SFX_LEVEL]  = {"levelup", 110},
    [SFX_CLICK]  = {"click", 110},
    [SFX_YES]    = {"yes", 100},
    [SFX_NO]     = {"no", 100},
    [SFX_HAGGLE] = {"haggle", 100},
    [SFX_HIT]    = {"hit", 100},
    [SFX_IDLE]   = {"idle", 90},
};

static int voice = -1;
static int kind = -1, note = 0, left = 0;
static bool enabled = true;

static void stop_voice(void) {
    if (voice >= 0) { soundKill(voice); voice = -1; }
}

static void start_note(void) {
    stop_voice();
    if (kind < 0 || note >= NOTE_MAX || FX[kind][note].frames == 0) { kind = -1; return; }
    voice = soundPlayPSG(DutyCycle_50, FX[kind][note].freq, 56, 64);
    left = FX[kind][note].frames;
}

void sfx_init(void) {
    soundEnable();
    for (int i = 0; i < sfx_sample_count; i++)   // the ARM7 reads samples straight from RAM
        DC_FlushRange(sfx_samples[i].pcm, sfx_samples[i].len);
}

void sfx_set_enabled(bool on) {
    enabled = on;
    if (!on) { kind = -1; stop_voice(); }
}

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
    if (!enabled || k < 0 || k >= SFX_KINDS) return;
    if (SMP[k].prefix) {
        const SfxSample *s = pick_sample(SMP[k].prefix);
        if (s) soundPlaySample(s->pcm, SoundFormat_8Bit, s->len, (u16)sfx_sample_rate, SMP[k].vol, 64, false, 0);
        return;
    }
    kind = k;
    note = 0;
    start_note();
}

void sfx_tick(void) {
    if (kind < 0) return;
    if (--left <= 0) { note++; start_note(); }
}
