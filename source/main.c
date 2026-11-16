// Jerry Life for Nintendo DS: platform glue (video, input, saving, main loop).
// All game logic lives in jl.c / ui.c / world.c (portable C); this file only feeds them input
// once per frame and copies the two software-rendered 256x192 framebuffers to VRAM.
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "daycycle.h"
#include "gfx.h"
#include "jl.h"
#include "sfx.h"
#include "ui.h"
#include "util.h"
#include "world.h"

#define SAVE_PATH "JerryLife.sav"
#define AUTOSAVE_FRAMES (60 * 60)
#define FB_BYTES (SCREEN_W * SCREEN_H * 2)

static px fbTop[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static px fbBot[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static u16 *vramTop, *vramBot;

static bool fatOk = false;
static char saveStatus[32] = "";
static uint8_t saveBuf[4096];

/* ---------------- platform hooks used by ui.c ---------------- */

bool plat_storage_ok(void) { return fatOk; }
const char *plat_save_status(void) { return saveStatus; }

void plat_set_music(bool on) {
    if (audio_music_playing() && audio_music_audible() != on) audio_music_toggle();
    sfx_set_enabled(on);
}

void plat_save_now(void) {
    if (!fatOk) {
        snprintf(saveStatus, sizeof saveStatus, "No SD: saving off");
        return;
    }
    size_t n = jl_pack(saveBuf, sizeof saveBuf);
    if (n == 0) {
        snprintf(saveStatus, sizeof saveStatus, "Save too big");
        return;
    }
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) {
        snprintf(saveStatus, sizeof saveStatus, "Save failed");
        return;
    }
    size_t w = fwrite(saveBuf, 1, n, f);
    fclose(f);
    snprintf(saveStatus, sizeof saveStatus, w == n ? "Saved" : "Save failed");
}

static bool load_save(void) {
    if (!fatOk) {
        snprintf(saveStatus, sizeof saveStatus, "No SD: saving off");
        return false;
    }
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) {
        snprintf(saveStatus, sizeof saveStatus, "New game");
        return false;
    }
    size_t n = fread(saveBuf, 1, sizeof saveBuf, f);
    fclose(f);
    if (!jl_unpack(saveBuf, n)) {
        snprintf(saveStatus, sizeof saveStatus, "Bad save ignored");
        return false;
    }
    snprintf(saveStatus, sizeof saveStatus, "Save loaded");
    return true;
}

/* ---------------- setup ---------------- */

static void setup_video(void) {
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    int bgT = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgB = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramTop = bgGetGfxPtr(bgT);
    vramBot = bgGetGfxPtr(bgB);
}

static void blit(const px *src, u16 *dst) {
    DC_FlushRange(src, FB_BYTES);
    dmaCopy(src, dst, FB_BYTES);
}

static uint32_t map_keys(uint32_t k) {
    uint32_t u = 0;
    if (k & KEY_A) u |= UK_A;
    if (k & KEY_B) u |= UK_B;
    if (k & KEY_X) u |= UK_X;
    if (k & KEY_Y) u |= UK_Y;
    if (k & KEY_L) u |= UK_L;
    if (k & KEY_R) u |= UK_R;
    if (k & KEY_START) u |= UK_START;
    if (k & KEY_SELECT) u |= UK_SELECT;
    if (k & KEY_UP) u |= UK_UP;
    if (k & KEY_DOWN) u |= UK_DOWN;
    if (k & KEY_LEFT) u |= UK_LEFT;
    if (k & KEY_RIGHT) u |= UK_RIGHT;
    return u;
}

int main(void) {
    setup_video();
    keysSetRepeat(15, 4);
    util_seed((uint32_t)time(NULL));

    fatOk = fatInitDefault();
    jl_init();
    load_save();          // leaves the fresh game from jl_init() in place when there is no save
    world_init();
    jl_start();
    sfx_init();
    audio_init();
    plat_set_music(jl.musicOn);
    ui_init();

    UiInput in;
    memset(&in, 0, sizeof in);
    u32 frame = 0, sinceSave = 0;
    bool readyTop = false, readyBot = false, copyBotNext = false;

    while (1) {
        swiWaitForVBlank();

        // Copy at most one framebuffer per vblank (a full copy eats a good part of it).
        if (readyBot && (!readyTop || copyBotNext)) {
            blit(fbBot, vramBot);
            readyBot = false;
            copyBotNext = false;
        } else if (readyTop) {
            blit(fbTop, vramTop);
            readyTop = false;
            copyBotNext = true;
        }

        scanKeys();
        u32 down = keysDown(), held = keysHeld(), up = keysUp(), rep = keysDownRepeat();
        in.down = map_keys(down);
        in.held = map_keys(held);
        in.repeat = map_keys(rep);
        in.touchDown = (down & KEY_TOUCH) != 0;
        in.touchHeld = (held & KEY_TOUCH) != 0;
        in.touchUp = (up & KEY_TOUCH) != 0;
        if (in.touchHeld) {
            touchPosition t;
            touchRead(&t);
            in.tx = t.px;
            in.ty = t.py;
        }

        if (frame % 60 == 0) daycycle_update();   // follow the DS real-time clock

        ui_update(&in);
        jl_tick();
        sfx_tick();

        if (++sinceSave >= AUTOSAVE_FRAMES) {
            sinceSave = 0;
            plat_save_now();
            ui_mark_bottom();
        }

        frame++;
        if (!readyTop) {
            ui_draw_top(fbTop);
            readyTop = true;
        }
        if ((ui_bottom_dirty() || frame % 30 == 0) && !readyBot) {
            ui_draw_bottom(fbBot);
            readyBot = true;
        }
    }
    return 0;
}
