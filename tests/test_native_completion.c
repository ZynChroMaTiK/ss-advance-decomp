#include "ssa/native_completion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,starts,game_frames,tally_frames,records,chained_records,guards,events;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"completion start%u line%d: %s\n",starts,__LINE__,#x);exit(1);}}while(0)
static int event(void *ctx,const SsaCompletion *s,SsaCompletionEvent kind,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    SsaCompletion before=*s;SsaCompletionOps *ops=ctx;
    CHECK(!b && !d && c==(kind==SSA_COMPLETION_NOTICE?40u:0u));
    CHECK(kind==SSA_COMPLETION_NOTICE?(a>=0xb6 && a<=0xc4):a==(kind==SSA_COMPLETION_COMMAND?16u:7u));
    CHECK(ssa_completion_step((SsaCompletion *)s,0,0,0,ops)==-1 && !memcmp(s,&before,sizeof before));
    ++events;return 0;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*g));
    unsigned map,difficulty,i,t,setting;FILE *f;SsaCompletionOps ops={event,NULL};ops.context=&ops;
    SsaStatisticsTotals campaign[3]={{0}};
    CHECK(argc==2 && rom && g && before);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(difficulty=0;difficulty<3;++difficulty)for(map=0;map<12;++map){
        SsaCompletion tally,unchanged;SsaCompletionRecord out,sentinel;
        SsaStatisticsTotals totals,old_totals;uint32_t hits=0,spawned=0,kills=0,attacks=0;
        unsigned *busy[6]={&g->game.updating,&g->world.query_active,&g->combat.updating,
            &g->spawn.updating,&g->player.updating,&g->weapon.updating};
        CHECK(ssa_native_game_init(g,rom,0x800000,map)==1);g->damage.difficulty=(uint8_t)difficulty;
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        for(t=0;t<32;++t){
            if(t==2 || t==24){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);
                CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
            if(t==4){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,1,0)==1);
                CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,0,0)==1);}
            CHECK(ssa_native_game_tick(g)==1);++game_frames;
        }
        memset(&unchanged,0x5a,sizeof unchanged);tally=unchanged;
        CHECK(ssa_native_completion_init(&tally,g)==-1 && !memcmp(&tally,&unchanged,sizeof tally));++guards;
        /* Controlled end fixture: this validates the adapter, not normal-input
         * level completion. Never used as campaign admission evidence. */
        g->finished=1;g->gameplay.life_game.input_state=3;g->score=UINT32_MAX-99;
        g->players[0].inventory.health=113;g->players[0].inventory.stat_c6=23;g->players[0].inventory.stat_c4=41;
        g->players[0].inventory.weapons=0x3ff;g->gameplay.weapon.current=10;g->gameplay.weapon.requested=9;
        for(i=0;i<10;++i){g->players[0].inventory.ammo[i]=(int16_t)(1+i);
            g->weapon_game.shots[i]=UINT32_C(0x80000000)+i;g->weapon.hits[i]=100+i;hits+=g->weapon.hits[i];}
        for(i=0;i<12;++i){g->spawn.game.spawned[i]=UINT32_C(0x80000000)+i;spawned+=g->spawn.game.spawned[i];
            g->damage.enemy_hits[i]=200+i;attacks+=g->damage.enemy_hits[i];g->damage.enemy_kills[i]=300+i;kills+=g->damage.enemy_kills[i];}
        g->pickup.game.collected=17;
        memcpy(before,g,sizeof(*g));CHECK(ssa_native_completion_init(&tally,g)==1 && !memcmp(before,g,sizeof(*g)));
        CHECK(tally.level==map && tally.difficulty==difficulty && tally.source_score==g->score && tally.source_counter==g->frame_game.counter);
        memset(&totals,0xff,sizeof totals);old_totals=totals;memset(&sentinel,0x5a,sizeof sentinel);out=sentinel;
        CHECK(ssa_native_completion_record(&out,&tally,g,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out) && !tally.recorded);++guards;
        for(t=0;t<5000 && tally.running;++t){CHECK(ssa_completion_step(&tally,1,1,1,&ops)>=0);++tally_frames;}
        CHECK(!tally.running && tally.stage==6 && !tally.recorded);
        CHECK(!memcmp(before,g,sizeof(*g)));
        /* Preserve valid source and completed state through every setting byte.
         * Original1231C clears both weapon counters for ANY nonzero setting. */
        for(setting=0;setting<256;++setting){
            SsaCompletion one=tally;
            CHECK(ssa_native_completion_record(&out,&one,g,&totals,(uint8_t)setting)==1 && one.recorded);
            CHECK(out.next_level==map+1 && out.weapons.current==10 && out.weapons.requested==9 && out.weapons.weapons==0x3ff);
            CHECK(!memcmp(out.weapons.ammo,g->players[0].inventory.ammo,sizeof out.weapons.ammo));
            CHECK(out.totals.score==tally.score && out.totals.health==113 && out.totals.armor6==23 && out.totals.armor4==41 && out.totals.level==map);
            CHECK(out.totals.elapsed==UINT32_MAX+g->frame_game.elapsed && out.totals.pickups==16);
            CHECK(out.totals.spawned==UINT32_MAX+spawned && out.totals.kills==UINT32_MAX+kills && out.totals.hits==UINT32_MAX+attacks);
            CHECK(out.totals.weapon_hits==(setting?UINT32_MAX:UINT32_MAX+hits));
            for(i=0;i<10;++i){CHECK(out.checkpoint.weapons[i].owned==1 && out.checkpoint.weapons[i].ammo==(int)i+1);
                CHECK(out.checkpoint.weapons[i].shots==(setting?0:g->weapon_game.shots[i]) && out.checkpoint.weapons[i].hits==(setting?0:g->weapon.hits[i]));}
            for(i=0;i<12;++i)CHECK(out.checkpoint.enemies[i].spawned==g->spawn.game.spawned[i] &&
                out.checkpoint.enemies[i].hits==g->damage.enemy_hits[i] && out.checkpoint.enemies[i].kills==g->damage.enemy_kills[i]);
            out=sentinel;CHECK(ssa_native_completion_record(&out,&one,g,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out));
            ++records;++guards;
        }
        CHECK(!memcmp(before,g,sizeof(*g)) && !memcmp(&totals,&old_totals,sizeof totals));
        {SsaCompletion one=tally;SsaStatisticsTotals old=campaign[difficulty];
         CHECK(ssa_native_completion_record(&out,&one,g,&campaign[difficulty],0)==1);
         CHECK(out.totals.elapsed==old.elapsed+g->frame_game.elapsed && out.totals.pickups==old.pickups+17u);
         CHECK(out.totals.spawned==old.spawned+spawned && out.totals.kills==old.kills+kills && out.totals.hits==old.hits+attacks);
         CHECK(out.totals.weapon_hits==old.weapon_hits+hits && out.totals.score==tally.score);
         campaign[difficulty]=out.totals;++chained_records;}
        for(i=0;i<6;++i){*busy[i]=1;out=sentinel;unchanged=tally;
            CHECK(ssa_native_completion_record(&out,&tally,g,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out) && !memcmp(&tally,&unchanged,sizeof tally));
            CHECK(ssa_native_completion_init(&tally,g)==-1 && !memcmp(&tally,&unchanged,sizeof tally));*busy[i]=0;guards+=2;}
        for(i=0;i<8;++i){
            if(i==0)++g->score;
            if(i==1)++g->frame_game.counter;
            if(i==2)++g->frame_game.elapsed;
            if(i==3)++g->players[0].inventory.ammo[2];
            if(i==4)g->finished=0;
            if(i==5)g->gameplay.life_game.input_state=4;
            if(i==6)g->players[0].inventory.health=0;
            if(i==7)g->error_line=17;
            out=sentinel;unchanged=tally;
            CHECK(ssa_native_completion_record(&out,&tally,g,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out) && !memcmp(&tally,&unchanged,sizeof tally));
            memcpy(g,before,sizeof(*g));++guards;
        }
        out=sentinel;unchanged=tally;
        CHECK(ssa_native_completion_record(&out,&tally,NULL,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out));
        CHECK(ssa_native_completion_record(&out,NULL,g,&totals,0)==-1 && !memcmp(&out,&sentinel,sizeof out));
        CHECK(ssa_native_completion_record(&out,&tally,g,NULL,0)==-1 && !memcmp(&out,&sentinel,sizeof out));
        CHECK(ssa_native_completion_record(NULL,&tally,g,&totals,0)==-1 && !memcmp(&tally,&unchanged,sizeof tally));guards+=4;
        ssa_native_game_free(g);++starts;
    }
    free(before);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"starts\":%u,\"maps\":12,\"difficulties\":3,\"game_frames\":%u,\"tally_frames\":%u,\"records\":%u,\"chained_records\":%u,\"guards\":%u,\"events\":%u}\n",
        checks,starts,game_frames,tally_frames,records,chained_records,guards,events);return 0;
}
