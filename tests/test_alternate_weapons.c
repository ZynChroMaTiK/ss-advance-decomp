#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned checks,frames,pickups,shots,transitions,loadout_cases,reloads,reload_sounds,reequips,paired_clicks,shotgun_cases,pellets_checked,shotgun_timing_cases;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"alternate line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void audio_play(void *context,unsigned song){(void)context;(void)song;}
static void audio_stop(void *context,unsigned song){(void)context;(void)song;}
static void audio_source(void *context,SsaAudioSourceCommand command,unsigned song,uint64_t source,const SsaAudioPoint *point){
    SsaNativeGame *g=context;
    if(song!=0x28)return;
    CHECK(command==SSA_AUDIO_SOURCE_PLAY && source==1 && !point);
    CHECK(g->alternates.reload_phase==2 || g->alternates.reload_phase==3);++reload_sounds;
}
static void tick(SsaNativeGame *g){CHECK(ssa_native_game_tick(g)==1);++frames;}
static SsaNativeGame *start(const uint8_t *rom,unsigned enabled){
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i;
    CHECK(g && ssa_native_game_init_campaign(g,rom,0x800000,0,1,NULL)==1);
    CHECK(ssa_native_game_alternate_weapons(g,enabled,1)==1);
    g->players[0].inventory.health=32767;
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    for(i=0;i<30;++i)tick(g);
    return g;
}
static void dispose(SsaNativeGame *g){ssa_native_game_free(g);free(g);}
static unsigned add_colt(SsaNativeGame *g){
    unsigned slot;SsaScriptActor *a;
    CHECK(ssa_entity_pool_allocate(&g->spawn.pool,&slot)==1);a=&g->spawn.actors[slot];CHECK(!a->owned);
    memset(a,0,sizeof(*a));a->owned=1;a->activation.entity=&a->entity;a->entity.type=0xe1;a->animation=0x20;
    a->entity.body.x=g->players[0].entity.body.x;a->entity.body.y=g->players[0].entity.body.y;a->entity.body.z=g->players[0].entity.body.z;
    CHECK(ssa_world_insert(&g->world,&a->entity)==1);return slot;
}
static void pickup(SsaNativeGame *g,unsigned accepted){
    unsigned slot=add_colt(g),before=g->pickup.game.collected;SsaEntity *p=&g->players[0].entity;
    CHECK(ssa_world_query(&g->world,p,p->body.x,p->body.y)>=0);
    CHECK(g->pickup.game.collected==before+accepted);
    CHECK(g->spawn.actors[slot].owned==!accepted);
    if(!accepted)CHECK(ssa_script_spawn_release(&g->spawn,slot)==0);
    ++pickups;
}
static void weapon_pixels(SsaNativeGame *g,uint16_t *pixels){
    SsaSpriteDispatch d=g->sprites;SsaSpriteOam o=g->oam;uint8_t zero[14400]={0};uint16_t black[256]={0};
    d.active_count=1;d.active[0]=g->weapon.object.slot;
    CHECK(ssa_native_game_build_oam(g,&o,&d)==1);
    CHECK(ssa_display_compose_gameplay(&g->registers,zero,sizeof zero,black,g->sprite_assets.palette,&o,
        g->cache.tiles,sizeof g->cache.tiles,pixels,38400,100,0)==1);
}
static void word(FILE *f,unsigned n,unsigned bytes){while(bytes--){fputc(n&255,f);n>>=8;}}
static void preview(const char *directory,const char *name,SsaNativeGame *g){
    char path[2048];FILE *f;unsigned i;uint16_t pixels[38400];if(!directory)return;
    CHECK(snprintf(path,sizeof path,"%s/%s.bmp",directory,name)<(int)sizeof path);
    weapon_pixels(g,pixels);f=fopen(path,"wb");CHECK(f);
    fputs("BM",f);word(f,54+38400*4,4);word(f,0,4);word(f,54,4);word(f,40,4);word(f,240,4);word(f,(unsigned)-160,4);
    word(f,1,2);word(f,32,2);word(f,0,4);word(f,38400*4,4);word(f,0,4);word(f,0,4);word(f,0,4);word(f,0,4);
    for(i=0;i<38400;++i){unsigned c=pixels[i];word(f,(((c&31)*255/31)<<16)|((((c>>5)&31)*255/31)<<8)|(((c>>10)&31)*255/31),4);}
    CHECK(!fclose(f));
}
static void settle(SsaNativeGame *g,unsigned variant){
    unsigned i;for(i=0;i<240;++i){tick(g);if(!g->weapon.colt_pair_pending && !g->alternates.reload_phase && g->alternates.clock>=g->alternates.next_shot && g->gameplay.weapon.current==2 && g->weapon.object.y==0x5000 &&
       g->alternates.variant==variant && g->alternates.left_y==(variant?0x5000:0xdc00) && !(g->gameplay.weapon.flags&0x400) &&
       !(g->alternates.hands[0].animation==11 && !g->alternates.hands[0].done) &&
       !(g->alternates.hands[1].animation==11 && !g->alternates.hands[1].done))break;}
    CHECK(i<240);
}
static void toggle(SsaNativeGame *g,unsigned variant){
    unsigned i;uint16_t before[38400],after[38400];int32_t y=g->weapon.object.y;
    weapon_pixels(g,before);CHECK(ssa_native_game_select_weapon(g,2)==1 && g->alternates.requested_variant==variant);
    for(i=0;i<20;++i){unsigned p;tick(g);CHECK(g->gameplay.weapon.current==2 && g->weapon.object.y==y);
        weapon_pixels(g,after);for(p=0;p<38400;++p)if(p%240>=120)CHECK(before[p]==after[p]);
        if(g->alternates.variant==variant && g->alternates.left_y==(variant?0x5000:0xdc00))break;
    }
    CHECK(i>=15 && i<20);++transitions;
}
static void flow(const uint8_t *rom,const char *directory){
    SsaNativeGame *g=start(rom,0),*copy=calloc(1,sizeof(*copy));SsaNativeCarry carry;SsaPcRenderer renderer={0};
    uint16_t pixels[38400];unsigned i,count=0;
    CHECK(g->players[0].inventory.weapons==1 && !g->alternates.dual_colts);
    pickup(g,1);settle(g,0);CHECK(g->players[0].inventory.weapons&2);pickup(g,0);CHECK(!g->alternates.dual_colts);
    dispose(g);g=start(rom,1);
    CHECK(g->players[0].inventory.weapons==3 && g->gameplay.weapon.current==2 && !g->alternates.dual_colts);
    CHECK(ssa_native_game_select_weapon(g,2)==1 && !g->alternates.requested_variant);tick(g);
    pickup(g,1);CHECK(g->alternates.dual_colts && g->alternates.equip_dual);settle(g,1);
    pickup(g,0);weapon_pixels(g,pixels);
    for(i=0;i<38400;++i){CHECK(pixels[i]==pixels[i/240*240+239-i%240]);count+=pixels[i]!=0;}
    CHECK(count>100);preview(directory,"dual-idle",g);
    CHECK(ssa_native_game_draw(g)==1);
    CHECK(ssa_pc_renderer_resize(&renderer,480,320)==1 && ssa_pc_renderer_draw(&renderer,g)==1);
    toggle(g,0);preview(directory,"single-idle",g);toggle(g,1);
    CHECK(ssa_native_game_previous_weapon(g)==1 && !g->alternates.requested_variant);settle(g,0);
    CHECK(ssa_native_game_previous_weapon(g)==1 && g->alternates.requested_variant);settle(g,1);
    CHECK(ssa_native_game_cycle_weapon(g,-1)==1 && !g->alternates.requested_variant);
    CHECK(ssa_native_game_cycle_weapon(g,1)==1 && g->alternates.requested_variant);
    CHECK(ssa_native_game_cycle_weapon(g,1)==1 && g->gameplay.weapon.requested==1);
    CHECK(ssa_native_game_cycle_weapon(g,1)==1 && g->gameplay.weapon.requested==2 && !g->alternates.requested_variant);
    CHECK(ssa_native_game_cycle_weapon(g,1)==1 && g->alternates.requested_variant);settle(g,1);
    CHECK(ssa_native_game_carry_capture(&carry,g)==1 && carry.dual_colts && carry.weapon_variant==1);
    CHECK(copy && ssa_native_game_init_campaign(copy,rom,0x800000,0,1,&carry)==1);
    CHECK(copy->alternates.enabled && copy->alternates.dual_colts && copy->alternates.variant==1);
    copy->weapon.object.y=0x5000;tick(copy);
    CHECK(copy->weapon.object.screen_y==80); /* First reveal after a dual carry. */
    dispose(copy);copy=calloc(1,sizeof(*copy));g->finished=1;
    CHECK(ssa_native_game_carry_next(&carry,g)==1 && carry.dual_colts && carry.weapon_variant==1);
    CHECK(copy && ssa_native_game_init_campaign(copy,rom,0x800000,1,1,&carry)==1);
    CHECK(copy->alternates.enabled && copy->alternates.dual_colts && copy->alternates.variant==1);dispose(copy);g->finished=0;
    CHECK(ssa_native_game_alternate_weapons(g,0,0)==1 && !g->alternates.variant && !g->pickup.repeat_weapon);
    CHECK(ssa_native_game_select_weapon(g,2)==1 && !g->alternates.requested_variant);
    ssa_pc_renderer_free(&renderer);dispose(g);
}
static void firing(const uint8_t *rom,const char *directory){
    SsaNativeGame *single=start(rom,1),*dual=start(rom,1);
    unsigned i,before_dual,starts=0,last_left=1,single_count=0,hand_count[2]={0},overlap=0;
    uint64_t single_times[6],hand_times[2][6];
    pickup(dual,1);settle(dual,1);settle(single,0);
    single->players[0].inventory.ammo[1]=dual->players[0].inventory.ammo[1]=300;
    single->weapon.controller.cooldown[7]=dual->weapon.controller.cooldown[7]=600;
    ssa_pc_input_runtime_button(&single->input_runtime,1,1);ssa_pc_input_runtime_button(&dual->input_runtime,1,1);
    for(i=0;i<160;++i){unsigned old=single->alternates.shot_serial,old_dual=dual->alternates.shot_serial;
        tick(single);tick(dual);
        CHECK(single->weapon.controller.cooldown[7]==dual->weapon.controller.cooldown[7]);
        CHECK(single->players[0].inventory.ammo[1]==999 && dual->players[0].inventory.ammo[1]==999); /* Original infinite-Colt HUD rule. */
        if(old!=single->alternates.shot_serial){
            CHECK(single_count<6);single_times[single_count++]=i*20;
        }
        if(dual->alternates.hands[0].animation==11 && !dual->alternates.hands[0].done && (single->gameplay.weapon.flags&0x400) && !dual->alternates.reload_phase)
            CHECK(dual->alternates.hands[0].frame_index==single->weapon.object.frame_index);
        overlap+=(dual->alternates.hands[0].animation==11 && !dual->alternates.hands[0].done &&
                  dual->alternates.hands[1].animation==11 && !dual->alternates.hands[1].done);
        if(old_dual!=dual->alternates.shot_serial){
            CHECK(dual->alternates.firing_left!=last_left);last_left=dual->alternates.firing_left;++starts;
            CHECK(hand_count[last_left]<6);hand_times[last_left][hand_count[last_left]++]=i*20;
            if(last_left && starts==2){unsigned p,mirrored=0;
                preview(directory,"left-firing",dual);CHECK(ssa_native_game_draw(dual)==1);
                for(p=0;p<dual->actor_scene.queue.projection_count;++p){const SsaActorProjection *a=&dual->actor_scene.queue.projections[p];
                    if(a->animation==dual->particle_rules.animation[8] && (a->frame_flags&UINT32_C(0x80000000)))++mirrored;
                }
                CHECK(mirrored>0);
            }
        }
    }
    CHECK(single_count==6 && hand_count[0]==6 && hand_count[1]==6 && overlap>0);
    for(i=0;i<6;++i){CHECK(hand_times[0][i]==single_times[i]);
        CHECK(single_times[i]-single_times[0]>=i*540 && single_times[i]-single_times[0]<i*540+20);
        CHECK(hand_times[1][i]-hand_times[0][0]>=(uint64_t)i*540+270);
        CHECK(hand_times[1][i]-hand_times[0][0]<(uint64_t)i*540+290);}
    CHECK(dual->weapon_game.shots[1]==12 && single->weapon_game.shots[1]==6);shots+=starts;
    ssa_pc_input_runtime_button(&single->input_runtime,1,0);ssa_pc_input_runtime_button(&dual->input_runtime,1,0);
    settle(dual,1);
    /* A variant change waits through the shot; a quick click during the left
     * hand transition remains buffered until that transition completes. */
    before_dual=dual->weapon_game.shots[1];
    ssa_pc_input_runtime_button(&dual->input_runtime,1,1);tick(dual);ssa_pc_input_runtime_button(&dual->input_runtime,1,0);
    CHECK(dual->gameplay.weapon.flags&0x400);
    CHECK(ssa_native_game_select_weapon(dual,2)==1);
    for(i=0;i<3;++i){tick(dual);CHECK(dual->alternates.left_y==0x5000 && dual->weapon.object.y==0x5000);}
    settle(dual,0);CHECK(dual->weapon_game.shots[1]==before_dual+1);
    CHECK(ssa_native_game_select_weapon(dual,2)==1);tick(dual);
    ssa_pc_input_runtime_button(&dual->input_runtime,1,1);tick(dual);ssa_pc_input_runtime_button(&dual->input_runtime,1,0);
    before_dual=dual->weapon_game.shots[1];
    for(i=0;i<60 && dual->weapon_game.shots[1]==before_dual;++i)tick(dual);
    CHECK(i<60 && dual->alternates.variant==1 && dual->alternates.left_y==0x5000);
    settle(dual,1);
    ssa_pc_input_runtime_button(&dual->input_runtime,1,1);
    before_dual=dual->alternates.shot_serial;
    for(i=0;i<80 && dual->alternates.shot_serial<before_dual+2;++i)tick(dual);
    CHECK(dual->alternates.hands[0].animation==11 && !dual->alternates.hands[0].done &&
          dual->alternates.hands[1].animation==11 && !dual->alternates.hands[1].done);
    {SsaWeaponObject paused[2];memcpy(paused,dual->alternates.hands,sizeof paused);
        dual->frame_game.flags|=1;tick(dual);CHECK(!memcmp(paused,dual->alternates.hands,sizeof paused));
        dual->frame_game.flags&=UINT16_C(0xfffe);
    }
    CHECK(ssa_native_game_select_weapon(dual,1)==1);before_dual=dual->alternates.shot_serial;
    for(i=0;i<60 && ((dual->alternates.hands[0].animation==11 && !dual->alternates.hands[0].done) ||
        (dual->alternates.hands[1].animation==11 && !dual->alternates.hands[1].done));++i){
        tick(dual);CHECK(dual->gameplay.weapon.current==2 && dual->weapon.object.y==0x5000);
    }
    CHECK(i<60);
    for(i=0;i<80 && (dual->gameplay.weapon.current!=1 || dual->weapon.object.y!=0x5000);++i)tick(dual);
    CHECK(i<80 && dual->alternates.shot_serial==before_dual);
    dispose(single);dispose(dual);
}
static void loadouts(const uint8_t *rom){
    unsigned setting,enabled;SsaNativeGame *g,*next;SsaNativeCarry carry;
    for(setting=1;setting<=36;++setting)for(enabled=0;enabled<2;++enabled){
        SsaInventory inventory;unsigned current,expected;
        g=calloc(1,sizeof(*g));CHECK(g && ssa_native_game_init_loadout(g,rom,0x800000,(setting-1)%12,setting)==1);
        inventory=g->players[0].inventory;current=g->gameplay.weapon.current;
        expected=enabled && g->level!=0 && (inventory.weapons&2)!=0;
        CHECK(ssa_native_game_alternate_weapons(g,enabled,SSA_ALTERNATE_LOADOUT)==1);
        CHECK(!memcmp(&inventory,&g->players[0].inventory,sizeof inventory));
        CHECK(g->gameplay.weapon.current==current && g->gameplay.weapon.requested==current);
        CHECK(g->alternates.dual_colts==expected);
        CHECK(g->alternates.variant==(expected && current==2));
        CHECK(g->alternates.requested_variant==(expected && current==2));
        CHECK(ssa_native_game_carry_capture(&carry,g)==1 && carry.dual_colts==expected);
        if(current!=2 && (inventory.weapons&2)){
            CHECK(ssa_native_game_select_weapon(g,2)==1 && g->alternates.requested_variant==expected);
        }
        dispose(g);++loadout_cases;
    }
    /* A carried single Colt remains single; only standalone loadouts upgrade. */
    g=start(rom,1);CHECK(ssa_native_game_carry_capture(&carry,g)==1 && !carry.dual_colts);
    next=calloc(1,sizeof(*next));CHECK(next && ssa_native_game_init_campaign(next,rom,0x800000,1,1,&carry)==1);
    CHECK(ssa_native_game_alternate_weapons(next,1,SSA_ALTERNATE_RETAIN)==1 && !next->alternates.dual_colts);
    dispose(next);dispose(g);
}
static void magazines(const uint8_t *rom,const char *directory){
    unsigned dual_mode;
    for(dual_mode=0;dual_mode<2;++dual_mode){
        SsaNativeGame *g=start(rom,1);unsigned i,phase=0,seen[4]={0},old_shots=0,paused=0,burst=0,right_done=0;
        uint64_t began=0;uint16_t idle[38400],image[38400];SsaNativeCarry carry;
        if(dual_mode){pickup(g,1);settle(g,1);}else settle(g,0);
        {SsaNativeAudioOps audio={0};audio.play=audio_play;audio.stop=audio_stop;audio.source=audio_source;audio.context=g;
            CHECK(ssa_native_game_bind_audio(g,&audio)==1);}
        weapon_pixels(g,idle);ssa_pc_input_runtime_button(&g->input_runtime,1,1);
        for(i=0;i<850;++i){unsigned old_phase=g->alternates.reload_phase,serial=g->alternates.shot_serial,sounds=reload_sounds;
            tick(g);phase=g->alternates.reload_phase;
            if(serial!=g->alternates.shot_serial)++burst;
            CHECK(reload_sounds==sounds+(phase!=old_phase && phase>=2));
            CHECK(g->alternates.rounds[0]<=6 && g->alternates.rounds[1]<=6);
            if(old_phase>=2 && phase==old_phase)CHECK(serial==g->alternates.shot_serial);
            if(phase!=old_phase){
                if(phase==1){CHECK(g->alternates.rounds[0]==6);CHECK(g->alternates.rounds[1]==(dual_mode?6u:0u));
                    CHECK(burst==(dual_mode?12u:6u));burst=0;}
                if(phase==2){CHECK(old_phase==1);began=g->alternates.clock;right_done=0;++seen[2];}
                if(phase==3){
                    CHECK(old_phase==2 && dual_mode && g->alternates.rounds[0]==6);
                    CHECK(g->alternates.clock-began>=396 && g->alternates.clock-began<416);
                    ++seen[3];
                }
                if(!phase && old_phase>=2){
                    unsigned duration=dual_mode?1680:1284;
                    CHECK(g->alternates.clock-began>=duration && g->alternates.clock-began<duration+20);
                    CHECK(!dual_mode || right_done);
                    CHECK(!g->alternates.rounds[1]);++reloads;
                    if(dual_mode)++reloads;
                    if(g->alternates.shot_serial!=serial)CHECK(g->alternates.firing_left==0);
                }
            }
            if(phase==3 && !g->alternates.rounds[0] && !right_done){
                CHECK(g->alternates.clock-began>=1284 && g->alternates.clock-began<1304);right_done=1;
            }
            if(phase>=2 && ((g->alternates.clock-began>=240 && g->alternates.clock-began<260) ||
                (dual_mode && g->alternates.clock-began>=1320 && g->alternates.clock-began<1340))){unsigned p;
                weapon_pixels(g,image);CHECK(memcmp(image,idle,sizeof idle));
                for(p=0;p<38400;++p)if((phase==2 && p%240<120) || (phase==3 && p%240>=120))CHECK(image[p]==idle[p]);
                preview(directory,phase==2?(dual_mode?"dual-reload-right":"single-reload"):"dual-reload-left",g);
                if(!paused){uint64_t clock=g->alternates.clock,end=g->alternates.reload_end;unsigned pause_sounds=reload_sounds;
                    g->frame_game.flags|=1;tick(g);tick(g);CHECK(g->alternates.clock==clock && g->alternates.reload_end==end);
                    CHECK(reload_sounds==pause_sounds);
                    g->frame_game.flags&=UINT16_C(0xfffe);paused=1;
                    CHECK(ssa_native_game_carry_capture(&carry,g)==1 && carry.colt_rounds[0]==6);
                    /* Pausing intentionally requires release before firing. */
                    ssa_pc_input_runtime_button(&g->input_runtime,1,0);
                    ssa_pc_input_runtime_button(&g->input_runtime,1,1);
                }
            }
            if(phase==3 && g->alternates.clock-began>=720 && g->alternates.clock-began<740){unsigned p,changed[2]={0};
                weapon_pixels(g,image);for(p=0;p<38400;++p)changed[p%240>=120]+=image[p]!=idle[p];
                CHECK(changed[0]>0 && changed[1]>0);preview(directory,"dual-reload-overlap",g);
            }
            old_shots=g->alternates.shot_serial;
        }
        if(!(seen[2]>=2 && (!dual_mode || seen[3]>=2) && old_shots>12 && paused))
            fprintf(stderr,"magazine mode%u: right%u left%u shots%u paused%u phase%u held%u flags%x clock%llu end%llu\n",dual_mode,seen[2],seen[3],old_shots,paused,g->alternates.reload_phase,g->weapon.controller.held,g->weapon_game.flags,(unsigned long long)g->alternates.clock,(unsigned long long)g->alternates.reload_end);
        CHECK(seen[2]>=2 && (!dual_mode || seen[3]>=2) && old_shots>12 && paused);
        ssa_pc_input_runtime_button(&g->input_runtime,1,0);settle(g,dual_mode);
        /* The magazine is tied to the hand, not to single/dual selection. */
        CHECK(ssa_native_game_carry_capture(&carry,g)==1);
        {SsaNativeGame *copy=calloc(1,sizeof(*copy));CHECK(copy && ssa_native_game_init_campaign(copy,rom,0x800000,1,1,&carry)==1);
            CHECK(!memcmp(copy->alternates.rounds,g->alternates.rounds,sizeof carry.colt_rounds));dispose(copy);}
        if(dual_mode){
            g->alternates.rounds[0]=0;g->alternates.rounds[1]=5;g->alternates.next_left=1;
            ssa_pc_input_runtime_button(&g->input_runtime,1,1);
            for(i=0;i<80 && g->alternates.reload_phase!=2;++i)tick(g);
            CHECK(i<80 && g->alternates.rounds[0]==1 && g->alternates.rounds[1]==6);
            CHECK(ssa_native_game_alternate_weapons(g,0,SSA_ALTERNATE_RETAIN)==1);
            CHECK(!g->alternates.reload_phase);
        }
        dispose(g);
    }
    {SsaNativeGame *g=start(rom,0);unsigned i;pickup(g,1);settle(g,0);
        ssa_pc_input_runtime_button(&g->input_runtime,1,1);for(i=0;i<260;++i)tick(g);
        CHECK(g->weapon_game.shots[1]>6 && !g->alternates.reload_phase && !g->alternates.rounds[0]);dispose(g);}
}
static void reequip(const uint8_t *rom){
    SsaNativeGame *g=start(rom,1);unsigned i,before,right;
    pickup(g,1);settle(g,1);
    before=g->alternates.shot_serial;ssa_pc_input_runtime_button(&g->input_runtime,1,1);
    for(i=0;i<100 && g->alternates.shot_serial<before+4;++i)tick(g);
    ssa_pc_input_runtime_button(&g->input_runtime,1,0);settle(g,1);
    CHECK(g->alternates.rounds[0]==2 && g->alternates.rounds[1]==2);
    right=g->alternates.rounds[0];toggle(g,0);CHECK(g->alternates.rounds[0]==right);
    toggle(g,1);CHECK(g->alternates.rounds[0]==right && !g->alternates.rounds[1]);++reequips;
    CHECK(ssa_native_game_select_weapon(g,1)==1);
    for(i=0;i<100 && (g->gameplay.weapon.current!=1 || g->weapon.object.y!=0x5000);++i)tick(g);
    CHECK(i<100 && g->alternates.rounds[0]==right);
    CHECK(ssa_native_game_select_weapon(g,2)==1);settle(g,1);
    CHECK(!g->alternates.rounds[0] && !g->alternates.rounds[1]);++reequips;
    toggle(g,0);ssa_pc_input_runtime_button(&g->input_runtime,1,1);before=g->alternates.shot_serial;
    for(i=0;i<100 && g->alternates.shot_serial<before+2;++i)tick(g);
    ssa_pc_input_runtime_button(&g->input_runtime,1,0);settle(g,0);CHECK(g->alternates.rounds[0]==2);
    CHECK(ssa_native_game_select_weapon(g,1)==1);
    for(i=0;i<100 && (g->gameplay.weapon.current!=1 || g->weapon.object.y!=0x5000);++i)tick(g);
    CHECK(i<100);CHECK(ssa_native_game_select_weapon(g,2)==1);
    /* Explicitly choose single while the whole Colt weapon is being drawn. */
    CHECK(ssa_native_game_select_weapon(g,2)==1);settle(g,0);
    CHECK(!g->alternates.rounds[0] && !g->alternates.rounds[1]);++reequips;
    CHECK(ssa_native_game_select_weapon(g,1)==1);
    for(i=0;i<100 && (g->gameplay.weapon.current!=1 || g->weapon.object.y!=0x5000);++i)tick(g);
    CHECK(i<100);
    /* An empty carried magazine can have a deferred reload while unequipped. */
    g->alternates.rounds[0]=6;g->alternates.rounds[1]=5;
    g->alternates.reload_phase=1;g->alternates.reload_dual=1;
    CHECK(ssa_native_game_select_weapon(g,2)==1);settle(g,1);
    CHECK(!g->alternates.rounds[0] && !g->alternates.rounds[1] && !g->alternates.reload_phase);
    ++reequips;dispose(g);
}
static void tap(SsaNativeGame *g){
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
}
static void paired_input(const uint8_t *rom){
    SsaNativeGame *g=start(rom,1);unsigned click,i,before,old,starts,hand[2]={0};uint64_t times[2]={0};
    pickup(g,1);settle(g,1);
    /* Six released clicks empty all twelve rounds, not six. Inspect actual
     * controller starts and damage/stat commits, including a sub-tick click. */
    for(click=0;click<6;++click){
        before=g->weapon_game.shots[1];starts=0;tap(g);
        for(i=0;i<65;++i){old=g->alternates.shot_serial;tick(g);
            if(old!=g->alternates.shot_serial){CHECK(starts<2);hand[starts]=g->alternates.firing_left;times[starts++]=g->alternates.clock;}
            if(starts==2 && !(g->gameplay.weapon.flags&0x400) &&
               !(g->alternates.hands[1].animation==11 && !g->alternates.hands[1].done))break;
        }
        CHECK(i<65 && starts==2 && hand[0]==0 && hand[1]==1);
        CHECK(times[1]-times[0]>=270 && times[1]-times[0]<290);
        CHECK(g->weapon_game.shots[1]==before+2 && !g->weapon.colt_pair_pending);
        if(click<5)CHECK(g->alternates.rounds[0]==click+1 && g->alternates.rounds[1]==click+1 && !g->alternates.reload_phase);
        ++paired_clicks;
    }
    CHECK(g->alternates.rounds[0]==6 && g->alternates.rounds[1]==6 && g->alternates.reload_phase);
    /* Clicking again during reload retains both shots after the mouse release. */
    before=g->weapon_game.shots[1];tap(g);settle(g,1);
    CHECK(g->weapon_game.shots[1]==before+2 && g->alternates.rounds[0]==1 && g->alternates.rounds[1]==1);++paired_clicks;
    /* A second quick click while the first pair is pending must not vanish. */
    before=g->weapon_game.shots[1];tap(g);tick(g);CHECK(g->weapon.colt_pair_pending==1);
    tap(g);settle(g,1);CHECK(g->weapon_game.shots[1]==before+4);paired_clicks+=2;
    /* Focus loss and pause cancel an unfinished pair, just like click buffering. */
    before=g->weapon_game.shots[1];tap(g);tick(g);CHECK(g->weapon.colt_pair_pending==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,0);CHECK(!g->weapon.colt_pair_pending);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);settle(g,1);CHECK(g->weapon_game.shots[1]==before+1);
    before=g->weapon_game.shots[1];tap(g);tick(g);CHECK(g->weapon.colt_pair_pending==1);
    g->frame_game.flags|=1;tick(g);CHECK(!g->weapon.colt_pair_pending);
    g->frame_game.flags&=UINT16_C(0xfffe);settle(g,1);CHECK(g->weapon_game.shots[1]==before+1);
    toggle(g,0);before=g->weapon_game.shots[1];tap(g);settle(g,0);
    CHECK(g->weapon_game.shots[1]==before+1); /* Single is still one shot. */
    CHECK(ssa_native_game_alternate_weapons(g,0,SSA_ALTERNATE_RETAIN)==1);
    before=g->weapon_game.shots[1];tap(g);settle(g,0);CHECK(g->weapon_game.shots[1]==before+1);
    dispose(g);
}
static struct {
    SsaNativeGame *game;unsigned count;double ray[14][3];
    int (*impact)(void *,const SsaWeaponRuntime *,const SsaWeaponHit *);
} pattern_capture;
static int pattern_impact(void *context,const SsaWeaponRuntime *r,const SsaWeaponHit *hit){
    unsigned n=pattern_capture.count++;double yaw,slope,horizontal;
    CHECK(context==pattern_capture.game && n<14);
    yaw=atan2((double)r->trace.traversal.ray.dx,r->trace.traversal.ray.dy);
    slope=r->trace.marker/65536.0;horizontal=1/sqrt(1+slope*slope);
    pattern_capture.ray[n][0]=sin(yaw)*horizontal;pattern_capture.ray[n][1]=cos(yaw)*horizontal;pattern_capture.ray[n][2]=slope*horizontal;
    return pattern_capture.impact(context,r,hit);
}
static void shotgun_pattern(const uint8_t *rom){
    static const double offsets[14][2]={
        {-5.14,2.58},{0,2.58},{5.14,2.58},
        {-6.84,.86},{-1.72,.86},{1.72,.86},{6.84,.86},
        {-5.14,-.86},{0,-.86},{5.14,-.86},
        {-6.84,-2.58},{-1.72,-2.58},{1.72,-2.58},{6.84,-2.58}};
    static const int pitches[6]={0,5440,-5440,14500,0,0};unsigned mode;
    const double pi=3.14159265358979323846;
    for(mode=0;mode<8;++mode){
        unsigned enabled=mode<6,i,slot;SsaScriptActor *target;int32_t sy,cy;uint16_t yaw;
        SsaNativeGame *g=start(rom,enabled);uint32_t before_hits,before_shots;double pitch;
        g->players[0].inventory.weapons|=4;g->players[0].inventory.ammo[2]=100;
        CHECK(ssa_native_game_select_weapon(g,3)==1);
        for(i=0;i<100 && (g->gameplay.weapon.current!=3 || g->weapon.object.y!=0x5000);++i)tick(g);
        CHECK(i<100 && g->weapon.alternate_shotgun==enabled);
        yaw=(uint16_t)(g->players[0].motion.movement.heading+mode*8192);
        g->players[0].motion.movement.heading=yaw;g->players[0].motion.facing=(uint8_t)(yaw>>8);
        g->gba_view=mode>=4;g->player.free_aim=mode<4;g->player.pitch=mode<6?pitches[mode]:0;
        g->damage.difficulty=1;
        CHECK(ssa_entity_pool_allocate(&g->spawn.pool,&slot)==1);target=&g->spawn.actors[slot];
        CHECK(!target->owned);memset(target,0,sizeof(*target));target->owned=1;
        target->activation.entity=&target->entity;target->activation.health=200;target->activation.state=1;
        target->entity.type=0xcd;target->entity.body.flags=2;target->entity.radius=16*65536;
        ssa_sincos(&g->math,yaw,&sy,&cy);
        target->entity.body.x=g->players[0].entity.body.x+sy*24;target->entity.body.y=g->players[0].entity.body.y+cy*24;
        target->entity.body.z=g->players[0].entity.body.z-32*65536;target->entity.body.height=256*65536;
        CHECK(ssa_world_insert(&g->world,&target->entity)==1);
        pattern_capture.game=g;pattern_capture.count=0;pattern_capture.impact=g->weapon_ops.impact;g->weapon_ops.impact=pattern_impact;
        before_hits=g->weapon.hits[2];before_shots=g->weapon_game.shots[2];
        CHECK(ssa_weapon_runtime_step(&g->weapon,&g->weapon_game,1,1,&g->weapon_ops)==0);
        CHECK(g->gameplay.weapon.flags&0x400);
        CHECK(ssa_weapon_runtime_animate(&g->weapon,0,1)==0);
        CHECK(ssa_weapon_runtime_step(&g->weapon,&g->weapon_game,0,0,&g->weapon_ops)==0);
        CHECK(pattern_capture.count==(enabled?14u:3u));
        CHECK(g->weapon_game.shots[2]==before_shots+1 && g->players[0].inventory.ammo[2]==98);
        if(enabled){
            double angle=yaw*2*pi/65536,forward[3],right[3],up[3];
            pitch=g->player.free_aim?g->player.pitch*2*pi/65536:atan(g->weapon.controller.aim_slope/65536.0);
            forward[0]=sin(angle)*cos(pitch);forward[1]=cos(angle)*cos(pitch);forward[2]=sin(pitch);
            right[0]=cos(angle);right[1]=-sin(angle);right[2]=0;
            up[0]=-sin(angle)*sin(pitch);up[1]=-cos(angle)*sin(pitch);up[2]=cos(pitch);
            for(i=0;i<14;++i){unsigned j;double h=offsets[i][0]*pi/180,v=offsets[i][1]*pi/180,dot=0;
                for(j=0;j<3;++j){double expected=forward[j]*cos(v)*cos(h)+right[j]*cos(v)*sin(h)+up[j]*sin(v);
                    dot+=expected*pattern_capture.ray[i][j];}
                if(dot>1)dot=1;
                CHECK(acos(dot)*180/pi<.08);++pellets_checked;
            }
            if(!mode){CHECK(target->activation.health==130);CHECK(g->weapon.hits[2]==before_hits+14);}
        }else{CHECK(target->activation.health==160);CHECK(g->weapon.hits[2]==before_hits+3);}
        g->weapon_ops.impact=pattern_capture.impact;++shotgun_cases;dispose(g);
    }
}
static void shotgun_timing(const uint8_t *rom){
    unsigned mode;
    for(mode=0;mode<3;++mode){
        SsaNativeGame *g=start(rom,1);unsigned i,starts=0,dwell[17]={0},count;
        g->players[0].inventory.weapons|=4;g->players[0].inventory.ammo[2]=(int16_t)(mode==2?1:200);
        CHECK(ssa_native_game_select_weapon(g,3)==1);
        for(i=0;i<100 && (g->gameplay.weapon.current!=3 || g->weapon.object.y!=0x5000);++i)tick(g);
        CHECK(i<100);g->gba_view=mode==1;
        if(mode==1){ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);tick(g);}
        count=g->weapon_animations.lengths[mode==2?22:12]-1u;
        ssa_pc_input_runtime_button(&g->input_runtime,1,1);
        for(i=0;i<(mode==2?150u:316u);++i){
            tick(g);
            if(g->weapon.shotgun_animating){
                unsigned elapsed=g->weapon.shotgun_elapsed,position=elapsed==105?count-1:elapsed*count/105;
                CHECK(elapsed>=1 && elapsed<=105 && g->weapon.object.frame_index==position);
                CHECK(g->weapon.object.done==(elapsed==105));
                if(elapsed==1){CHECK(i==starts*105);++starts;}
                if(i<105)++dwell[position];
            }
            if(i<105)CHECK(g->gameplay.weapon.current==3 && g->weapon.object.y==0x5000);
            if(i==50){unsigned elapsed=g->weapon.shotgun_elapsed,position=g->weapon.object.frame_index;
                g->frame_game.flags|=1;tick(g);tick(g);
                CHECK(g->weapon.shotgun_elapsed==elapsed && g->weapon.object.frame_index==position);
                g->frame_game.flags&=UINT16_C(0xfffe);
                ssa_pc_input_runtime_button(&g->input_runtime,1,0);ssa_pc_input_runtime_button(&g->input_runtime,1,1);
            }
        }
        CHECK(starts==(mode==2?1u:4u));
        for(i=0;i<count;++i)CHECK(dwell[i]>=105/count && dwell[i]<=(105+count-1)/count);
        if(mode==2)CHECK(g->gameplay.weapon.current!=3); /* Last shell's shorter artwork still occupies the full cycle. */
        else{
            ssa_pc_input_runtime_button(&g->input_runtime,1,0);
            CHECK(ssa_native_game_select_weapon(g,1)==1);
            for(i=0;i<104;++i){tick(g);CHECK(g->gameplay.weapon.current==3 && g->weapon.object.y==0x5000);}
            for(i=0;i<80 && g->gameplay.weapon.current!=1;++i)tick(g);
            CHECK(i<80);
        }
        ++shotgun_timing_cases;dispose(g);
    }
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;
    CHECK(argc>=2 && rom && (f=fopen(argv[1],"rb")));CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    flow(rom,argc==3?argv[2]:NULL);firing(rom,argc==3?argv[2]:NULL);loadouts(rom);magazines(rom,argc==3?argv[2]:NULL);reequip(rom);paired_input(rom);shotgun_pattern(rom);shotgun_timing(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"frames\":%u,\"pickups\":%u,\"alternating_shots\":%u,\"left_hand_transitions\":%u,\"loadout_cases\":%u,\"reloads\":%u,\"reload_sounds\":%u,\"reequips\":%u,\"paired_clicks\":%u,\"shotgun_cases\":%u,\"pellets_checked\":%u,\"shotgun_timing_cases\":%u,\"shotgun_interval_ms\":1750,\"colt_shot_ms\":450,\"colt_reload_ms\":1070,\"dual_reload_ms\":1400}\n",checks,frames,pickups,shots,transitions,loadout_cases,reloads,reload_sounds,reequips,paired_clicks,shotgun_cases,pellets_checked,shotgun_timing_cases);return 0;
}
