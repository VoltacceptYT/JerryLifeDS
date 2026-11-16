#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "map.h"
#include "sprites.h"
#include "util.h"

/* ================================================================ tiles */

#define NTILES 27
static px tileBase[NTILES][256];  // season-tinted
static px tileOut[NTILES][256];   // + light tint
static int bakedSeason = -1, bakedNB = -1;
static DayTint bakedTint = {256, 256, 256};
static bool bakedAny = false;
static px mm[MM_W * MM_H] __attribute__((aligned(32)));

static const px *raw_tile(int i) {
    if (i < 13) return (const px *)ter_water[i];
    if (i < 26) return (const px *)ter_grass[i - 13];
    return (const px *)ter_sand;
}

static px mix(px v, int r, int g, int b, int a) {  // a: 0..256
    int vr = v & 31, vg = (v >> 5) & 31, vb = (v >> 10) & 31;
    vr += ((r >> 3) - vr) * a >> 8;
    vg += ((g >> 3) - vg) * a >> 8;
    vb += ((b >> 3) - vb) * a >> 8;
    return (px)(0x8000 | vr | (vg << 5) | (vb << 10));
}

static void bake_season(int season) {
    static const int T[4][4] = {{255, 170, 200, 26}, {255, 230, 120, 15}, {220, 110, 20, 56}, {235, 245, 255, 159}};
    for (int i = 0; i < NTILES; i++) {
        const px *src = raw_tile(i);
        bool water = i < 13;
        for (int k = 0; k < 256; k++) {
            px v = src[k] | 0x8000;
            if (!water) v = mix(v, T[season][0], T[season][1], T[season][2], T[season][3]);
            else if (season == SEASON_WINTER) v = mix(v, 200, 230, 255, 56);
            tileBase[i][k] = v;
        }
    }
}

static void bake_light(void) {
    for (int i = 0; i < NTILES; i++)
        for (int k = 0; k < 256; k++) tileOut[i][k] = spr_tint_px(tileBase[i][k]);
}

/* ================================================================ minimap */

static void mm_sprite(int id, int wx, int wy) {
    const SprDef *d = spr_def(id);
    int x0 = (wx - d->ax) / MM_SCALE, y0 = (wy - d->ay) / MM_SCALE;
    for (int v = 2; v < d->h; v += MM_SCALE)
        for (int u = 2; u < d->w; u += MM_SCALE) {
            px col = spr_color_at(id, u, v);
            if (!col) continue;
            int x = x0 + u / MM_SCALE, y = y0 + v / MM_SCALE;
            if (x >= 0 && y >= 0 && x < MM_W && y < MM_H) mm[y * MM_W + x] = spr_tint_px(col);
        }
}

static void bake_minimap(void) {
    for (int ty = 0; ty < GH; ty++)
        for (int tx = 0; tx < GW; tx++) {
            const px *t = tileOut[map_tile_index(tx, ty)];
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) mm[(ty * 4 + j) * MM_W + tx * 4 + i] = t[(j * 4 + 2) * 16 + i * 4 + 2];
        }
    for (int i = 0; i < map_ndecor; i++) {
        const Decor *d = &map_decor[i];
        if (d->kind == D_TREE) mm_sprite(map_tree_spr(jl.season, d->t), d->x, d->y);
        else if (d->kind == D_PALM) mm_sprite(map_spr_palm(), d->x, d->y);
        else if (d->solid) mm_sprite(d->spr, d->x, d->y);
    }
    for (int i = 0; i < map_nbuildings; i++) mm_sprite(map_buildings[i].spr, map_buildings[i].x, map_buildings[i].y);
}

const px *world_minimap(void) { return mm; }

static int tint_delta(DayTint a, DayTint b) {
    int dr = abs((int)a.r - (int)b.r), dg = abs((int)a.g - (int)b.g), db = abs((int)a.b - (int)b.b);
    return dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
}

void world_init(void) {
    bakedAny = false;
    bakedSeason = -1;
    bakedNB = -1;
}

