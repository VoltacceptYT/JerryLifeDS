// Island terrain, buildings, decoration and collision (ported from Jerry Life's buildMap / layout).
#include "map.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sprites.h"
#include "util.h"

Building map_buildings[MAX_BUILDINGS];
int map_nbuildings;
Decor map_decor[MAX_DECOR];
int map_ndecor;

const int MAP_SLOTS[MAX_HOUSES][2] = {{260, 455}, {370, 475}, {480, 455}, {590, 475}, {690, 450}, {165, 440}};
const int HOUSE_COST[MAX_HOUSES] = {0, 0, 0, 150, 300, 500};

static uint8_t grid[GH][GW];   // K_*
static uint8_t walkable[GH][GW];
static Rect collide[MAX_COLLIDE];
static int ncollide;

/* sprite ids, resolved once */
static int sprHouse[2][7], sprHouseW[2][7];
static int sprTree[4][3], sprPalm;
static int sprFixed[5], sprFixedW[5];
static int sprObjGrass[8], sprObjSand[7];

typedef struct { int type; int x, y; const char *key, *wk, *name; } Fixed;
static const Fixed FIXED[5] = {
    {BT_HALL, 512, 275, "b6_1", "b6_w1", "Town Hall"},
    {BT_SHOP, 352, 285, "b1_2", "b1_w2", "Jerry Toys"},
    {BT_CAFE, 700, 300, "b4_3", "b4_w3", "Jerry Cafe"},
    {BT_TOWER, 250, 385, "b5_1", "b5_w1", "News Tower"},
    {BT_CAR, 610, 370, "prop_car", NULL, "Snack Car"},
};

typedef struct { const char *key; int x, y; } Prop;
static const Prop PROPS[7] = {{"obj_bench", 450, 345}, {"obj_parasol", 515, 350}, {"prop_workbench", 300, 300},
                              {"prop_beehive", 800, 230}, {"prop_tent", 840, 330}, {"obj_rock_big", 190, 330},
                              {"obj_rock_boulder", 840, 250}};
static int sprProp[7], sprSnowman;

static const char *const OBJ_GRASS[8] = {"obj_tuft_a", "obj_tuft_b", "obj_tuft_c", "obj_flowers",
                                         "obj_sprout", "obj_bush", "obj_rock_small", "obj_rock_pair_l"};
static const char *const OBJ_SAND[7] = {"obj_shell", "obj_starfish", "obj_gshell", "obj_coral", "obj_stone", "obj_crab", "obj_conch"};
static const char *const SEASONS[4] = {"spring", "summer", "fall", "winter"};

/* ------------------------------------------------------------ terrain */

static int kind_at(int x, int y) { return (x < 0 || y < 0 || x >= GW || y >= GH) ? K_WATER : grid[y][x]; }

void map_init(void) {
    char name[24];
    for (int s = 0; s < 4; s++)
        for (int t = 0; t < 3; t++) {
            snprintf(name, sizeof name, "tree_%s_%d", SEASONS[s], t + 1);
            sprTree[s][t] = spr_find(name);
        }
    sprPalm = spr_find("obj_palm");
    for (int c = 1; c <= 6; c++) {
        for (int st = 0; st < 2; st++) {
            snprintf(name, sizeof name, "b%d_%d", st ? 8 : 7, c);
            sprHouse[st][c] = spr_find(name);
            snprintf(name, sizeof name, "b%d_w%d", st ? 8 : 7, c);
            sprHouseW[st][c] = spr_find(name);
        }
    }
    for (int i = 0; i < 5; i++) {
        sprFixed[i] = spr_find(FIXED[i].key);
        sprFixedW[i] = FIXED[i].wk ? spr_find(FIXED[i].wk) : -1;
    }
    for (int i = 0; i < 8; i++) sprObjGrass[i] = spr_find(OBJ_GRASS[i]);
    for (int i = 0; i < 7; i++) sprObjSand[i] = spr_find(OBJ_SAND[i]);
    for (int i = 0; i < 7; i++) sprProp[i] = spr_find(PROPS[i].key);
    sprSnowman = spr_find("prop_snowman");

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            float nx = (x + .5f - 32) / 29, ny = (y + .5f - 20) / 17.5f;
            float rr = sqrtf(nx * nx + ny * ny), a = atan2f(ny, nx);
            float wob = .06f * sinf(a * 3 + 1) + .04f * sinf(a * 5 + 2) + .03f * sinf(a * 7);
            int k = K_WATER;
            if (rr < .95f + wob) k = K_SAND;
            if (rr < .72f + wob) k = K_GRASS;
            float pd = hypotf((float)(x - 50), (float)(y - 27));  // the pond
            if (pd < 3.4f) k = K_WATER;
            else if (pd < 4.9f && k != K_WATER) k = K_SAND;
            grid[y][x] = (uint8_t)k;
        }
    for (int y = 0; y < GH; y++)  // grass never touches water directly
        for (int x = 0; x < GW; x++)
            if (grid[y][x] == K_GRASS) {
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (kind_at(x + dx, y + dy) == K_WATER) grid[y][x] = K_SAND;
            }
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) walkable[y][x] = grid[y][x] != K_WATER;
}

