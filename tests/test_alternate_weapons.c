#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned checks,frames,pickups,shots,transitions,loadout_cases,reloads,reload_sounds,reequips,paired_clicks,shotgun_cases,pellets_checked,shotgun_timing_cases,shotgun_reload_sounds;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"alternate line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void audio_play(void *context,unsigned song){(void)context;(void)song;}
static void audio_stop(void *context,unsigned song){(void)context;(void)song;}
static void audio_source(void *context,SsaAudioSourceCommand command,unsigned song,uint64_t source,const SsaAudioPoint *point){
    SsaNativeGame *g=context;
    if(song!=0x28)return;
    CHECK(command==SSA_AUDIO_SOURCE_PLAY && source==1 && !point);
    if(g->gameplay.weapon.current==3){
        if(g->alternates.shotgun_inserting){
            CHECK(g->alternates.enabled && g->weapon.shotgun_animating &&
                g->weapon.object.animation==12 && (g->weapon.object.frame&0x3fff)==23);
            ++shotgun_reload_sounds;
        }
        return;
    }
    CHECK(g->alternates.reload_phase==2 || g->alternates.reload_phase==3);++reload_sounds;
}
static void tick(SsaNativeGame *g){CHECK(ssa_native_game_tick(g)==1);++frames;}
static SsaNativeGame *start(const uint8_t *rom,unsigned enabled){
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i;
    CHECK(g && ssa_native_game_init_campaign(g,rom,0x800000,0,1,NULL)==1);
    if(enabled<2)CHECK(ssa_native_game_alternate_weapons(g,enabled,1)==1);
    g->players[0].inventory.health=32767;
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    for(i=0;i<30;++i)tick(g);
    return g;
}
static void dispose(SsaNativeGame *g){ssa_native_game_free(g);free(g);}
static unsigned add_pickup(SsaNativeGame *g,unsigned item){
    unsigned slot;SsaScriptActor *a;
    CHECK(ssa_entity_pool_allocate(&g->spawn.pool,&slot)==1);a=&g->spawn.actors[slot];CHECK(!a->owned);
    memset(a,0,sizeof(*a));a->owned=1;a->activation.entity=&a->entity;a->entity.type=0xe1;a->animation=(uint16_t)item;
    a->entity.body.x=g->players[0].entity.body.x;a->entity.body.y=g->players[0].entity.body.y;a->entity.body.z=g->players[0].entity.body.z;
    CHECK(ssa_world_insert(&g->world,&a->entity)==1);return slot;
}
static void pickup_item(SsaNativeGame *g,unsigned item,unsigned accepted){
    unsigned slot=add_pickup(g,item),before=g->pickup.game.collected;SsaEntity *p=&g->players[0].entity;
    CHECK(ssa_world_query(&g->world,p,p->body.x,p->body.y)>=0);
    CHECK(g->pickup.game.collected==before+accepted);
    CHECK(g->spawn.actors[slot].owned==!accepted);
    if(!accepted)CHECK(ssa_script_spawn_release(&g->spawn,slot)==0);
    ++pickups;
}
static void pickup(SsaNativeGame *g,unsigned accepted){pickup_item(g,0x20,accepted);}
static SsaNativeGame *pickup_start(const uint8_t *rom,unsigned mode){
    /* Cover untouched default startup, enabled alternates, and disabling the
     * option through the same settings application used by the frontend. */
    SsaNativeGame *g=start(rom,mode?1:2);SsaSettings settings=SSA_SETTINGS_DEFAULT;
    if(mode==2)CHECK(ssa_native_game_apply_settings(g,&settings)==1);
    CHECK(g->alternates.enabled==(mode==1));return g;
}
static void pc_pickups(const uint8_t *rom){
    static const unsigned ammo_items[8]={0x35,0x3a,0x3b,0x38,0x3d,0x36,0x3c,0x39};
    static const unsigned weapon_items[8]={0x21,0x22,0x25,0x23,0x29,0x27,0x28,0x26};
    static const unsigned amounts[8]={10,50,50,5,5,1,5,5};
    unsigned mode,i;
    for(mode=0;mode<3;++mode){SsaNativeGame *g=pickup_start(rom,mode);SsaInventory *inv=&g->players[0].inventory;
        CHECK(g->pickup.pc_pickups && g->pickup.collect_owned_weapons);
        for(i=0;i<8;++i){unsigned weapon=i+3,bit=1u<<(weapon-1),limit=g->inventory_rules.ammo_capacity[weapon-1];
            unsigned expected,current,requested,wait;
            CHECK(!(inv->weapons&bit));inv->ammo[weapon-1]=0;
            pickup_item(g,ammo_items[i],1);
            CHECK((unsigned)inv->ammo[weapon-1]==amounts[i] && !(inv->weapons&bit) && !inv->pending_count);
            if(!i){SsaNativeCarry carry;SsaNativeGame *next=calloc(1,sizeof(*next));
                CHECK(next && ssa_native_game_carry_capture(&carry,g)==1);
                CHECK(ssa_native_game_init_campaign(next,rom,0x800000,0,1,&carry)==1);
                CHECK(next->pickup.pc_pickups && next->pickup.collect_owned_weapons && (unsigned)next->players[0].inventory.ammo[2]==amounts[0] &&
                    !(next->players[0].inventory.weapons&4));dispose(next);}
            inv->ammo[weapon-1]=(int16_t)(limit-1);pickup_item(g,ammo_items[i],1);
            CHECK((unsigned)inv->ammo[weapon-1]==limit);pickup_item(g,ammo_items[i],0);
            inv->ammo[weapon-1]=(int16_t)amounts[i];pickup_item(g,weapon_items[i],1);
            for(wait=0;wait<100 && inv->pending_count;++wait)tick(g);
            expected=amounts[i]+limit/2;if(expected>limit)expected=limit;
            CHECK(wait<100 && (inv->weapons&bit) && (unsigned)inv->ammo[weapon-1]==expected && !inv->pending_count);
            /* Test duplicates while this very weapon is actually equipped. */
            CHECK(ssa_native_game_select_weapon(g,weapon)==1);
            for(wait=0;wait<200 && (g->gameplay.weapon.current!=weapon || g->weapon.object.y!=0x5000);++wait)tick(g);
            CHECK(wait<200 && g->gameplay.weapon.current==weapon);
            current=g->gameplay.weapon.current;requested=g->gameplay.weapon.requested;
            inv->ammo[weapon-1]=0;pickup_item(g,weapon_items[i],1);
            CHECK((unsigned)inv->ammo[weapon-1]==limit/2 && !inv->pending_count &&
                g->gameplay.weapon.current==current && g->gameplay.weapon.requested==requested);
            inv->ammo[weapon-1]=(int16_t)(limit-1);pickup_item(g,weapon_items[i],1);
            CHECK((unsigned)inv->ammo[weapon-1]==limit && !inv->pending_count);
            {SsaWeaponObject before=g->weapon.object;
                pickup_item(g,weapon_items[i],1);
                CHECK((unsigned)inv->ammo[weapon-1]==limit && !inv->pending_count &&
                    g->gameplay.weapon.current==current && g->gameplay.weapon.requested==requested &&
                    !memcmp(&before,&g->weapon.object,sizeof before));}
        }
        dispose(g);g=pickup_start(rom,mode);inv=&g->players[0].inventory;
        memset(inv->ammo,0,sizeof inv->ammo);pickup_item(g,0x33,1);
        for(i=2;i<10;++i)CHECK(!(inv->weapons&(1u<<i)) && inv->ammo[i]==g->inventory_rules.ammo_capacity[i]);
        dispose(g);
    }
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
    pickup(g,1);settle(g,0);CHECK(g->players[0].inventory.weapons&2);pickup(g,1);CHECK(!g->alternates.dual_colts && !g->players[0].inventory.pending_count);
    dispose(g);g=start(rom,1);
    CHECK(g->players[0].inventory.weapons==3 && g->gameplay.weapon.current==2 && !g->alternates.dual_colts);
    CHECK(ssa_native_game_select_weapon(g,2)==1 && !g->alternates.requested_variant);tick(g);
    pickup(g,1);CHECK(g->alternates.dual_colts && g->alternates.equip_dual);settle(g,1);
    {SsaWeaponObject before=g->weapon.object;int16_t ammo=g->players[0].inventory.ammo[1];
        pickup(g,1);CHECK(g->alternates.dual_colts && !g->players[0].inventory.pending_count &&
            g->players[0].inventory.ammo[1]==ammo && !memcmp(&before,&g->weapon.object,sizeof before));}
    weapon_pixels(g,pixels);
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
static void reload_key(SsaNativeGame *g){
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'R',1,0)==1);
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'R',0,0)==1);
}
static void manual_reload(const uint8_t *rom){
    unsigned dual;
    for(dual=0;dual<2;++dual){
        SsaNativeGame *g=start(rom,1);unsigned i,ammo,before,sounds;uint64_t began;
        SsaNativeAudioOps audio={0};audio.context=g;audio.play=audio_play;audio.stop=audio_stop;audio.source=audio_source;
        CHECK(ssa_native_game_bind_audio(g,&audio)==1);
        if(dual)pickup(g,1);
        settle(g,dual);sounds=reload_sounds;
        reload_key(g);tick(g);CHECK(!g->alternates.reload_phase && reload_sounds==sounds);
        tap(g);settle(g,dual);
        CHECK(g->alternates.rounds[0]==1 && g->alternates.rounds[1]==dual);
        ammo=(unsigned)g->players[0].inventory.ammo[1];before=g->weapon_game.shots[1];
        /* Disabled/focus-lost actions are consumed, never replayed on resume. */
        g->frame_game.flags|=1;reload_key(g);tick(g);
        g->frame_game.flags&=UINT16_C(0xfffe);tick(g);CHECK(!g->alternates.reload_phase);
        reload_key(g);ssa_pc_input_runtime_focus(&g->input_runtime,0);
        ssa_pc_input_runtime_focus(&g->input_runtime,1);tick(g);CHECK(!g->alternates.reload_phase);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'R',1,0)==1);tick(g);
        CHECK(g->alternates.reload_phase==2 && reload_sounds==sounds+1);
        began=g->alternates.clock;
        for(i=0;i<100 && g->alternates.reload_phase;++i){
            CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'R',1,1)==1);tick(g);
        }
        CHECK(i<100 && g->alternates.clock-began==(dual?1680u:1300u));
        CHECK(!g->alternates.rounds[0] && !g->alternates.rounds[1]);
        CHECK(reload_sounds==sounds+1+dual && g->weapon_game.shots[1]==before &&
            (unsigned)g->players[0].inventory.ammo[1]==ammo);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'R',0,0)==1);
        tap(g);settle(g,dual);CHECK(g->weapon_game.shots[1]==before+1+dual);
        /* Reload waits for recoil, and another R cannot restart its deadline. */
        tap(g);tick(g);reload_key(g);tick(g);
        CHECK(g->alternates.reload_phase==1 || g->alternates.reload_phase==2);
        settle(g,dual);CHECK(!g->alternates.reload_phase);
        dispose(g);
    }
}
static unsigned bobbing_viewport(SsaNativeGame *g,const uint16_t *source,unsigned style){
    static const unsigned sizes[2][2]={{960,540},{480,640}};
    SsaPcRenderer pc={0};uint32_t *background=malloc(960*540*sizeof(*background));
    uint32_t flags=g->weapon.object.flags;unsigned old_style=g->hud_style,old_suppressed=g->hud_suppressed,k,wide=0;
    CHECK(background);g->hud_suppressed=1;g->hud_style=style;
    for(k=0;k<2;++k){unsigned i,samples=0,changed=0,dx,dy;double scale,left,top;
        CHECK(ssa_pc_renderer_resize(&pc,sizes[k][0],sizes[k][1])==1);
        g->weapon_bobbing=0;g->weapon.object.flags=flags&~UINT32_C(1);
        CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1 && ssa_pc_renderer_draw(&pc,g)==1);
        memcpy(background,pc.pixels,(size_t)pc.width*pc.height*sizeof(*background));
        g->weapon.object.flags=flags;g->weapon_bobbing=1;
        CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1 && g->pc_weapon_separate && ssa_pc_renderer_draw(&pc,g)==1);
        scale=pc.width/240.0<pc.height/160.0?pc.width/240.0:pc.height/160.0;
        left=(pc.width-240*scale)/2;top=(pc.height-160*scale)/2;
        dx=(unsigned)g->weapon_bob_x/256;dy=(unsigned)g->weapon_bob_y/256;
        /* Exact original pixels that move outside the3:2 canvas must still
         * appear in the native viewport, without edge filling or scaling. */
        for(i=0;i<38400;++i)if(source[i] && (i%240+dx>=240 || i/240+dy>=160)){
            unsigned x=(unsigned)(left+(i%240+dx+0.5)*scale),y=(unsigned)(top+(i/240+dy+0.5)*scale),c=source[i];
            unsigned rr=c&31,gg=(c>>5)&31,bb=(c>>10)&31;
            uint32_t color=(((rr<<3)|(rr>>2))<<16)|(((gg<<3)|(gg>>2))<<8)|((bb<<3)|(bb>>2));
            if(x>=pc.width || y>=pc.height)continue;
            CHECK(pc.pixels[(size_t)y*pc.width+x]==color);++samples;
            changed+=pc.pixels[(size_t)y*pc.width+x]!=background[(size_t)y*pc.width+x];
        }
        if(k)CHECK(samples && changed);else{wide+=samples;if(samples)CHECK(changed);}
    }
    g->hud_style=old_style;g->hud_suppressed=old_suppressed;
    CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1);
    ssa_pc_renderer_free(&pc);free(background);return wide;
}
static void bobbing(const uint8_t *rom,const char *directory){
    SsaNativeGame *g=start(rom,0);uint16_t still[38400],moving[38400],repeat[38400];
    unsigned weapon,i,changed,overflow=0;int32_t x,y;SsaInventory inventory;uint16_t sx,sy;
    CHECK(!g->weapon_bobbing);
    g->players[0].inventory.weapons=0x3ff;
    for(i=0;i<10;++i)g->players[0].inventory.ammo[i]=999;
    for(weapon=1;weapon<=10;++weapon){
        CHECK(ssa_native_game_select_weapon(g,weapon)==1);
        for(i=0;i<180 && (g->gameplay.weapon.current!=weapon || g->weapon.object.y!=0x5000);++i)tick(g);
        CHECK(i<180);tick(g);
        g->player.camera.bob_amplitude=0x100000;g->player.camera_frame.counter=16;
        g->players[0].entity.body.velocity_x=16*65536;
        g->players[0].entity.body.z=(int32_t)g->players[0].entity.body.floor*65536;
        x=g->weapon.object.x;y=g->weapon.object.y;inventory=g->players[0].inventory;
        sx=g->weapon.object.scale_x;sy=g->weapon.object.scale_y;
        g->weapon_bobbing=0;weapon_pixels(g,still);CHECK(!g->weapon_bob_x && !g->weapon_bob_y);
        g->weapon_bobbing=1;weapon_pixels(g,moving);CHECK(g->weapon_bob_x>0 && g->weapon_bob_y>0);
        CHECK(g->weapon.object.screen_x==(x+g->weapon_bob_x)/256 && g->weapon.object.screen_y==(y+g->weapon_bob_y)/256);
        weapon_pixels(g,repeat);CHECK(!memcmp(moving,repeat,sizeof moving));
        /* Pure integer translation: original pixels, without filling/scaling. */
        {unsigned dx=(unsigned)g->weapon_bob_x/256,dy=(unsigned)g->weapon_bob_y/256;
            for(i=8;i<160;++i)CHECK(!memcmp(moving+i*240+8,still+(i-dy)*240+8-dx,224*sizeof(uint16_t)));}
        CHECK(g->weapon.object.scale_x==sx && g->weapon.object.scale_y==sy);
        /* Every hand/model still has opaque pixels reaching the bottom.
         * The arch must never pull its cropped edge upward. */
        {unsigned bottom=0,right=0,old_right=0;
            for(i=0;i<240;++i)bottom+=moving[159*240+i]!=0;
            for(i=0;i<160;++i){right+=moving[i*240+239]!=0;old_right+=still[i*240+239]!=0;}
            CHECK(bottom>0 && (!old_right || right>0));}
        CHECK(g->weapon.object.x==x && g->weapon.object.y==y && !memcmp(&inventory,&g->players[0].inventory,sizeof inventory));
        for(changed=i=0;i<38400;++i)changed+=still[i]!=moving[i];
        CHECK(changed>20);
        overflow+=bobbing_viewport(g,still,weapon&1?SSA_HUD_ORIGINAL:SSA_HUD_CLASSIC);
        g->player.pose.third_person=1;weapon_pixels(g,repeat);
        CHECK(!g->weapon_bob_x && !g->weapon_bob_y && !memcmp(still,repeat,sizeof still));
        g->player.pose.third_person=0;g->players[0].entity.body.velocity_x=0;
        g->players[0].entity.body.velocity_y=0;weapon_pixels(g,repeat);
        CHECK(!g->weapon_bob_x && !g->weapon_bob_y &&
            !memcmp(still,repeat,sizeof still));g->weapon_bobbing=0;
    }
    CHECK(overflow>10);
    /* Compare the visible arch endpoints against the recovered camera's real
     * left/right step events. The center rises; each step reaches the other side. */
    {SsaBody body=g->players[0].entity.body;SsaPlayerVertical vertical=g->players[0].vertical;
        SsaCamera camera={0};SsaCameraFrame frame={0};unsigned steps=0,last=0;int32_t previous_x=0,highest=512;
        body.velocity_x=8*65536;body.velocity_y=0;vertical.flags=4;frame.is_local=1;
        g->weapon_bobbing=1;g->player.camera.bob_amplitude=0x100000;
        g->players[0].entity.body.velocity_x=16*65536;
        for(i=1;i<=140;++i){int event;frame.counter=(uint16_t)i;
            event=ssa_camera_step(&body,&vertical,&camera,&frame,&g->math);
            g->player.camera=camera;
            g->player.camera_frame.counter=(uint16_t)i;
            CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1);
            CHECK(g->weapon_bob_y>=0 && g->weapon_bob_y<=512);
            CHECK(g->weapon_bob_x>=0 && g->weapon_bob_x<=1536);
            if(g->weapon_bob_y<highest)highest=g->weapon_bob_y;
            if(event){
                CHECK(g->weapon_bob_y>=416 && (event==13?g->weapon_bob_x<=24:g->weapon_bob_x>=1512));
                if(steps)CHECK((i-last==17 || i-last==18) && (int64_t)(previous_x-768)*(g->weapon_bob_x-768)<0);
                last=i;previous_x=g->weapon_bob_x;++steps;
            }
        }
        CHECK(steps==7 && highest<=2);
    }
    dispose(g);
    /* Actual PC walking input: single-axis W must visibly cross native pixel
     * rows, just as W+D does. Neither may move above/left of original art. */
    for(weapon=0;weapon<2;++weapon){unsigned low=512,high=0,visible=0;
        g=start(rom,0);g->weapon_bobbing=1;
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0)==1);
        if(weapon)CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'D',1,0)==1);
        for(i=0;i<70;++i){tick(g);weapon_pixels(g,moving);
            CHECK(g->weapon_bob_x>=0 && g->weapon_bob_y>=0);
            if((unsigned)g->weapon_bob_y<low)low=(unsigned)g->weapon_bob_y;
            if((unsigned)g->weapon_bob_y>high)high=(unsigned)g->weapon_bob_y;
            visible+=g->weapon.object.screen_y>g->weapon.object.y/256;
        }
        CHECK(visible && high-low>=256);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0)==1);
        if(weapon)CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'D',0,0)==1);
        for(i=0;i<30;++i)tick(g);
        weapon_pixels(g,repeat);CHECK(!g->weapon_bob_x && !g->weapon_bob_y);
        dispose(g);
    }
    for(weapon=0;weapon<2;++weapon){
        SsaActorProjection projections[32];SsaPcRenderer pc={0};unsigned slot,count,flash=0;uint32_t before=0,after=0;
        g=start(rom,1);if(weapon)pickup(g,1);settle(g,weapon);
        if(weapon)CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
        tap(g);for(i=0;i<30;++i){tick(g);
            for(slot=0;slot<100;++slot)if(g->spawn.actors[slot].owned && g->spawn.actors[slot].entity.type==0x96 &&
                (g->spawn.actors[slot].entity.body.flags&0x2000) && (!weapon || g->alternates.flash_left[slot]))break;
            if(slot<100)break;
        }
        CHECK(i<30 && slot<100);
        g->player.camera.bob_amplitude=0x100000;g->player.camera_frame.counter=16;
        g->players[0].entity.body.velocity_x=16*65536;
        g->players[0].entity.body.z=(int32_t)g->players[0].entity.body.floor*65536;
        inventory=g->players[0].inventory;x=g->weapon.object.x;y=g->weapon.object.y;
        CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1 && ssa_native_game_draw(g)==1);
        weapon_pixels(g,still);
        count=g->actor_scene.queue.projection_count;memcpy(projections,g->actor_scene.queue.projections,sizeof projections);
        CHECK(ssa_pc_renderer_resize(&pc,480,320)==1 && ssa_pc_renderer_draw(&pc,g)==1);
        for(i=0;i<480*320;++i)before=before*33+pc.pixels[i];
        g->weapon_bobbing=1;
        CHECK(ssa_native_game_build_oam(g,&g->oam,&g->sprites)==1 && ssa_native_game_draw(g)==1);
        CHECK(g->actor_scene.queue.projection_count==count);
        for(i=0;i<count;++i){const SsaActorProjection *v=&g->actor_scene.queue.projections[i];
            if(v->animation==g->spawn.actors[slot].animation){
                CHECK(v->x==projections[i].x+g->actor_scene.screen_x &&
                    v->y==projections[i].y+g->actor_scene.screen_y);++flash;
                CHECK(v->width==projections[i].width && v->height==projections[i].height);
            }else CHECK(v->x==projections[i].x && v->y==projections[i].y);
        }
        CHECK(flash && g->weapon.object.x==x && g->weapon.object.y==y && !memcmp(&inventory,&g->players[0].inventory,sizeof inventory));
        CHECK(ssa_pc_renderer_draw(&pc,g)==1);
        for(i=0;i<480*320;++i)after=after*33+pc.pixels[i];
        CHECK(before!=after);
        if(weapon){unsigned left=0,right=0;weapon_pixels(g,moving);
            for(i=0;i<38400;++i)if(moving[i]){if(i%240<120)++left;else ++right;}
            CHECK(left>30 && right>30);
            {unsigned dx=(unsigned)g->weapon_bob_x/256,dy=(unsigned)g->weapon_bob_y/256;
                for(i=8;i<160;++i){
                    CHECK(!memcmp(moving+i*240+8,still+(i-dy)*240+8-dx,104*sizeof(uint16_t)));
                    CHECK(!memcmp(moving+i*240+128,still+(i-dy)*240+128-dx,104*sizeof(uint16_t)));}}}
        if(weapon)(void)bobbing_viewport(g,still,SSA_HUD_CLASSIC);
        preview(directory,weapon?"dual-bobbing":"single-bobbing",g);
        /* Pause and extra render preparations cannot advance the motion. */
        g->frame_game.flags|=1;tick(g);x=g->weapon_bob_x;y=g->weapon_bob_y;
        CHECK(ssa_native_game_prepare_view(g)==1 && g->weapon_bob_x==x && g->weapon_bob_y==y);
        g->frame_game.flags&=UINT16_C(0xfffe);
        g->player.camera.bob_amplitude=0;
        g->players[0].entity.body.velocity_x=g->players[0].entity.body.velocity_y=0;
        g->players[0].entity.body.z=(int32_t)g->players[0].entity.body.floor*65536+32*65536;
        weapon_pixels(g,repeat);CHECK(!g->weapon_bob_x && g->weapon_bob_y==512);
        {unsigned bottom=0;for(i=0;i<240;++i)bottom+=repeat[159*240+i]!=0;CHECK(bottom>0);}
        ssa_pc_renderer_free(&pc);dispose(g);
    }
    /* A real stationary jump must bob without horizontal movement, then
     * return to the idle baseline after landing without advancing on draws. */
    g=start(rom,1);settle(g,0);g->weapon_bobbing=1;
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0)==1);
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0)==1);
    {unsigned airborne=0,peak=0;
        for(i=0;i<100;++i){tick(g);
            if(g->players[0].entity.body.z>(int32_t)g->players[0].entity.body.floor*65536){
                ++airborne;CHECK(g->weapon_bob_y>=0);if((unsigned)g->weapon_bob_y>peak)peak=(unsigned)g->weapon_bob_y;
                x=g->weapon_bob_x;y=g->weapon_bob_y;weapon_pixels(g,repeat);
                CHECK(g->weapon_bob_x==x && g->weapon_bob_y==y);
            }else if(airborne)break;
        }
        CHECK(i<100 && airborne>10 && peak>=256 && !g->weapon_bob_x && !g->weapon_bob_y);
    }
    dispose(g);
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
        g->vertical_aim=1;g->gba_view=mode>=4;g->player.free_aim=mode<4;g->player.pitch=mode<6?pitches[mode]:0;
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
        SsaNativeGame *g=start(rom,1);unsigned i,starts=0,dwell[18]={0},count,drawable,idle_ticks=0;
        unsigned reload_before=shotgun_reload_sounds;
        uint16_t idle_frame=g->weapon_animations.words[g->weapon_animations.offsets[2]]&0x3fff;
        g->players[0].inventory.weapons|=4;g->players[0].inventory.ammo[2]=(int16_t)(mode==2?1:200);
        CHECK(ssa_native_game_select_weapon(g,3)==1);
        for(i=0;i<100 && (g->gameplay.weapon.current!=3 || g->weapon.object.y!=0x5000);++i)tick(g);
        CHECK(i<100);g->gba_view=mode==1;
        {SsaNativeAudioOps audio={0};audio.play=audio_play;audio.stop=audio_stop;audio.source=audio_source;audio.context=g;
            CHECK(ssa_native_game_bind_audio(g,&audio)==1);}
        if(mode==1){ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);tick(g);}
        drawable=g->weapon_animations.lengths[mode==2?22:12]-1u;
        count=mode==2?6:18;
        CHECK(drawable==(mode==2?6u:17u));
        ssa_pc_input_runtime_button(&g->input_runtime,1,1);
        for(i=0;i<(mode==2?150u:316u);++i){
            tick(g);
            if(g->weapon.shotgun_animating){
                unsigned elapsed=g->weapon.shotgun_elapsed,position=0;
                uint16_t expected;
                while(elapsed>((position+1)*105+count-1)/count)++position;
                CHECK(elapsed>=1 && elapsed<=105 && g->weapon.object.frame_index==(position<drawable?position:drawable-1));
                expected=position==count-1?idle_frame:g->weapon_animations.words[g->weapon_animations.offsets[mode==2?22:12]+position]&0x3fff;
                CHECK((g->weapon.object.frame&0x3fff)==expected);
                CHECK(g->weapon.object.done==(elapsed==105));
                if(elapsed==1){CHECK(i==starts*105);++starts;}
                if(i<105){++dwell[position];if(position==count-1){++idle_ticks;CHECK(g->gameplay.weapon.flags&0x400);}}
            }
            if(i<105)CHECK(g->gameplay.weapon.current==3 && g->weapon.object.y==0x5000);
            if(i==53){unsigned elapsed=g->weapon.shotgun_elapsed,position=g->weapon.object.frame_index,sounds=shotgun_reload_sounds;
                g->frame_game.flags|=1;tick(g);tick(g);
                CHECK(g->weapon.shotgun_elapsed==elapsed && g->weapon.object.frame_index==position && shotgun_reload_sounds==sounds);
                g->frame_game.flags&=UINT16_C(0xfffe);
                ssa_pc_input_runtime_button(&g->input_runtime,1,0);ssa_pc_input_runtime_button(&g->input_runtime,1,1);
            }
        }
        CHECK(starts==(mode==2?1u:4u));
        CHECK(shotgun_reload_sounds==reload_before+(mode==2?0u:3u));
        for(i=0;i<count;++i)CHECK(dwell[i]>=105/count && dwell[i]<=(105+count-1)/count);
        CHECK(idle_ticks==(mode==2?17u:5u));
        if(mode==2)CHECK(g->gameplay.weapon.current!=3); /* Last shell's shorter artwork still occupies the full cycle. */
        else{
            ssa_pc_input_runtime_button(&g->input_runtime,1,0);
            CHECK(ssa_native_game_select_weapon(g,1)==1);
            for(i=0;i<104;++i){tick(g);CHECK(g->gameplay.weapon.current==3 && g->weapon.object.y==0x5000);}
            for(i=0;i<80 && g->gameplay.weapon.current!=1;++i)tick(g);
            CHECK(i<80);
            CHECK(shotgun_reload_sounds==reload_before+4);
        }
        ++shotgun_timing_cases;dispose(g);
    }
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;
    CHECK(argc>=2 && rom && (f=fopen(argv[1],"rb")));CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    pc_pickups(rom);flow(rom,argc==3?argv[2]:NULL);firing(rom,argc==3?argv[2]:NULL);loadouts(rom);magazines(rom,argc==3?argv[2]:NULL);reequip(rom);manual_reload(rom);bobbing(rom,argc==3?argv[2]:NULL);paired_input(rom);shotgun_pattern(rom);shotgun_timing(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"frames\":%u,\"pickups\":%u,\"alternating_shots\":%u,\"left_hand_transitions\":%u,\"loadout_cases\":%u,\"reloads\":%u,\"reload_sounds\":%u,\"reequips\":%u,\"paired_clicks\":%u,\"shotgun_cases\":%u,\"pellets_checked\":%u,\"shotgun_timing_cases\":%u,\"shotgun_interval_ms\":1750,\"colt_shot_ms\":450,\"colt_reload_ms\":1070,\"dual_reload_ms\":1400}\n",checks,frames,pickups,shots,transitions,loadout_cases,reloads,reload_sounds,reequips,paired_clicks,shotgun_cases,pellets_checked,shotgun_timing_cases);return 0;
}