void world_prepare(void) {
    bool redo = !bakedAny;
    bool seasonChanged = bakedSeason != jl.season || jl.seasonDirty;
    DayTint t = daycycle_tint();
    bool lightChanged = !bakedAny || tint_delta(t, bakedTint) >= 4;
    if (seasonChanged) {
        bake_season(jl.season);
        bakedSeason = jl.season;
        jl.seasonDirty = false;
    }
    if (lightChanged) {
        spr_set_tint(t);
        bakedTint = t;
    }
    if (seasonChanged || lightChanged || redo) bake_light();
    if (seasonChanged || lightChanged || redo || bakedNB != map_nbuildings) {
        bake_minimap();
        bakedNB = map_nbuildings;
    }
    bakedAny = true;
}

/* ================================================================ drawing helpers */

void fx_blend_px(Canvas *c, int x, int y, px col, int a) {
    if (x < c->cx0 || x >= c->cx1 || y < c->cy0 || y >= c->cy1) return;
    px *d = &c->buf[y * SCREEN_W + x];
    *d = mix(*d, (col & 31) << 3, ((col >> 5) & 31) << 3, ((col >> 10) & 31) << 3, a);
}

void fx_shade_rect(Canvas *c, int x, int y, int w, int h) {
    int x0 = x < c->cx0 ? c->cx0 : x, y0 = y < c->cy0 ? c->cy0 : y;
    int x1 = x + w > c->cx1 ? c->cx1 : x + w, y1 = y + h > c->cy1 ? c->cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        px *row = c->buf + yy * SCREEN_W;
        for (int xx = x0; xx < x1; xx++) row[xx] = (px)(0x8000 | ((row[xx] >> 1) & 0x3DEF));
    }
}

static void fill_ellipse_shade(Canvas *c, int cx, int cy, int rx, int ry) {  // soft ground shadow
    for (int dy = -ry; dy <= ry; dy++) {
        int yy = cy + dy;
        if (yy < c->cy0 || yy >= c->cy1) continue;
        int hw = (int)(rx * sqrtf(1.0f - (float)(dy * dy) / (float)(ry * ry + 1)) + .5f);
        for (int xx = cx - hw; xx <= cx + hw; xx++) {
            if (xx < c->cx0 || xx >= c->cx1) continue;
            px *d = &c->buf[yy * SCREEN_W + xx];
            *d = (px)(0x8000 | ((*d - ((*d >> 2) & 0x1CE7)) & 0x7FFF));
        }
    }
}

static void glow(Canvas *c, int cx, int cy, int r, px col, int a) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) fx_blend_px(c, cx + dx, cy + dy, col, a);
}

void fx_ellipse(Canvas *c, int cx, int cy, int rx, int ry, px col) {
    for (int i = 0; i < 48; i++) {
        float a = (float)i * (6.2831853f / 48.0f);
        int x = cx + (int)lroundf(cosf(a) * rx), y = cy + (int)lroundf(sinf(a) * ry);
        if (x >= c->cx0 && x < c->cx1 && y >= c->cy0 && y < c->cy1) c->buf[y * SCREEN_W + x] = col;
    }
}

void fx_outline_text_left(Canvas *c, const Font *f, int x, int y, const char *s, px fg) {
    px k = RGB8(0, 0, 0);
    gfx_text(c, f, x - 1, y, s, k, 0);
    gfx_text(c, f, x + 1, y, s, k, 0);
    gfx_text(c, f, x, y - 1, s, k, 0);
    gfx_text(c, f, x, y + 1, s, k, 0);
    gfx_text(c, f, x, y, s, fg, 0);
}

void fx_outline_text(Canvas *c, const Font *f, int cx, int y, const char *s, px fg) {
    fx_outline_text_left(c, f, cx - gfx_text_width(f, s) / 2, y, s, fg);
}

static px bit_col(char ch) {
    switch (ch) {
    case 'r': return RGB8(240, 70, 80);
    case 'p': return RGB8(255, 91, 143);
    case 'y': return RGB8(255, 216, 74);
    case 'o': return RGB8(255, 150, 40);
    case 'k': return RGB8(43, 33, 24);
    case 'w': return RGB8(255, 255, 255);
    case 'b': return RGB8(111, 214, 255);
    case 'g': return RGB8(120, 200, 100);
    case 'n': return RGB8(150, 100, 60);
    default: return 0;
    }
}

