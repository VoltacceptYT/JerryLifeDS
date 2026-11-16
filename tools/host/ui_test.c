#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "jl.h"
#include "util.h"
#include "daycycle.h"
#include "map.h"
#include "world.h"
// Host test: drives ui.c headlessly, dumps both screens to /tmp/ui.raw and round-trips the save format.
void sfx_play(int k){(void)k;}
void plat_save_now(void){} void plat_set_music(bool on){(void)on;}
const char *plat_save_status(void){return "Saved";} bool plat_storage_ok(void){return true;}
static px top[SCREEN_W*SCREEN_H], bot[SCREEN_W*SCREEN_H];
int main(void){
  util_seed(7); jl_init(); world_init(); jl_start(); ui_init();
  UiInput in; memset(&in,0,sizeof in);
  FILE *f=fopen("/tmp/ui.raw","wb");
  for(int t=0;t<600;t++){
    in.down=0; in.repeat=0;
    if(t==5){ in.down=UK_A; in.repeat=UK_A; }
    if(t==100){in.touchDown=1;in.touchHeld=1;in.tx=128;in.ty=100;} else {in.touchDown=0;}
    ui_update(&in); jl_tick();
    if(t%100==0||ui_bottom_dirty()){ ui_draw_top(top); ui_draw_bottom(bot);} }
  fwrite(top,1,sizeof top,f); fwrite(bot,1,sizeof bot,f); fclose(f);
  uint8_t buf[4096]; size_t n=jl_pack(buf,sizeof buf); printf("pack %zu bytes, unpack %d\n",n,jl_unpack(buf,n));
  return 0; }
