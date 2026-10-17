#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks,cases,buttons,clicks,level_now,mode_now;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"use map%u mode%u line%d: %s\n",level_now,mode_now,__LINE__,#x);exit(1);}}while(0)
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int s16(const uint8_t *p){unsigned n=u16(p);return n<32768?(int)n:(int)n-65536;}
static void play(void *context,unsigned sound){(void)context;if(sound==41)++clicks;}
static void stop(void *context,unsigned sound){(void)context;(void)sound;}
static int place(SsaNativeGame *g,unsigned line){
    static const int distances[]={48,64,80,96,32};unsigned side,d;
    const uint8_t *l=g->map.data+g->map.lines+line*40;
    const uint8_t *a=g->map.data+g->map.vertices+u16(l)*4,*b=g->map.data+g->map.vertices+u16(l+2)*4;
    int32_t mx=(s16(a)+s16(b))*32768,my=(s16(a+2)+s16(b+2))*32768;
    SsaMapCollision query={0};SsaPlayerSpawn *p=&g->players[0];
    CHECK(ssa_map_collision_init(&query,&g->map)==1);
    for(side=0;side<2;++side)for(d=0;d<sizeof(distances)/sizeof(distances[0]);++d){
        int32_t sine,cosine,x,y;int sector;uint16_t normal=(uint16_t)(u16(l+28)+0x4000+side*0x8000);
        SsaEntity probe=p->entity;size_t i;
        ssa_sincos(&g->math,normal,&sine,&cosine);
        x=mx+sine*distances[d];y=my+cosine*distances[d];
        if(ssa_map_query(&query,x,y,x,y,p->entity.radius)!=1)continue;
        sector=ssa_map_sector(&g->map,x,y);if(sector<0 || g->map.data[g->map.sectors+sector*20+10]==255)continue;
        probe.body.x=x;probe.body.y=y;
        if(ssa_player_interaction_query(&query,&probe,(uint8_t)((uint16_t)(normal+0x8000)>>8),&g->math)<0)continue;
        for(i=0;i<query.special_count && query.special_lines[i]!=line;++i){}
        if(i==query.special_count)continue;
        CHECK(ssa_world_remove(&g->world,&p->entity)==1);
        p->entity.body.x=x;p->entity.body.y=y;p->entity.body.velocity_x=p->entity.body.velocity_y=p->entity.body.velocity_z=0;
        CHECK(ssa_world_insert(&g->world,&p->entity)==1);
        p->entity.body.z=(int32_t)p->entity.body.floor*65536;
        p->motion.x=x;p->motion.y=y;p->motion.z=p->entity.body.z;p->motion.velocity_z=0;
        p->motion.movement.heading=(uint16_t)(normal+0x8000);p->motion.facing=(uint8_t)(p->motion.movement.heading>>8);
        ssa_map_collision_free(&query);return 1;
    }
    ssa_map_collision_free(&query);return 0;
}
static void press(SsaNativeGame *g){
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'E',1,0)==1);
    CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'E',0,0)==1);
}
static void button_case(const uint8_t *rom,unsigned level,unsigned index,unsigned mode,unsigned view){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaSwitchEntry *e;unsigned line;uint32_t action;
    SsaNativeAudioOps audio={play,stop,NULL,NULL,NULL,NULL,NULL,NULL};
    level_now=level;mode_now=mode;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,level)==1 && g->gameplay.immediate_use==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_native_game_tick(g)==1);g->damage.god_mode=1;
    e=&g->level_runtime.switches.entries[index];line=e->line;CHECK(!(e->flags&1) && place(g,line));
    g->player.view.third_person=(uint8_t)view;
    action=mode==1?0x9800:mode==2?0xdc00:0x5000;
    g->weapon.object.y=(int32_t)action;g->gameplay.weapon_action=action;
    if(mode==3){
        g->players[0].inventory.weapons|=4;g->players[0].inventory.ammo[2]=20;
        g->gameplay.weapon.current=g->gameplay.weapon.requested=3;
        g->gameplay.weapon.flags=0x400;g->players[0].motion.movement.weapon_flags=0x400;
        CHECK(ssa_weapon_animation_set(&g->weapon.object,12,g->weapon_rules.period[2])==1);
    }
    if(mode==4)g->gameplay.immediate_use=0; /* Original ready weapon cannot activate yet. */
    clicks=0;CHECK(ssa_native_game_bind_audio(g,&audio)==1);
    if(mode!=5)press(g); /* No key must never activate a button. */
    CHECK(ssa_native_game_tick(g)==1);
    if(mode==4 || mode==5){
        CHECK(!(e->flags&1) && !clicks);
        g->gameplay.immediate_use=1;press(g);CHECK(ssa_native_game_tick(g)==1);
    }
    CHECK((e->flags&1) && clicks==1);
    CHECK(u16(g->map.data+g->map.sides+u16(g->map.data+g->map.lines+line*40+8)*12+6)==e->on_texture);
    if(mode!=2)CHECK(g->gameplay.weapon_action<0xdc00);
    if(mode==3)CHECK(g->gameplay.weapon.current==3 && (g->gameplay.weapon.flags&0x400) && g->weapon_game.shots[2]==1);
    press(g);CHECK(ssa_native_game_tick(g)==1 && clicks==1); /* Original one-shot latch. */
    CHECK(!g->error_line);++cases;ssa_native_game_free(g);free(g);
}
int main(int argc,char **argv){
    FILE *f;uint8_t *rom=malloc(0x800000);unsigned level,index,mode,view;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    for(level=0;level<16;++level){
        SsaNativeGame *g=calloc(1,sizeof(*g));unsigned count;
        level_now=level;CHECK(g && ssa_native_game_init(g,rom,0x800000,level)==1);count=(unsigned)g->level_runtime.switches.count;
        for(index=0;index<count;++index){
            if(g->level_runtime.switches.entries[index].flags&1)continue;
            ++buttons;
            for(view=0;view<2;++view)for(mode=0;mode<6;++mode)button_case(rom,level,index,mode,view);
        }
        ssa_native_game_free(g);free(g);
    }
    CHECK(buttons && cases==buttons*12);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"buttons\":%u,\"cases\":%u,\"views\":2}\n",checks,buttons,cases);return 0;
}