void fx_bits(Canvas *c, int x, int y, const char *const *rows, int nrows) {
    for (int j = 0; j < nrows; j++)
        for (int i = 0; rows[j][i]; i++) {
            px col = bit_col(rows[j][i]);
            if (col && x + i >= c->cx0 && x + i < c->cx1 && y + j >= c->cy0 && y + j < c->cy1)
                c->buf[(y + j) * SCREEN_W + x + i] = col;
        }
}

static const char *const B_HEART[6] = {".pp.pp.", "ppppppp", "ppppppp", ".ppppp.", "..ppp..", "...p..."};
static const char *const B_NOTE[7] = {"..kkkk.", "..k..k.", "..k..k.", "..k....", "kkk....", "kkk....", ".k....."};
static const char *const B_ANGRY[7] = {"r.r.r.r", ".r...r.", "r.....r", ".......", "r.....r", ".r...r.", "r.r.r.r"};
static const char *const B_STAR[7] = {"...y...", "...y...", "yyyyyyy", ".yyyyy.", "..yyy..", ".yy.yy.", ".y...y."};
static const char *const B_BROKEN[7] = {".rr.rr.", "rrrrrrr", "rr.rrrr", ".rrr.r.", "..rrrr.", "...rr..", "...r..."};
static const char *const B_ZZZ[7] = {"wwww...", "..w....", ".w.....", "wwww.ww", ".....w.", "....w..", "....ww."};
static const char *const B_FORK[9] = {"k.k.k.n..", "k.k.k.nn.", "kkkkk.nn.", ".kkk..nn.", "..k...n..", "..k...n..", "..k...n..", "..k...n..", "..k...n.."};
static const char *const B_SLEEP[9] = {"bbbbb....", "...b.....", "..b......", ".bbbbb.bb", "......b..", ".....b...", ".....bb.."};
static const char *const B_CHAT[9] = {"kkkkkkkkk", "k.......k", "k.k.k.k.k", "k.......k", "kkkkkkkkk", "..kk.....", "..k......"};

void fx_emote(Canvas *c, int e, int cx, int cy) {
    static const char *const *const S[6] = {B_HEART, B_NOTE, B_ANGRY, B_STAR, B_BROKEN, B_ZZZ};
    static const int N[6] = {6, 7, 7, 7, 7, 7};
    if (e < 0 || e > 5) return;
    fx_bits(c, cx - 3, cy - N[e] / 2, S[e], N[e]);
}

void fx_heart(Canvas *c, int cx, int cy, px col) {
    for (int j = 0; j < 6; j++)
        for (int i = 0; i < 7; i++)
            if (B_HEART[j][i] != '.') {
                int x = cx - 3 + i, y = cy - 3 + j;
                if (x >= c->cx0 && x < c->cx1 && y >= c->cy0 && y < c->cy1) c->buf[y * SCREEN_W + x] = col;
            }
}

void fx_concern_icon(Canvas *c, int code, int cx, int cy) {
    if (code == 1) fx_bits(c, cx - 4, cy - 4, B_FORK, 9);
    else if (code == 2) fx_bits(c, cx - 4, cy - 3, B_SLEEP, 7);
    else if (code == 3) fx_bits(c, cx - 4, cy - 3, B_CHAT, 7);
}

/* ================================================================ scene */

#define MAX_DRAW 128
typedef struct { int16_t y; uint8_t type, idx; } DrawItem;
enum { DI_DECOR, DI_BUILDING, DI_JERRY, DI_TOY };

static void draw_label_house(Canvas *c, const Building *b, int sx, int sy) {
    const Jerry *ow = NULL;
    for (int i = 0; i < jl.nHouses; i++)
        if (jl.houses[i].slot == b->slot && jl.houses[i].owner) ow = jl_jerry(jl.houses[i].owner);
    if (ow) {
        fx_outline_text(c, &fontsm, sx, sy + 4, ow->name, RGB8(255, 255, 255));
        if (ow->inside) {
            int zz = (int)(sinf(jl.clock * 2) * 3);
            fx_emote(c, EM_ZZZ, sx + zz, sy - spr_def(b->spr)->ay + 18);
        }
    } else {
        fx_outline_text(c, &fontsm, sx, sy + 4, "Vacant", RGB8(153, 255, 153));
    }
}

