#include "ssa/native_statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,starts,frames,snapshots,guards;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"statistics start%u line%d: %s\n",starts,__LINE__,#x);exit(1);}}while(0)
static void snapshot(SsaNativeGame *g,SsaNativeGame *before)
{
    SsaLevelStatistics level;unsigned i;
    memcpy(before,g,sizeof(*g));memset(&level,0xa5,sizeof level);
    CHECK(ssa_native_game_statistics(&level,g)==1);
    CHECK(!memcmp(before,g,sizeof(*g)));
    CHECK(level.elapsed==g->frame_game.elapsed && level.score==g->score && level.level==g->level);
    CHECK(level.health==g->players[0].inventory.health && level.armor6==g->players[0].inventory.stat_c6 &&
          level.armor4==g->players[0].inventory.stat_c4 && level.pickups==g->pickup.game.collected);
    for(i=0;i<10;++i){
        CHECK(level.rows.weapons[i].owned==((g->players[0].inventory.weapons>>i)&1));
        CHECK(level.rows.weapons[i].ammo==g->players[0].inventory.ammo[i]);
        CHECK(level.rows.weapons[i].shots==g->weapon_game.shots[i]);
        CHECK(level.rows.weapons[i].hits==g->weapon.hits[i]);
    }
    for(i=0;i<12;++i){
        CHECK(level.rows.enemies[i].spawned==g->spawn.game.spawned[i]);
        CHECK(level.rows.enemies[i].hits==g->damage.enemy_hits[i]);
        CHECK(level.rows.enemies[i].kills==g->damage.enemy_kills[i]);
    }
    ++snapshots;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*g));
    unsigned difficulty,map,i,t;FILE *f;
    CHECK(argc==2 && rom && g && before);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(difficulty=0;difficulty<3;++difficulty)for(map=0;map<16;++map){
        SsaLevelStatistics level,unchanged;SsaStatisticsRows saved;SsaStatisticsTotals totals={0};
        uint32_t hits=0,spawned=0,kills=0,attacks=0;
        unsigned *busy[6]={&g->game.updating,&g->world.query_active,&g->combat.updating,
            &g->spawn.updating,&g->player.updating,&g->weapon.updating};
        CHECK(ssa_native_game_init(g,rom,0x800000,map)==1);g->damage.difficulty=(uint8_t)difficulty;
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        snapshot(g,before);
        for(t=0;t<64;++t){
            if(t==2 || t==30){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);
                CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
            if(t==4){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,1,0)==1);
                CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,0,0)==1);}
            if(t==8)CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
            if(t==9)CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
            CHECK(ssa_native_game_tick(g)==1);++frames;
            if(t%16==15)snapshot(g,before);
        }
        /* Directed projection fixture, not completion or earned inventory proof.
         * Distinct values expose swapped columns, counter truncation and use of
         * a stale body-health mirror in place of canonical inventory health. */
        for(i=0;i<10;++i){g->weapon_game.shots[i]=UINT32_C(0x80001000)+i;
            g->weapon.hits[i]=UINT32_C(0xffffff00)+i;hits+=g->weapon.hits[i];}
        for(i=0;i<12;++i){g->spawn.game.spawned[i]=UINT32_C(0x80000000)+i;
            g->damage.enemy_hits[i]=100+i;g->damage.enemy_kills[i]=300+i;
            spawned+=g->spawn.game.spawned[i];attacks+=g->damage.enemy_hits[i];kills+=g->damage.enemy_kills[i];}
        g->players[0].inventory.health=113;g->players[0].inventory.stat_c6=-32768;
        g->players[0].inventory.stat_c4=32767;g->pickup.game.collected=UINT32_MAX;g->score=UINT32_MAX-7;
        snapshot(g,before);CHECK(ssa_native_game_statistics(&level,g)==1);
        CHECK(ssa_statistics_checkpoint_capture(&saved,&level)==1);
        CHECK(ssa_statistics_complete(&totals,&level)==1);
        CHECK(totals.weapon_hits==hits && totals.spawned==spawned && totals.kills==kills && totals.hits==attacks);
        CHECK(totals.elapsed==level.elapsed && totals.score==UINT32_MAX-7 && totals.pickups==UINT32_MAX);
        CHECK(totals.health==113 && totals.armor6==-32768 && totals.armor4==32767 && totals.level==map);
        memset(&level.rows,0,sizeof level.rows);CHECK(ssa_statistics_checkpoint_restore(&level,&saved)==1);
        CHECK(!memcmp(&level.rows,&saved,sizeof saved) && !level.pickups && level.health==113);
        memset(&unchanged,0x5a,sizeof unchanged);level=unchanged;
        for(i=0;i<6;++i){*busy[i]=1;memcpy(before,g,sizeof(*g));
            CHECK(ssa_native_game_statistics(&level,g)==-1 && !memcmp(&level,&unchanged,sizeof level));
            CHECK(!memcmp(before,g,sizeof(*g)));*busy[i]=0;++guards;}
        g->error_line=17;CHECK(ssa_native_game_statistics(&level,g)==-1 && !memcmp(&level,&unchanged,sizeof level));
        g->error_line=0;g->loaded=0;CHECK(ssa_native_game_statistics(&level,g)==-1 && !memcmp(&level,&unchanged,sizeof level));
        g->loaded=1;CHECK(ssa_native_game_statistics(&level,NULL)==-1 && !memcmp(&level,&unchanged,sizeof level));
        memcpy(before,g,sizeof(*g));CHECK(ssa_native_game_statistics(NULL,g)==-1 && !memcmp(before,g,sizeof(*g)));guards+=4;
        ssa_native_game_free(g);++starts;
    }
    free(before);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"starts\":%u,\"maps\":16,\"difficulties\":3,\"frames\":%u,\"snapshots\":%u,\"guards\":%u}\n",
        checks,starts,frames,snapshots,guards);
    return 0;
}
