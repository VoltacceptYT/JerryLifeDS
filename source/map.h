// Island terrain, fixed buildings, decoration and collision (ported from Jerry Life's buildMap / layout).
#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "jl.h"

enum { K_WATER, K_SAND, K_GRASS };
enum { BT_HALL, BT_SHOP, BT_CAFE, BT_TOWER, BT_CAR, BT_HOUSE };
enum { D_TREE, D_PALM, D_OBJ };

typedef struct {
    int type;        // BT_*
    int slot;        // house slot (BT_HOUSE) else -1
    int x, y;        // anchor (feet)
    int spr;         // current sprite (summer / winter variant)
    const char *name;
} Building;

typedef struct {
    int kind;        // D_*
    int x, y;
    int t;           // tree variant 0..2
    int spr;         // D_OBJ sprite
    bool solid;      // depth-sorted (otherwise flat on the ground)
} Decor;

typedef struct { int16_t x, y, w, h; } Rect;

#define MAX_BUILDINGS 12
#define MAX_DECOR 64
#define MAX_COLLIDE 72

extern Building map_buildings[MAX_BUILDINGS];
extern int map_nbuildings;
extern Decor map_decor[MAX_DECOR];
extern int map_ndecor;

extern const int MAP_SLOTS[MAX_HOUSES][2];
extern const int HOUSE_COST[MAX_HOUSES];

void map_init(void);               // resolve sprite ids, build the terrain grid
void map_layout(int season);       // (re)place buildings and decoration for the season
int map_tile_index(int tx, int ty);  // 0..12 water pieces, 13..25 grass pieces, 26 sand
int map_kind(int tx, int ty);
bool map_walk(float x, float y);
bool map_rand_spot(const float *nearXY, float rad, float *ox, float *oy); // nearXY = NULL: anywhere
int map_building_for_slot(int slot);  // index into map_buildings, -1 if none
int map_tree_spr(int season, int t);
int map_spr_palm(void);

#endif