static void draw_bubble(Canvas *c, const Jerry *j, int sx, int sy) {
    int x = sx, y = sy - 38;
    if (j->bubKind == BUB_EMOTE) {
        fx_emote(c, j->bubText[0], x, y - 4);
        return;
    }
    // word wrap at 16 chars
    char lines[4][20];
    int nl = 1;
    lines[0][0] = 0;
    const char *s = j->bubText;
    while (*s) {
        char word[40];
        int n = 0;
        while (*s && *s != ' ' && n < 39) word[n++] = *s++;
        word[n] = 0;
        while (*s == ' ') s++;
        char *L = lines[nl - 1];
        int len = (int)strlen(L);
        if (len && len + 1 + n > 16 && nl < 4) { nl++; L = lines[nl - 1]; L[0] = 0; len = 0; }
        if (len) strcat(L, " ");
        strncat(L, word, 19 - strlen(L));
    }
    int mw = 0;
    for (int i = 0; i < nl; i++) mw = maxi(mw, gfx_text_width(&fontsm, lines[i]));
    int w = mw + 8, h = nl * 9 + 5;
    int bx = clampi(x - w / 2, 2, SCREEN_W - w - 2), by = y - h - 3;
    px fill = RGB8(255, 251, 232), edge = RGB8(58, 47, 38);
    gfx_fill(c, bx, by, w, h, fill);
    gfx_frame(c, bx, by, w, h, edge);
    gfx_fill(c, x - 2, by + h, 5, 1, fill);
    gfx_fill(c, x - 1, by + h + 1, 3, 1, fill);
    gfx_fill(c, x, by + h + 2, 1, 1, fill);
    for (int i = 0; i < nl; i++) gfx_text_centered(c, &fontsm, bx + w / 2, by + 3 + i * 9, lines[i], RGB8(43, 33, 24), 0);
}

static void draw_jerry(Canvas *c, const Jerry *j, int camX, int camY, bool selected) {
    int sx = (int)lroundf(j->x) - camX, sy = (int)lroundf(j->y) - camY;
    float bob = (j->st == ST_WALK || j->st == ST_HOME || j->st == ST_PLAY) ? fabsf(sinf(jl.clock * 9 + (float)j->id)) * 2 : 0;
    float jump = (j->st == ST_CHAT || j->st == ST_EAT) ? fabsf(sinf(jl.clock * 7)) * 2 : 0;
    int lift = (int)(bob + jump);
    fill_ellipse_shade(c, sx, sy, 9, 3);
    if (selected) fx_ellipse(c, sx, sy, 12, 5, RGB8(255, 216, 74));
    bool flip = j->dir < 0;
    spr_draw(c, SKINS[j->skin].spr, sx, sy - lift, 2, flip, true);
    if (j->held >= 0 && j->held < GIFT_COUNT) {
        const SprDef *g = spr_def(GIFTS[j->held].spr);
        int left = flip ? sx - 8 - g->w : sx + 8;
        spr_draw(c, GIFTS[j->held].spr, left + g->ax, sy - lift - 20 + g->ay, 1, false, true);
    }
    if (j->angry) fx_emote(c, EM_ANGRY, sx + 10, sy - 36);
    fx_outline_text(c, &fontsm, sx, sy + 4, j->name, RGB8(255, 255, 255));
    if (j->conType != CON_NONE && j->bubKind == BUB_NONE) {
        int yy = sy - 40 + (int)(sinf(jl.clock * 4) * 2);
        gfx_fill(c, sx - 9, yy - 14, 18, 18, RGB8(255, 251, 232));
        gfx_frame(c, sx - 9, yy - 14, 18, 18, RGB8(58, 47, 38));
        if (j->conType == CON_WANT_FOOD) spr_icon(c, FOODS[j->conId].spr, sx - 8, yy - 13);
        else if (j->conType == CON_WANT_GIFT) spr_icon(c, GIFTS[j->conId].spr, sx - 8, yy - 13);
        else fx_concern_icon(c, j->conType == CON_HUNGRY ? 1 : j->conType == CON_BORED ? 2 : 3, sx, yy - 5);
    }
    if (j->bubKind != BUB_NONE) draw_bubble(c, j, sx, sy);
}

