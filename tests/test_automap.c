#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,draws;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"automap line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned level,i,step;
    SsaNativeGame *g=calloc(1,sizeof(*g));uint32_t *pixels=malloc(38400*4);
    SsaPlayerSpawn *player=malloc(sizeof(*player));SsaSpriteCache *cache=malloc(sizeof(*cache));
    CHECK(argc==2 && rom && g && pixels && player && cache && (f=fopen(argv[1],"rb")));
    CHECK(fread(rom,1,0x800000,f)==0x800000 && !fclose(f));
    for(level=0;level<16;++level){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1 && g->automap_zoom==8192);
        CHECK(ssa_native_game_tick(g)==1);++frames;
        CHECK(ssa_native_game_automap_zoom(g,1)==-1);
        CHECK(ssa_native_game_automap(g,1)==1 && (g->players[0].motion.movement.flags&8));
        for(step=0;step<5;++step){SsaEntity *blocking=g->world.blocking_entity;unsigned colored=0;
            CHECK(ssa_native_game_automap_zoom(g,step&1?100:-100)==1);
            CHECK(g->automap_zoom==(step&1?65536u:4096u));
            *player=g->players[0];*cache=g->cache;
            CHECK(ssa_native_game_draw(g)==1);++draws;
            CHECK(!memcmp(player,&g->players[0],sizeof(*player)) && !memcmp(cache,&g->cache,sizeof(*cache)) && g->world.blocking_entity==blocking);
            for(i=0;i<38400;++i)colored+=g->rgb888[i]!=0;
            CHECK(colored>100);memcpy(pixels,g->rgb888,38400*4);
            CHECK(ssa_native_game_draw(g)==1 && !memcmp(pixels,g->rgb888,38400*4));++draws;
        }
        ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);ssa_pc_input_runtime_relative(&g->input_runtime,30,0);
        ssa_pc_input_runtime_button(&g->input_runtime,1,1);
        for(i=0;i<32;++i){CHECK(ssa_native_game_tick(g)==1 && (g->players[0].motion.movement.flags&8));++frames;
            CHECK(ssa_native_game_draw(g)==1);++draws;}
        CHECK(g->weapon_game.shots[0]==0);
        ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);ssa_pc_input_runtime_button(&g->input_runtime,1,0);
        CHECK(ssa_native_game_automap(g,0)==1 && !(g->players[0].motion.movement.flags&8) && !g->weapon.fire_inhibited);
        CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_draw(g)==1);++frames;
        CHECK(ssa_native_game_automap(g,1)==1);
        /* Turning the feature off is cleanup, including from Options after
         * completion/pause. Only opening is gated by live gameplay. */
        g->finished=1;g->frame_game.flags|=1;
        CHECK(ssa_native_game_automap(g,1)==-1);
        CHECK(ssa_native_game_automap(g,0)==1 && !(g->players[0].motion.movement.flags&8));
        g->finished=0;g->frame_game.flags&=(uint16_t)~1u;
        CHECK(ssa_native_game_automap(g,1)==1);
        g->players[0].inventory.health=g->players[0].entity_health=0;
        CHECK(ssa_native_game_tick(g)==1 && !(g->players[0].motion.movement.flags&8));++frames;
        CHECK(ssa_native_game_automap(g,1)==-1);
        CHECK(ssa_native_game_automap(g,0)==1);ssa_native_game_free(g);
    }
    /* An automatic weapon already firing must stop while the map is open,
     * including when alternate_before refreshes its per-frame policy. */
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    g->players[0].inventory.weapons|=16;g->players[0].inventory.ammo[4]=100;
    CHECK(ssa_native_game_select_weapon(g,5)==1);
    for(i=0;i<100 && (g->gameplay.weapon.current!=5 || g->weapon.object.y!=0x5000);++i){CHECK(ssa_native_game_tick(g)==1);++frames;}
    CHECK(i<100);ssa_pc_input_runtime_button(&g->input_runtime,1,1);
    for(i=0;i<20;++i){CHECK(ssa_native_game_tick(g)==1);++frames;}
    CHECK(g->weapon_game.shots[4]>0);step=g->weapon_game.shots[4];
    CHECK(ssa_native_game_automap(g,1)==1);
    for(i=0;i<32;++i){CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[4]==step);++frames;}
    ssa_native_game_free(g);
    free(cache);free(player);free(pixels);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"native_frames\":%u,\"draws\":%u,\"maps\":16}\n",checks,frames,draws);return 0;
}
