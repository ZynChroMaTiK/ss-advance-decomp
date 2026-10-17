#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    FILE *f;uint8_t *rom;SsaNativeGame *g;unsigned i,airborne=0,third=0;
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    rom=malloc(0x800000);g=calloc(1,sizeof(*g));if(!rom || !g)return 2;
    if(fread(rom,1,0x800000,f)!=0x800000)return 2;
    fclose(f);
    if(ssa_native_game_init(g,rom,0x800000,0)!=1){fprintf(stderr,"init line%u\n",g->error_line);return 1;}
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    for(i=0;i<600;++i){
        if(i==10)ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
        if(i==90)ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
        if(i%120==0)ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);
        if(i%120==1)ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
        if(i%60==0)ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0);
        if(i%60==4)ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0);
        ssa_pc_input_runtime_button(&g->input_runtime,1,i%30<15);
        if(ssa_native_game_step(g)!=1){fprintf(stderr,"frame%u line%u pool%u health%d\n",i,g->error_line,g->spawn.pool.active_count,g->players[0].inventory.health);return 1;}
        if(g->players[0].entity.body.z>(int32_t)g->players[0].entity.body.floor*65536)++airborne;
        if(g->player.pose.third_person)++third;
    }
    if(!airborne || !third || !g->pickup.game.collected || g->players[0].inventory.health>=100)return 1;
    printf("{\"frames\":%u,\"health\":%d,\"entities\":%u,\"airborne\":%u,\"third_person\":%u,\"pickups\":%u,",
        i,g->players[0].inventory.health,g->spawn.pool.active_count,airborne,third,g->pickup.game.collected);
    ssa_native_game_free(g);
    if(ssa_native_game_init(g,rom,0x800000,0)!=1 || g->score || g->players[0].inventory.health!=100 ||
       g->spawn.pool.active_count!=16)return 1;
    for(i=0;i<120;++i)if(ssa_native_game_step(g)!=1)return 1;
    printf("\"restart_frames\":120,\"restart_health\":%d}\n",g->players[0].inventory.health);
    ssa_native_game_free(g);free(g);free(rom);return 0;
}
