// Jerry Life UI: top screen = the island (scrolling camera + HUD), bottom screen = touch panels.
// Portable C (no libnds): main.c feeds in an abstract UiInput once per frame and copies the two
// framebuffers to VRAM; tools/host/ui_test.c drives the same code on the PC.
//
// Controls (all of them also work with the stylus):
//   Island view   D-pad scroll (hold Y = fast) | A use whatever is under the crosshair | B deselect
//                 L news | R items | X residents | START menu | SELECT music
//                 touch the minimap: jump there (drag to scan), tap a resident / building to use it
//   Panels        D-pad move the focus | A press | B back | L/R switch tab | Up/Down scroll lists
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

enum {
    UK_A = 1 << 0, UK_B = 1 << 1, UK_X = 1 << 2, UK_Y = 1 << 3,
    UK_L = 1 << 4, UK_R = 1 << 5, UK_START = 1 << 6, UK_SELECT = 1 << 7,
    UK_UP = 1 << 8, UK_DOWN = 1 << 9, UK_LEFT = 1 << 10, UK_RIGHT = 1 << 11,
};

typedef struct {
    uint32_t down;     // keys pressed this frame
    uint32_t held;     // keys currently held
    uint32_t repeat;   // keys pressed this frame, or auto-repeating while held (see keysSetRepeat)
    bool touchDown;    // stylus touched the screen this frame
    bool touchHeld;    // stylus is on the screen
    bool touchUp;      // stylus lifted this frame
    int tx, ty;        // last known stylus position
} UiInput;

// ---- platform hooks (implemented in main.c; stubbed by the host tests) ----
void plat_save_now(void);                 // write the save file now
void plat_set_music(bool on);             // mute / unmute the music
const char *plat_save_status(void);       // "Saved", "No SD: saving off", ...
bool plat_storage_ok(void);               // is there a working SD card / FAT?

void ui_init(void);                       // call after jl_init()/load + jl_start()
void ui_update(const UiInput *in);        // once per frame (before jl_tick)
void ui_draw_top(px *fb);                 // the island + HUD + toasts
void ui_draw_bottom(px *fb);              // panels
bool ui_bottom_dirty(void);               // something changed: redraw the bottom screen soon
void ui_mark_bottom(void);                // force a redraw (e.g. after loading a save)

#endif