static void draw_toy(Canvas *c, int ti, int camX, int camY) {
    const ToyW *t = &jl.toys[ti];
    int sx = (int)lroundf(t->x) - camX, sy = (int)lroundf(t->y) - camY;
    float z = ti == TOY_BALLOON_SNAKE ? 16 + sinf(t->ph * 2) * 3 : fabsf(sinf(t->hop * 6)) * t->hop * 10;
    fill_ellipse_shade(c, sx, sy, 6, 2);
    spr_draw(c, TOYS[ti].spr, sx, sy - (int)z, 1, false, true);
}

static bool on_screen(int sx, int sy, int l, int r, int u, int d) {
    return sx + r > 0 && sx - l < SCREEN_W && sy + d > 0 && sy - u < SCREEN_H;
}

static void insert_item(DrawItem *list, int *n, DrawItem it) {
    if (*n >= MAX_DRAW) return;
    int i = (*n)++;
    while (i > 0 && list[i - 1].y > it.y) { list[i] = list[i - 1]; i--; }
    list[i] = it;
}

static px flake_color(int i) {
    static const px FALL[3] = {RGB8(216, 115, 28), RGB8(200, 69, 28), RGB8(232, 176, 32)};
    return jl.season == SEASON_WINTER ? RGB8(255, 255, 255) : jl.season == SEASON_FALL ? FALL[i % 3] : RGB8(255, 182, 208);
}

static void draw_flakes(Canvas *c) {
    int n = jl.season == SEASON_WINTER ? 34 : jl.season == SEASON_FALL ? 12 : jl.season == SEASON_SPRING ? 11 : 0;
    for (int i = 0; i < n; i++) {
        uint32_t h = (uint32_t)(i + 1) * 2654435761u;
        float vx = jl.season == SEASON_WINTER ? ((int)(h & 15) - 8) * 1.2f : 10 + (float)((h >> 4) & 15);
        float vy = jl.season == SEASON_WINTER ? 14 + (float)((h >> 8) & 15) : 16 + (float)((h >> 8) & 15);
        float x = fmodf((float)((h >> 12) & 255) + vx * jl.clock + (jl.season == SEASON_WINTER ? sinf(jl.clock + (float)i) * 6 : 0), 256.0f);
        float y = fmodf((float)((h >> 20) & 255) * .75f + vy * jl.clock, 192.0f);
        if (x < 0) x += 256;
        int s = jl.season == SEASON_WINTER ? 2 : 3;
        gfx_fill(c, (int)x, (int)y, s, s, flake_color(i));
    }
}

