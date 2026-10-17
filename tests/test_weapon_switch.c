#define main cadence_fixture_main
#include "test_weapon_cadence.c"
#undef main

static unsigned switch_cases,waiting_frames;
static void settle(SsaNativeGame *g,unsigned weapon){
    unsigned i;
    for(i=0;i<180 && (g->gameplay.weapon.current!=weapon || g->weapon.object.y!=0x5000);++i)
        CHECK(ssa_native_game_tick(g)==1);
    CHECK(i<180 && g->gameplay.weapon.current==weapon && g->weapon.object.y==0x5000);
}
static void switch_case(const uint8_t *rom,unsigned weapon,unsigned view){
    SsaNativeGame *g=click_game(rom,weapon);unsigned i,shots,completion=0;
    unsigned target=weapon==2?3:2,continuous=g->weapon_rules.continuous[weapon-1]!=0;
    CHECK(g->weapon.finish_fire_before_switch==1 && !g->previous_weapon);
    for(i=0;i<10;++i)g->players[0].inventory.ammo[i]=300;
    g->damage.god_mode=1;g->player.free_aim=1;g->player.view.third_person=(uint8_t)view;
    if(weapon==9)g->player.pitch=14564; /* Empty Time Strike gesture must also finish before switching. */
    if(continuous){
        CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
        for(i=0;i<60;++i)CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->weapon_game.shots[weapon-1]>0 && g->weapon.object.done);
    }else {tap(g);CHECK(ssa_native_game_tick(g)==1);}
    CHECK((g->gameplay.weapon.flags&0x400) && g->weapon.object.y==0x5000);
    shots=g->weapon_game.shots[weapon-1];
    CHECK(ssa_native_game_select_weapon(g,target)==1);
    /* A later selection replaces the pending destination without clipping. */
    if(view){CHECK(ssa_native_game_select_weapon(g,10)==1);CHECK(ssa_native_game_select_weapon(g,target)==1);}
    for(i=0;i<180 && (g->gameplay.weapon.flags&0x400);++i){
        if(i && g->weapon.object.done)completion=1;
        CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->gameplay.weapon.current==weapon && !g->previous_weapon);
        if(g->gameplay.weapon.flags&0x400){
            CHECK(g->weapon.object.y==0x5000 && g->weapon.switch_wait);
            ++waiting_frames;
        }else CHECK(completion && g->weapon.object.y>0x5000);
        if(continuous){
            CHECK(g->weapon_game.shots[weapon-1]==shots);
            if(!i){
                SsaWeaponObject before=g->weapon.object;
                g->frame_game.flags|=1;
                CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_tick(g)==1);
                CHECK(g->weapon.object.frame_index==before.frame_index && g->weapon.object.elapsed==before.elapsed &&
                      g->weapon.object.y==before.y && g->weapon.switch_wait);
                g->frame_game.flags&=(uint16_t)~1u;
                CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
                if(view && weapon!=1)g->players[0].inventory.ammo[weapon-1]=0;
            }
        }
    }
    CHECK(i>1 && i<180 && completion);
    if(!continuous){
        if(weapon==9)CHECK(g->weapon_game.shots[8]==shots && g->players[0].inventory.ammo[8]==300);
        else CHECK(g->weapon_game.shots[weapon-1]==shots+1);
    }
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    settle(g,target);CHECK(g->previous_weapon==weapon && !g->weapon.switch_wait);
    CHECK(!g->error_line);
    ++switch_cases;ssa_native_game_free(g);free(g);
}
static void history(const uint8_t *rom){
    SsaNativeGame *g=click_game(rom,1),*next=calloc(1,sizeof(*next));SsaNativeCarry carry;
    CHECK(ssa_native_game_previous_weapon(g)==0);
    g->players[0].inventory.ammo[2]=20;
    CHECK(ssa_native_game_select_weapon(g,3)==1 && !g->previous_weapon);
    settle(g,3);CHECK(g->previous_weapon==1);
    CHECK(ssa_native_game_previous_weapon(g)==1 && g->previous_weapon==1);
    settle(g,1);CHECK(g->previous_weapon==3);
    CHECK(ssa_native_game_previous_weapon(g)==1);settle(g,3);CHECK(g->previous_weapon==1);
    CHECK(ssa_native_game_carry_capture(&carry,g)==1 && carry.previous_weapon==1);
    CHECK(next && ssa_native_game_init(next,rom,0x800000,1)==1);
    CHECK(ssa_native_game_carry_apply(next,&carry)==1 && next->previous_weapon==1);
    carry.previous_weapon=11;
    CHECK(ssa_native_game_carry_apply(next,&carry)==-1 && next->previous_weapon==1);
    g->previous_weapon=2;g->players[0].inventory.ammo[1]=0;
    CHECK(ssa_native_game_previous_weapon(g)==0 && g->gameplay.weapon.requested==3);
    g->players[0].inventory.ammo[1]=20;g->players[0].inventory.weapons&=(uint16_t)~2u;
    CHECK(ssa_native_game_previous_weapon(g)==0 && g->gameplay.weapon.requested==3);
    g->game.updating=1;CHECK(ssa_native_game_previous_weapon(g)==-1);g->game.updating=0;
    CHECK(ssa_native_game_previous_weapon(NULL)==-1);
    ssa_native_game_free(next);free(next);ssa_native_game_free(g);free(g);
}
static void cancel_switch(const uint8_t *rom){
    SsaNativeGame *g=click_game(rom,3);unsigned i;
    g->players[0].inventory.ammo[1]=20;tap(g);CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_select_weapon(g,2)==1 && ssa_native_game_tick(g)==1);
    CHECK((g->gameplay.weapon.flags&0x400) && g->weapon.object.y==0x5000);
    CHECK(ssa_native_game_select_weapon(g,3)==1 && !g->weapon.switch_wait);
    for(i=0;i<180 && (g->gameplay.weapon.flags&0x400);++i){
        CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->gameplay.weapon.current==3 && g->gameplay.weapon.requested==3 && g->weapon.object.y==0x5000);
    }
    CHECK(i<180 && g->weapon_game.shots[2]==1 && !g->previous_weapon);
    ssa_native_game_free(g);free(g);
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned weapon,view;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    for(weapon=1;weapon<=10;++weapon)for(view=0;view<2;++view)switch_case(rom,weapon,view);
    history(rom);cancel_switch(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"switch_cases\":%u,\"waiting_frames\":%u,\"weapons\":10,\"views\":2}\n",
        checks,switch_cases,waiting_frames);
    return 0;
}
