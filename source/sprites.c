#include "sprites.h"

#include <string.h>

static uint16_t lutR[32], lutG[32], lutB[32];
static bool tintOn = false;
static int tintVersion = 1;

const SprDef *spr_def(int id) { return &spr_defs[id]; }

int spr_find(const char *name) {  // spr_names is sorted
    int lo = 0, hi = SPR_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int d = strcmp(name, spr_names[mid]);
        if (d == 0) return mid;
        if (d < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

static void build_lut(uint16_t *lut, int mul, int shift) {
    for (int i = 0; i < 32; i++) {
        int v = (i * mul + 128) >> 8;
        lut[i] = (uint16_t)((v > 31 ? 31 : v) << shift);
    }
}

void spr_set_tint(DayTint t) {
    tintOn = !(t.r >= 256 && t.g >= 256 && t.b >= 256);
    if (tintOn) {
        build_lut(lutR, t.r, 0);
        build_lut(lutG, t.g, 5);
        build_lut(lutB, t.b, 10);
    }
    tintVersion++;
}

bool spr_tint_active(void) { return tintOn; }

px spr_tint_px(px v) {
    if (!tintOn) return v;
    return (px)(0x8000 | lutR[v & 31] | lutG[(v >> 5) & 31] | lutB[(v >> 10) & 31]);
}

static inline const uint16_t *pal_of(const SprDef *d) { return (const uint16_t *)(atlas_blob + d->off); }
static inline const uint8_t *pix_of(const SprDef *d) { return atlas_blob + d->off + d->palN * 2u; }

// Palette cache: a sprite's palette (optionally tinted) is expanded once and reused while the same
// sprite is drawn again with the same light.
static px palBuf[256];
static int palId = -1, palVer = -1;
static bool palTinted;

static void load_pal(int id, bool tinted) {
    tinted = tinted && tintOn;
    if (palId == id && palVer == tintVersion && palTinted == tinted) return;
    const SprDef *d = &spr_defs[id];
    const uint16_t *p = pal_of(d);
    palBuf[0] = 0;
    for (int i = 0; i < d->palN; i++) {
        px v = (px)(p[i] | 0x8000);
        palBuf[i + 1] = tinted ? spr_tint_px(v) : v;
    }
    palId = id;
    palVer = tintVersion;
    palTinted = tinted;
}

static inline px shade(px d) {  // soft shadow: darken what is underneath by about a third
    return (px)(0x8000 | ((d - ((d >> 2) & 0x1CE7)) & 0x7FFF));
}

void spr_draw(Canvas *c, int id, int x, int y, int scale, bool flip, bool tinted) {
    if (id < 0) return;
    const SprDef *d = &spr_defs[id];
    int w = d->w, h = d->h;
    int ox = scale * d->ax, oy = scale * d->ay;
    int left = flip ? x - scale * (w - d->ax) : x - ox;
    int top = y - oy;
    if (left >= c->cx1 || top >= c->cy1 || left + scale * w <= c->cx0 || top + scale * h <= c->cy0) return;
    load_pal(id, tinted);
    const uint8_t *pix = pix_of(d);
    for (int sy = 0; sy < h; sy++) {
        int py0 = top + sy * scale;
        if (py0 >= c->cy1 || py0 + scale <= c->cy0) continue;
        const uint8_t *row = pix + sy * w;
        for (int sx = 0; sx < w; sx++) {
            uint8_t i = row[flip ? w - 1 - sx : sx];
            if (!i) continue;
            int px0 = left + sx * scale;
            for (int dy = 0; dy < scale; dy++) {
                int yy = py0 + dy;
                if (yy < c->cy0 || yy >= c->cy1) continue;
                px *dst = c->buf + yy * SCREEN_W;
                for (int dx = 0; dx < scale; dx++) {
                    int xx = px0 + dx;
                    if (xx < c->cx0 || xx >= c->cx1) continue;
                    dst[xx] = i == 255 ? shade(dst[xx]) : palBuf[i];
                }
            }
        }
    }
}

void spr_icon(Canvas *c, int id, int x, int y) {
    if (id < 0) return;
    const SprDef *d = &spr_defs[id];
    spr_draw(c, id, x + d->ax, y + d->ay, 1, false, false);
}

px spr_color_at(int id, int u, int v) {
    const SprDef *d = &spr_defs[id];
    if (u < 0 || v < 0 || u >= d->w || v >= d->h) return 0;
    uint8_t i = pix_of(d)[v * d->w + u];
    if (i == 0 || i == 255) return 0;
    return (px)(pal_of(d)[i - 1] | 0x8000);
}
