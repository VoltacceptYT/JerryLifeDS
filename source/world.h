// Top-screen world renderer + minimap + small drawing effects shared with the UI.
#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>

#include "gfx.h"
#include "jl.h"

#define VIEW_W SCREEN_W
#define VIEW_H SCREEN_H
#define MM_W 256              // minimap = whole island at 1/4 scale
#define MM_H 160
#define MM_SCALE 4

void world_init(void);
// Call every frame (cheap): rebuilds terrain/minimap caches after a season, light or layout change.
void world_prepare(void);
void world_draw(Canvas *c, int camX, int camY);
// Minimap: static terrain/buildings layer; dynamic dots are drawn on top by the UI.
const px *world_minimap(void);

// ---- little helpers ----
void fx_outline_text(Canvas *c, const Font *f, int cx, int y, const char *s, px fg);   // centred, black outline
void fx_outline_text_left(Canvas *c, const Font *f, int x, int y, const char *s, px fg);
void fx_blend_px(Canvas *c, int x, int y, px col, int a256);
void fx_shade_rect(Canvas *c, int x, int y, int w, int h);                            // darken a region (HUD plates)
void fx_ellipse(Canvas *c, int cx, int cy, int rx, int ry, px col);                   // outline
void fx_bits(Canvas *c, int x, int y, const char *const *rows, int nrows);             // tiny coloured bitmap
void fx_emote(Canvas *c, int emote, int cx, int cy);                                   // centred on (cx, cy)
void fx_concern_icon(Canvas *c, int code, int cx, int cy);                             // jl_concern_text() codes 1..3
void fx_heart(Canvas *c, int cx, int cy, px col);

#endif
