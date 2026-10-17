#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"cadence line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned checks,cases,click_cases,cancel_cases;
static unsigned native_flashes;
static int (*native_event_sink)(void *,const SsaWeaponRuntime *,SsaWeaponEvent *);
static int native_event(void *context,const SsaWeaponRuntime *r,SsaWeaponEvent *event){
    SsaNativeGame *g=context;
    if(event->kind==SSA_WEAPON_PARTICLE && (event->args[0]==9 || event->args[0]==10)){
        CHECK(!(g->weapon_game.frame&3));++native_flashes;
    }
    return native_event_sink(context,r,event);
}
typedef struct Attacks {unsigned damage,hits,aims,flashes;} Attacks;
static int no_target(void *context,const SsaWeaponController *s,uint16_t heading,int32_t range,SsaWeaponAimResult *out){
    Attacks *a=context;(void)s;(void)heading;(void)range;++a->aims;
    out->target=NULL;out->slope=0;out->fraction=INT32_MAX;out->marker=0x69696969;return 1;
}
static int attack(void *context,const SsaWeaponController *s,SsaWeaponEvent *event){
    Attacks *a=context;(void)s;
    if(event->kind==SSA_WEAPON_HITSCAN){a->damage+=(unsigned)event->args[6];++a->hits;}
    if(event->kind==SSA_WEAPON_PARTICLE && (event->args[0]==9 || event->args[0]==10))++a->flashes;
    return 1;
}
static void damage_budget(const uint8_t *rom,unsigned weapon,unsigned free_aim){
    SsaWeaponRules rules;SsaInventory inv={0};SsaWeaponControl control={0};
    SsaWeaponObject object={0};SsaEntity source={0};SsaWeaponController controller={0};
    SsaWeaponGame game={0};Attacks a={0};SsaWeaponControllerOps ops={no_target,attack,&a};unsigned i;
    CHECK(ssa_weapon_rules_load(&rules,rom,0x800000)==1);
    control.current=control.requested=weapon;control.flags=0x400;
    inv.ammo[weapon-1]=300;object.y=0x5000;controller.inventory=&inv;controller.control=&control;
    controller.object=&object;controller.source=&source;controller.source_health=100;controller.held=1;
    game.ticks=1;game.players=1;
    for(i=0;i<140;++i){game.frame=i/7+1;
        CHECK(ssa_weapon_controller_step_clocked(&controller,&game,&rules,&ops,i%7==6?7:0,free_aim)==1);
    }
    CHECK(a.hits==20 && a.damage==140*(weapon==1?1u:weapon==4?3u:4u) && inv.ammo[weapon-1]==(weapon==1?300:280) && game.shots[weapon-1]==20);
    CHECK(a.aims==(free_aim?140u:420u)); /* Aim still updates between shots. */
    CHECK(a.flashes==(weapon==1?0u:5u)); /* Original frame&3 gate: one flash per four attacks. */
    ++cases;
}
static void ready(SsaNativeGame *g,unsigned weapon){
    g->players[0].inventory.weapons=0x3ff;
    g->players[0].inventory.ammo[weapon-1]=300;
    g->players[0].inventory.health=32767;
    g->gameplay.weapon.current=g->gameplay.weapon.requested=weapon;
    g->gameplay.weapon.flags=0;g->players[0].motion.movement.weapon_flags=0;
    g->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
}
static void cadence(const uint8_t *rom,unsigned weapon,unsigned fps){
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned tick,render_clock=0,first=0,previous=0,shots=0;
    unsigned rate=weapon==1?85:1,refreshes=weapon==1?584:7;
    unsigned total=weapon==1?86:85,delay=weapon==1?7:8;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && g->weapon.automatic_period==7 && g->weapon.automatic_gba_clock==1);
    CHECK(g->weapon.double_minigun==0 && g->weapon.clocked_chainsaw==SSA_CHAINSAW_GBA_CADENCE);
    CHECK(g->weapon.chainsaw_range==192 && g->weapon_rules.range[0]==96);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_native_game_tick(g)==1);ready(g,weapon);
    native_event_sink=g->weapon_ops.event;g->weapon_ops.event=native_event;native_flashes=0;
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    for(tick=1;tick<=600;++tick){unsigned before=g->weapon_game.shots[weapon-1];
        CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.frame==g->frame_game.counter);
        CHECK(g->weapon_game.shots[weapon-1]==(uint64_t)(tick-1)*16777216*rate/((uint64_t)60*280896*refreshes));
        if(g->weapon_game.shots[weapon-1]!=before){
            CHECK(g->weapon_game.shots[weapon-1]==before+1);
            if(!first)first=tick;else CHECK(weapon==1?
                (tick-previous==6 || tick-previous==7):(tick-previous==7 || tick-previous==8));
            previous=tick;++shots;
        }
        render_clock+=fps;
        while(render_clock>=60){uint32_t fired=g->weapon_game.shots[weapon-1];int16_t ammo=g->players[0].inventory.ammo[weapon-1];
            CHECK(ssa_native_game_prepare_view(g)==1);
            CHECK(g->weapon_game.shots[weapon-1]==fired && g->players[0].inventory.ammo[weapon-1]==ammo);
            render_clock-=60;
        }
    }
    CHECK(first==delay+1 && shots==total && g->players[0].inventory.ammo[weapon-1]==(weapon==1?999:300-(int)total));
    CHECK(native_flashes==(weapon==1?0:21u));
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1 && ssa_native_game_tick(g)==1);
    CHECK(!(g->gameplay.weapon.flags&0x400) && !g->weapon.automatic_remaining && !g->weapon.automatic_elapsed);
    for(tick=0;tick<10;++tick)CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==total);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1 && ssa_native_game_tick(g)==1);
    CHECK(g->weapon_game.shots[weapon-1]==total);
    for(tick=0;tick<delay;++tick)CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==total+(tick==delay-1));
    {unsigned remaining=g->weapon.automatic_remaining,elapsed=g->weapon.automatic_elapsed;
        g->frame_game.flags|=1;
        for(tick=0;tick<10;++tick)CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==total+1);
        CHECK(g->weapon.automatic_remaining==remaining && g->weapon.automatic_elapsed==elapsed);
        g->frame_game.flags&=(uint16_t)~1u;
        for(tick=0;tick<3;++tick)CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->weapon_game.shots[weapon-1]==total+1);
        CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
        CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
        for(tick=0;tick<=delay;++tick)CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->weapon_game.shots[weapon-1]==total+2);
    }
    if(weapon!=1){g->players[0].inventory.ammo[weapon-1]=0;
        CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==total+2);
        CHECK(!g->weapon.automatic_remaining && !g->weapon.automatic_elapsed);
    }
    ++cases;ssa_native_game_free(g);free(g);
}
static SsaNativeGame *click_game(const uint8_t *rom,unsigned weapon){
    SsaNativeGame *g=calloc(1,sizeof(*g));
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && g->weapon.buffer_fire==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_native_game_tick(g)==1);ready(g,weapon);return g;
}
static void await_click(SsaNativeGame *g,unsigned weapon,unsigned before){unsigned i;
    for(i=0;i<90 && g->weapon_game.shots[weapon-1]==before;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->weapon_game.shots[weapon-1]==before+1 && !g->weapon.fire_pending_weapon);
}
static void tap(SsaNativeGame *g){
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
}
static void quick_clicks(const uint8_t *rom,unsigned weapon){
    SsaNativeGame *g=click_game(rom,weapon);unsigned i;int16_t spent=weapon==1?0:weapon==3?2:1;
    /* Both OS events arrive before polling: the one-tick pulse must survive
     * the start animation and (for automatics) the original cadence delay. */
    tap(g);CHECK(ssa_native_game_tick(g)==1 && !g->weapon_game.shots[weapon-1]);
    CHECK(g->weapon.fire_pending_weapon==weapon);await_click(g,weapon,0);
    CHECK(g->players[0].inventory.ammo[weapon-1]==(weapon<=2?999:300-spent));
    /* Another short click during the previous attack/reload must reach one
     * more shot; it must neither reset cadence nor run continuously. */
    tap(g);await_click(g,weapon,1);
    CHECK(g->players[0].inventory.ammo[weapon-1]==(weapon<=2?999:300-2*spent));
    for(i=0;i<90;++i)CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==2);
    CHECK(!(g->gameplay.weapon.flags&0x400));
    /* Click/release while the ready weapon is still being raised. */
    g->weapon.object.y=0xdc00;tap(g);await_click(g,weapon,2);
    for(i=0;i<90;++i)CHECK(ssa_native_game_tick(g)==1 && g->weapon_game.shots[weapon-1]==3);
    CHECK(g->players[0].inventory.ammo[weapon-1]==(weapon<=2?999:300-3*spent));
    ++click_cases;ssa_native_game_free(g);free(g);
}
static void cancel_click(const uint8_t *rom,unsigned weapon,unsigned mode){
    SsaNativeGame *g=click_game(rom,weapon);unsigned i;
    tap(g);CHECK(ssa_native_game_tick(g)==1 && g->weapon.fire_pending_weapon==weapon);
    if(mode==0)ssa_pc_input_runtime_focus(&g->input_runtime,0);
    else if(mode==1)ssa_pc_input_runtime_enable(&g->input_runtime,0);
    else if(mode==2)g->frame_game.flags|=1;
    else {g->players[0].inventory.ammo[1]=100;CHECK(ssa_native_game_select_weapon(g,2)==1);}
    for(i=0;i<12;++i)CHECK(ssa_native_game_tick(g)==1 && !g->weapon_game.shots[weapon-1] && !g->weapon.fire_pending_weapon);
    ++cancel_cases;ssa_native_game_free(g);free(g);
}
int main(int argc,char **argv){
    static const unsigned fps[]={0,30,60,144,240};unsigned i,w;uint8_t *rom=malloc(0x800000);FILE *f;
    if(argc!=2 || !rom)return 2;
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(rom,1,0x800000,f)!=0x800000){fclose(f);return 2;}fclose(f);
    for(w=1;w<=5;++w)if(w==1 || w==4 || w==5){
        damage_budget(rom,w,0);damage_budget(rom,w,1);
        for(i=0;i<sizeof(fps)/sizeof(fps[0]);++i)cadence(rom,w,fps[i]);
        for(i=0;i<4;++i)cancel_click(rom,w,i);
    }
    for(w=1;w<=10;++w)if(w!=8 && w!=9)quick_clicks(rom,w);
    printf("{\"result\":\"pass\",\"checks\":%u,\"cases\":%u,\"click_cases\":%u,\"cancel_cases\":%u,\"original_frame_vblanks\":7,"
        "\"gba_cpu_hz\":16777216,\"gba_frame_cycles\":280896,\"minigun_shots_per_second\":%.9f,\"chainsaw_shots_per_second\":%.9f,\"chainsaw_range\":192,\"shots_per_second\":%.9f,\"simulation_hz\":60}\n",
        checks,cases,click_cases,cancel_cases,16777216.0/(280896.0*7),16777216.0*85/(280896.0*584),16777216.0/(280896.0*7));
    free(rom);return 0;
}
