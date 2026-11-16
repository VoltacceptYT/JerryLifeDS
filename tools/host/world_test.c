// Host test: runs the simulation headlessly and dumps the top-screen world view.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "daycycle.h"
#include "jl.h"
#include "map.h"
#include "sprites.h"
#include "util.h"
#include "world.h"

void sfx_play(int kind) { (void)kind; }
static px fb[SCREEN_W * SCREEN_H];

static void dump(const char *path, int camX, int camY) {
    Canvas c;
    gfx_begin(&c, fb);
    world_prepare();
    world_draw(&c, camX, camY);
    FILE *f = fopen(path, "ab");
    fwrite(fb, 1, sizeof fb, f);
    fclose(f);
}

int main(int argc, char **argv) {
    int season = argc > 1 ? atoi(argv[1]) : 1;
    int timeMode = argc > 2 ? atoi(argv[2]) : 2;
    util_seed(1234);
    jl_init();
    jl.seasonMode = (uint8_t)(season + 1);
    jl.timeMode = (uint8_t)timeMode;
    jl_start();
    jl_create_jerry("Jerry", 0, PERS_SWEET);
    jl_create_jerry("Gerald", 1, PERS_GRUMPY);
    jl_create_jerry("Jez", 2, PERS_ENERGETIC);
    jl.coins = 5000;
    jl_buy_toy(TOY_BEACH_BALL);
    jl_buy_toy(TOY_WHITE_DICE);
    jl_buy_food(0);
    jl_feed(&jl.jerries[0], 0);
    remove("/tmp/world.raw");
    for (int t = 0; t < 60 * 20; t++) jl_tick();
    printf("jerries at: ");
    for (int i = 0; i < jl.nJerries; i++) printf("(%d,%d) ", (int)jl.jerries[i].x, (int)jl.jerries[i].y);
    printf("\nbuildings %d decor %d\n", map_nbuildings, map_ndecor);
    dump("/tmp/world.raw", 256, 190);
    dump("/tmp/world.raw", 0, 0);
    dump("/tmp/world.raw", 512, 224);
    return 0;
}
