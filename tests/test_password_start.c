#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,starts,frames,restarts,level_starts,carry_starts,starter_restarts;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"password start%u line%d: %s\n",starts,__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g)),*next;
    unsigned difficulty,level,j,t;FILE *f;
    CHECK(argc==2 && rom && g);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_init_loadout(g,rom,0x800000,0,37)==-1 && !g->rom);
    CHECK(ssa_native_game_init_loadout(g,rom,0x800000,1,1)==-1 && !g->rom);
    for(difficulty=0;difficulty<3;++difficulty)for(level=0;level<SSA_NATIVE_CAMPAIGN_LEVELS;++level){
        unsigned setting=difficulty*12+level+1;uint16_t weapons=1;SsaNativeCarry entry;
        CHECK(ssa_native_game_init_loadout(g,rom,0x800000,level,setting)==1);
        CHECK(!g->frame_game.counter && !g->score && g->damage.difficulty==difficulty);
        CHECK(g->gameplay.life_game.continue_flag==(difficulty==0?5:difficulty==1?3:2));
        CHECK(g->players[0].inventory.health==100 && g->players[0].inventory.ammo[0]==999);
        for(j=1;j<10;++j){int ammo=g->loadouts.ammo[setting-1][j-1];
            int cap=(int16_t)g->inventory_rules.ammo_capacity[j];
            if(ammo){weapons|=(uint16_t)(1u<<j);if(ammo>cap)ammo=cap;}
            CHECK(g->players[0].inventory.ammo[j]==ammo);
        }
        CHECK(g->players[0].inventory.weapons==weapons);
        CHECK((weapons&(1u<<(g->gameplay.weapon.current-1))) && g->gameplay.weapon.current==g->gameplay.weapon.requested);
        CHECK(ssa_native_game_carry_capture(&entry,g)==1);
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        for(t=0;t<24;++t){
            if(t==2 || t==14){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
            if(t==4){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,1,0)==1);CHECK(ssa_pc_input_runtime_key(&g->input_runtime,32,0,0)==1);}
            CHECK(ssa_native_game_step(g)==1);++frames;
        }
        CHECK(!g->player.pose.third_person);
        next=calloc(1,sizeof(*next));CHECK(next);
        CHECK(ssa_native_game_init(next,rom,0x800000,level)==1);
        CHECK(ssa_native_game_carry_apply(next,&entry)==1);
        CHECK(!memcmp(&next->players[0].inventory,&entry.inventory,sizeof(entry.inventory)));
        CHECK(next->gameplay.weapon.current==entry.weapon && next->gameplay.weapon.requested==entry.weapon);
        CHECK(ssa_native_game_step(next)==1);++frames;++restarts;
        ssa_native_game_free(next);free(next);ssa_native_game_free(g);++starts;
    }
    /* Fresh starts use ROM data; advancement must retain even a very different
     * existing inventory. Compare both paths and exact entry restarts. */
    for(difficulty=0;difficulty<3;++difficulty)for(level=0;level<SSA_NATIVE_CAMPAIGN_LEVELS;++level){
        SsaNativeCarry progress={0},before,entry;unsigned weapon=1;uint16_t owned=1;
        progress.inventory.weapons=0x3ff;progress.weapon=10;progress.previous_weapon=9;
        for(j=0;j<10;++j)progress.inventory.ammo[j]=(int16_t)(j==9?1:0);
        progress.inventory.health=73;progress.inventory.stat_c4=41;progress.inventory.stat_c6=23;
        progress.inventory.pending_count=2;progress.inventory.pending_weapons[0]=8;progress.inventory.pending_weapons[1]=10;
        progress.score=123456;progress.third_person=1;before=progress;
        CHECK(ssa_native_game_init_campaign(g,rom,0x800000,level,difficulty,NULL)==1);
        CHECK(!g->frame_game.counter);
        CHECK(g->damage.difficulty==difficulty && g->gameplay.life_game.continue_flag==(difficulty==0?5:difficulty==1?3:2));
        CHECK(!g->score && !g->player.view.third_person && !g->previous_weapon);
        CHECK(g->players[0].inventory.health==100);
        CHECK(g->players[0].inventory.ammo[0]==999 && !g->players[0].inventory.pending_count);
        for(j=1;j<10;++j){size_t at=0x44bcf0+(difficulty*12+level+1)*20+(j-1)*2;
            int ammo=level?(int16_t)((unsigned)rom[at]|(unsigned)rom[at+1]<<8):0;
            int cap=(int16_t)g->inventory_rules.ammo_capacity[j];
            CHECK(g->players[0].inventory.ammo[j]==(ammo && ammo>cap?cap:ammo));
            if(ammo){owned|=(uint16_t)(1u<<j);weapon=j+1;}
        }
        CHECK(g->players[0].inventory.weapons==owned && g->gameplay.weapon.current==weapon && g->gameplay.weapon.requested==weapon);
        CHECK(ssa_native_game_carry_capture(&entry,g)==1);++level_starts;
        CHECK(ssa_native_game_step(g)==1);++frames;
        next=calloc(1,sizeof(*next));CHECK(next && ssa_native_game_init(next,rom,0x800000,level)==1);
        CHECK(ssa_native_game_carry_apply(next,&entry)==1);
        CHECK(!memcmp(&next->players[0].inventory,&entry.inventory,sizeof(entry.inventory)) && next->gameplay.weapon.requested==weapon);
        CHECK(ssa_native_game_step(next)==1);++frames;++starter_restarts;
        ssa_native_game_free(next);free(next);ssa_native_game_free(g);
        CHECK(ssa_native_game_init_campaign(g,rom,0x800000,level,difficulty,&progress)==1);
        CHECK(!memcmp(&progress,&before,sizeof(progress)) && !g->frame_game.counter);
        CHECK(!memcmp(&g->players[0].inventory,&progress.inventory,sizeof(progress.inventory)));
        CHECK(g->gameplay.weapon.current==10 && g->gameplay.weapon.requested==10 && g->previous_weapon==9);
        CHECK(g->score==123456 && g->player.view.third_person==1 && g->damage.difficulty==difficulty);
        CHECK(ssa_native_game_step(g)==1);++frames;++carry_starts;ssa_native_game_free(g);
        progress.inventory.health=0;
        CHECK(ssa_native_game_init_campaign(g,rom,0x800000,level,difficulty,&progress)==-1 && !g->rom);
    }
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,12,1,NULL)==-1 && !g->rom);
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,3,NULL)==-1 && !g->rom);
    free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"password_starts\":%u,\"frames\":%u,\"entry_restarts\":%u,\"level_starts\":%u,\"carry_starts\":%u,\"starter_restarts\":%u,\"levels\":%u,\"difficulties\":3}\n",checks,starts,frames,restarts,level_starts,carry_starts,starter_restarts,SSA_NATIVE_CAMPAIGN_LEVELS);
    return 0;
}
