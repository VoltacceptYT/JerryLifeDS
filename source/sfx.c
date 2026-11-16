// Sound effects for Jerry Life: sfx_play() (declared in jl.h) is implemented here for the DS.
// Each effect is up to three notes; one PSG voice plays them back to back. The music keeps its
// own ADPCM voice (audio.c), so effects never interrupt it.
#include <nds.h>
#include <stdbool.h>

#include "jl.h"
#include "sfx.h"

typedef struct { u16 freq; u8 frames; } Note;
#define NOTE_MAX 3

static const Note FX[][NOTE_MAX] = {
    [SFX_POP]   = {{880, 3}, {1175, 3}, {0, 0}},
    [SFX_COIN]  = {{1319, 3}, {1760, 6}, {0, 0}},
    [SFX_HEART] = {{784, 4}, {988, 4}, {1319, 7}},
    [SFX_BAD]   = {{220, 6}, {165, 9}, {0, 0}},
    [SFX_LEVEL] = {{523, 4}, {659, 4}, {1047, 10}},
};

extern const int8_t click_pcm[];
extern const unsigned click_pcm_len, click_pcm_rate;
#define CLICK_VOLUME 110

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
    DC_FlushRange(click_pcm, click_pcm_len); // the ARM7 reads the sample straight from RAM
}

void sfx_set_enabled(bool on) {
    enabled = on;
    if (!on) { kind = -1; stop_voice(); }
}

void sfx_play(int k) {
    if (!enabled || k < 0 || k > SFX_CLICK) return;
    if (k == SFX_CLICK) { // sampled effect on its own hardware voice; doesn't disturb the PSG blips
        soundPlaySample(click_pcm, SoundFormat_8Bit, click_pcm_len, (u16)click_pcm_rate, CLICK_VOLUME, 64, false, 0);
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