void world_draw(Canvas *c, int camX, int camY) {
    camX = clampi(camX, 0, PW - VIEW_W);
    camY = clampi(camY, 0, PH - VIEW_H);
    // terrain
    int tx0 = camX / 16, ty0 = camY / 16;
    for (int ty = ty0; ty < GH && ty * 16 - camY < VIEW_H; ty++) {
        int y0 = ty * 16 - camY;
        for (int tx = tx0; tx < GW && tx * 16 - camX < VIEW_W; tx++) {
            const px *t = tileOut[map_tile_index(tx, ty)];
            int x0 = tx * 16 - camX;
            int xs = x0 < 0 ? -x0 : 0, xe = x0 + 16 > VIEW_W ? VIEW_W - x0 : 16;
            if (xe <= xs) continue;
            for (int j = 0; j < 16; j++) {
                int yy = y0 + j;
                if (yy < 0 || yy >= VIEW_H) continue;
                memcpy(c->buf + yy * SCREEN_W + x0 + xs, t + j * 16 + xs, (size_t)(xe - xs) * 2);
            }
        }
    }
    // collectible sparkles
    for (int i = 0; i < MAX_SPARKS; i++) {
        const Spark *s = &jl.sparks[i];
        if (!s->on) continue;
        int sx = (int)s->x - camX, sy = (int)s->y - camY;
        if (!on_screen(sx, sy, 16, 16, 16, 16)) continue;
        float a = .6f + .4f * sinf(jl.clock * 5 + s->t);
        glow(c, sx, sy - 4, (int)(9 + sinf(jl.clock * 5 + s->t) * 2), RGB8(255, 248, 176), (int)(a * 90));
        spr_draw(c, s->spr, sx, sy, 1, false, true);
    }
    // flat decoration, then the depth-sorted objects
    DrawItem list[MAX_DRAW];
    int n = 0;
    for (int i = 0; i < map_ndecor; i++) {
        const Decor *d = &map_decor[i];
        int sx = d->x - camX, sy = d->y - camY;
        int spr = d->kind == D_TREE ? map_tree_spr(jl.season, d->t) : d->kind == D_PALM ? map_spr_palm() : d->spr;
        const SprDef *r = spr_def(spr);
        if (!on_screen(sx, sy, r->ax, r->w - r->ax, r->ay, r->h - r->ay)) continue;
        if (d->kind == D_OBJ && !d->solid) { spr_draw(c, spr, sx, sy, 1, false, true); continue; }
        insert_item(list, &n, (DrawItem){(int16_t)d->y, DI_DECOR, (uint8_t)i});
    }
    for (int i = 0; i < map_nbuildings; i++) {
        const Building *b = &map_buildings[i];
        const SprDef *r = spr_def(b->spr);
        if (on_screen(b->x - camX, b->y - camY, r->ax + 20, r->w - r->ax + 20, r->ay, r->h - r->ay + 12))
            insert_item(list, &n, (DrawItem){(int16_t)b->y, DI_BUILDING, (uint8_t)i});
    }
    for (int i = 0; i < jl.nJerries; i++) {
        const Jerry *j = &jl.jerries[i];
        if (!j->inside && on_screen((int)j->x - camX, (int)j->y - camY, 40, 40, 70, 14))
            insert_item(list, &n, (DrawItem){(int16_t)j->y, DI_JERRY, (uint8_t)i});
    }
    for (int i = 0; i < TOY_COUNT; i++)
        if (jl.toys[i].on && on_screen((int)jl.toys[i].x - camX, (int)jl.toys[i].y - camY, 12, 12, 40, 6))
            insert_item(list, &n, (DrawItem){(int16_t)jl.toys[i].y, DI_TOY, (uint8_t)i});

    for (int k = 0; k < n; k++) {
        const DrawItem *it = &list[k];
        if (it->type == DI_DECOR) {
            const Decor *d = &map_decor[it->idx];
            int spr = d->kind == D_TREE ? map_tree_spr(jl.season, d->t) : d->kind == D_PALM ? map_spr_palm() : d->spr;
            spr_draw(c, spr, d->x - camX, d->y - camY, 1, false, true);
        } else if (it->type == DI_BUILDING) {
            const Building *b = &map_buildings[it->idx];
            int sx = b->x - camX, sy = b->y - camY;
            spr_draw(c, b->spr, sx, sy, 1, false, true);
            if (b->type == BT_HOUSE) draw_label_house(c, b, sx, sy);
            else fx_outline_text(c, &fontsm, sx, sy + 5, b->name, RGB8(255, 216, 74));
        } else if (it->type == DI_JERRY) {
            const Jerry *j = &jl.jerries[it->idx];
            draw_jerry(c, j, camX, camY, jl.selected == j->id);
        } else {
            draw_toy(c, it->idx, camX, camY);
        }
    }
    // particles and floating numbers
    for (int i = 0; i < MAX_PARTS; i++) {
        const Part *p = &jl.parts[i];
        if (!p->on) continue;
        int sx = (int)p->x - camX, sy = (int)p->y - camY;
        if (p->kind == PK_HEART) {
            if (p->l > .35f || ((int)(jl.clock * 30) & 1)) fx_heart(c, sx, sy, RGB8(255, 91, 143));
        } else if (p->kind == PK_CONF) {
            gfx_fill(c, sx, sy, 2, 2, p->col);
        } else if (p->kind == PK_ANGRY) {
            fx_emote(c, EM_ANGRY, sx, sy);
        }
    }
    draw_flakes(c);
    for (int i = 0; i < MAX_POPUPS; i++) {
        const Popup *p = &jl.popups[i];
        if (!p->on) continue;
        if (p->l > .5f || ((int)(jl.clock * 30) & 1)) fx_outline_text(c, &fontsm, (int)p->x - camX, (int)p->y - camY, p->text, p->col);
    }
}