int map_kind(int tx, int ty) { return kind_at(tx, ty); }

static int autotile(int x, int y, int k) {
    bool n = kind_at(x, y - 1) == k, e = kind_at(x + 1, y) == k, s = kind_at(x, y + 1) == k, w = kind_at(x - 1, y) == k;
    if (!n) return !w ? 0 : (!e ? 2 : 1);
    if (!s) return !w ? 6 : (!e ? 8 : 7);
    if (!w) return 3;
    if (!e) return 5;
    if (kind_at(x + 1, y + 1) != k) return 9;
    if (kind_at(x - 1, y + 1) != k) return 10;
    if (kind_at(x + 1, y - 1) != k) return 11;
    if (kind_at(x - 1, y - 1) != k) return 12;
    return 4;
}

int map_tile_index(int tx, int ty) {
    int k = kind_at(tx, ty);
    if (k == K_SAND) return 26;
    return k == K_GRASS ? 13 + autotile(tx, ty, K_GRASS) : autotile(tx, ty, K_WATER);
}

/* ------------------------------------------------------------ layout */

// mulberry32, the same generator as the web game, so the island looks the same.
typedef struct { uint32_t s; } Rng;
static double rn(Rng *r) {
    r->s += 0x6D2B79F5u;
    uint32_t t = (r->s ^ (r->s >> 15)) * (1u | r->s);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

static int cell_kind(double x, double y) { return kind_at((int)floor(x / 16), (int)floor(y / 16)); }

static void add_collide(int x, int y, int w, int h) {
    if (ncollide < MAX_COLLIDE) collide[ncollide++] = (Rect){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
}

static bool blocked(double x, double y, int pad) {
    for (int i = 0; i < ncollide; i++) {
        const Rect *c = &collide[i];
        if (x > c->x - pad && x < c->x + c->w + pad && y > c->y - pad - 50 && y < c->y + c->h + pad) return true;
    }
    for (int i = 0; i < map_nbuildings; i++) {
        const Building *b = &map_buildings[i];
        const SprDef *r = spr_def(b->spr);
        if (x > b->x - r->ax - pad && x < b->x - r->ax + r->w + pad && y > b->y - r->ay - pad && y < b->y + pad) return true;
    }
    return false;
}

static void add_decor(int kind, int x, int y, int t, int spr, bool solid) {
    if (map_ndecor < MAX_DECOR) map_decor[map_ndecor++] = (Decor){kind, x, y, t, spr, solid};
}

static bool near_any(const double (*p)[2], int n, double x, double y, double d) {
    for (int i = 0; i < n; i++)
        if (hypot(p[i][0] - x, p[i][1] - y) < d) return true;
    return false;
}

void map_layout(int season) {
    map_nbuildings = 0;
    ncollide = 0;
    map_ndecor = 0;
    for (int i = 0; i < 5; i++) {
        Building *b = &map_buildings[map_nbuildings++];
        b->type = FIXED[i].type;
        b->slot = -1;
        b->x = FIXED[i].x;
        b->y = FIXED[i].y;
        b->spr = (season == SEASON_WINTER && sprFixedW[i] >= 0) ? sprFixedW[i] : sprFixed[i];
        b->name = FIXED[i].name;
    }
    for (int i = 0; i < jl.nHouses && map_nbuildings < MAX_BUILDINGS; i++) {
        const House *h = &jl.houses[i];
        int st = h->slot % 2;  // b7 / b8 alternate
        Building *b = &map_buildings[map_nbuildings++];
        b->type = BT_HOUSE;
        b->slot = h->slot;
        b->x = MAP_SLOTS[h->slot][0];
        b->y = MAP_SLOTS[h->slot][1];
        int w = sprHouseW[st][h->color];
        b->spr = (season == SEASON_WINTER && w >= 0) ? w : sprHouse[st][h->color];
        b->name = "Home";
    }
    for (int i = 0; i < map_nbuildings; i++) {
        const Building *b = &map_buildings[i];
        const SprDef *r = spr_def(b->spr);
        add_collide(b->x - r->ax + 6, b->y - 14, r->w - 12, 16);
    }

    Rng rg = {11};
    double placed[48][2];
    int np = 0, tries = 0;
    while (np < 26 && tries++ < 600) {
        double x = rn(&rg) * PW, y = rn(&rg) * PH;
        if (cell_kind(x, y) != K_GRASS) continue;
        if (cell_kind(x - 16, y) != K_GRASS || cell_kind(x + 16, y) != K_GRASS || cell_kind(x, y - 24) != K_GRASS) continue;
        if (blocked(x, y, 22)) continue;
        if (near_any((const double(*)[2])placed, np, x, y, 42)) continue;
        placed[np][0] = x;
        placed[np][1] = y;
        np++;
        int t = (int)floor(rn(&rg) * 3);
        add_decor(D_TREE, (int)x, (int)y, t, sprTree[season][t], true);
        add_collide((int)x - 6, (int)y - 8, 12, 8);
    }
    tries = 0;
    int pc = 0;
    while (pc < 9 && tries++ < 500) {
        double x = rn(&rg) * PW, y = rn(&rg) * PH;
        if (cell_kind(x, y) != K_SAND) continue;
        if (cell_kind(x, y + 16) == K_WATER || cell_kind(x, y - 16) == K_WATER || cell_kind(x + 16, y) == K_WATER ||
            cell_kind(x - 16, y) == K_WATER)
            continue;
        if (blocked(x, y, 24) || near_any((const double(*)[2])placed, np, x, y, 48)) continue;
        if (np < 48) { placed[np][0] = x; placed[np][1] = y; np++; }
        add_decor(D_PALM, (int)x, (int)y, 0, sprPalm, true);
        add_collide((int)x - 6, (int)y - 8, 12, 8);
        pc++;
    }
    tries = 0;
    pc = 0;
    while (pc < 14 && tries++ < 500) {
        double x = rn(&rg) * PW, y = rn(&rg) * PH;
        int k = cell_kind(x, y);
        if (k == K_WATER) continue;
        if (blocked(x, y, 16)) continue;
        int spr = k == K_GRASS ? sprObjGrass[(int)floor(rn(&rg) * 8)] : sprObjSand[(int)floor(rn(&rg) * 7)];
        add_decor(D_OBJ, (int)x, (int)y, 0, spr, false);
        pc++;
    }
    for (int i = 0; i < 7; i++) {
        if (cell_kind(PROPS[i].x, PROPS[i].y) == K_WATER) continue;
        add_decor(D_OBJ, PROPS[i].x, PROPS[i].y, 0, sprProp[i], true);
        const SprDef *r = spr_def(sprProp[i]);
        add_collide(PROPS[i].x - r->ax + 3, PROPS[i].y - 8, r->w - 6, 8);
    }
    if (season == SEASON_WINTER) add_decor(D_OBJ, 580, 320, 0, sprSnowman, true);
}

/* ------------------------------------------------------------ queries */

bool map_walk(float x, float y) {
    int cx = (int)floorf(x / 16), cy = (int)floorf(y / 16);
    if (cx < 0 || cy < 0 || cx >= GW || cy >= GH || !walkable[cy][cx]) return false;
    for (int i = 0; i < ncollide; i++) {
        const Rect *c = &collide[i];
        if (x > c->x && x < c->x + c->w && y > c->y && y < c->y + c->h) return false;
    }
    return true;
}

bool map_rand_spot(const float *nearXY, float rad, float *ox, float *oy) {
    for (int i = 0; i < 40; i++) {
        float x = nearXY ? nearXY[0] + R(-rad, rad) : R(60, PW - 60);
        float y = nearXY ? nearXY[1] + R(-rad, rad) : R(60, PH - 60);
        if (map_walk(x, y)) { *ox = x; *oy = y; return true; }
    }
    if (nearXY) { *ox = nearXY[0]; *oy = nearXY[1]; } else { *ox = 512; *oy = 340; }
    return false;
}

int map_building_for_slot(int slot) {
    for (int i = 0; i < map_nbuildings; i++)
        if (map_buildings[i].type == BT_HOUSE && map_buildings[i].slot == slot) return i;
    return -1;
}

int map_tree_spr(int season, int t) { return sprTree[season & 3][t % 3]; }
int map_spr_palm(void) { return sprPalm; }
