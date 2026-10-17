#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"unlimited range line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static void put(uint8_t *p,int n){p[0]=(uint8_t)n;p[1]=(uint8_t)((unsigned)n>>8);}
static void long_corridor(const SsaMathTables *math)
{
    SsaMapAssets m={0};SsaWorld w={0};SsaWeaponAim trace={0};SsaWeaponHit hit;
    SsaEntity source={0},target={0};unsigned i,words=4+250*4+3;
    m.size=4096;m.data=calloc(1,m.size);CHECK(m.data);
    m.sector_count=m.node_count=m.leaf_count=m.line_count=1;m.vertex_count=2;
    m.sectors=80;m.nodes=100;m.leaves=132;m.lines=140;m.vertices=180;m.blockmap=200;
    put(m.data+0x1a,(int)words);put(m.data+m.sectors+2,256);
    put(m.data+m.nodes+4,1);put(m.data+m.nodes+24,0x8000);put(m.data+m.nodes+26,0x8000);
    put(m.data+m.vertices,15000);put(m.data+m.vertices+2,-192);
    put(m.data+m.vertices+4,15000);put(m.data+m.vertices+6,192);
    put(m.data+m.lines+2,1);put(m.data+m.lines+6,384);
    put(m.data+m.blockmap,-16000);put(m.data+m.blockmap+2,-256);
    put(m.data+m.blockmap+4,250);put(m.data+m.blockmap+6,4);
    for(i=0;i<1000;++i)put(m.data+m.blockmap+8+i*2,1004);
    put(m.data+m.blockmap+2012,0xffff);
    CHECK(ssa_world_init(&w,&m,2)==1);CHECK(ssa_weapon_aim_init(&trace,&w,math)==1);
    source.body.x=-15000*65536;source.body.y=64*65536;source.body.height=128*65536;
    source.body.flags=2;source.radius=16*65536;target=source;target.body.x=14000*65536;
    CHECK(ssa_world_insert(&w,&source)==1 && ssa_world_insert(&w,&target)==1);
    CHECK(ssa_weapon_hit_query_free(&trace,&source,16384,96*65536,2048*65536,0,&hit)==1 && !hit.impact);
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,0,&hit,1)==1 && hit.target==&target);
    CHECK(hit.x/65536>13980 && hit.x/65536<14020); /*29,000-unit shot, beyond64 cells.*/
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,0,&hit,0)==1 && hit.target==&target);
    CHECK(ssa_world_remove(&w,&target)==1);
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,0,&hit,1)==1 && hit.impact && !hit.target);
    CHECK(abs(hit.x/65536-15000)<=1);
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,65536,&hit,1)==1 && hit.impact && !hit.target);
    CHECK(abs(hit.z/65536-256)<=1); /* Ceiling blocks before the distant wall. */
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,-65536,&hit,1)==1 && hit.impact && !hit.target);
    CHECK(abs(hit.z/65536)<=1);
    target.body.x=15500*65536;CHECK(ssa_world_insert(&w,&target)==1);
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,0,&hit,1)==1 && hit.impact && !hit.target);
    CHECK(abs(hit.x/65536-15000)<=1);CHECK(ssa_world_remove(&w,&target)==1);
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,49152,96*65536,0,&hit,1)==1 && !hit.impact);
    CHECK(trace.end_x<w.origin_x); /* Open ray ends at world edge, no self-hit. */
    CHECK(ssa_world_remove(&w,&source)==1);
    source.body.x=INT32_MAX-2;
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,16384,96*65536,0,&hit,1)==1 && !hit.impact && trace.end_x>0);
    source.body.x=INT32_MIN+2;
    CHECK(ssa_weapon_hit_query_unlimited(&trace,&source,49152,96*65536,0,&hit,1)==1 && !hit.impact && trace.end_x<0);
    CHECK(ssa_weapon_aim_free(&trace)==1);
    ssa_world_free(&w);free(m.data);
}
static int moved(void *context,SsaProjectile *p,int32_t x,int32_t y)
{unsigned *count=context;++*count;p->entity->body.x=x;p->entity->body.y=y;return 1;}
static int clear(void *context,SsaProjectile *p){(void)context;(void)p;return 1;}
static int blocker(void *context,SsaProjectile *p,SsaEntity **e){(void)context;(void)p;*e=NULL;return 0;}
static int health(void *context,const SsaEntity *e,int16_t *out){(void)context;(void)e;*out=100;return 0;}
static int damage(void *context,SsaProjectile *p,SsaEntity *e,uint32_t n){(void)context;(void)p;(void)e;(void)n;return 0;}
static int freeze(void *context,SsaProjectile *p,SsaEntity *e){(void)context;(void)p;(void)e;return 0;}
static void projectile_age(const SsaMathTables *math)
{
    SsaEntity e={0},owner={0};SsaProjectile p={0};SsaProjectileMotionGame g={0};unsigned moves=0,i;
    SsaProjectileMotionOps ops={moved,clear,blocker,health,damage,freeze,clear,clear,clear,&moves};
    p.entity=&e;p.owner=1;p.timer=480;p.speed=65536;e.type=0x65;e.body.velocity_x=65536;
    g.players[0]=&owner;g.player_count=1;g.ticks=1;
    CHECK(ssa_projectile_motion(&p,&g,math,&ops)==0 && !moves); /* Original expiry. */
    p.timer=480;
    for(i=0;i<1000;++i)CHECK(ssa_projectile_motion_unlimited(&p,&g,math,&ops)==1);
    CHECK(moves==1000 && e.body.x==1000*65536 && p.timer==480);
    p.owner=0;p.timer=480;
    CHECK(ssa_projectile_motion_unlimited(&p,&g,math,&ops)==0 && moves==1000);
}
static unsigned distant_runtime(uint8_t *rom)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaWeaponHit hit;unsigned level,dir,slot,shots=0,found=0;
    const int32_t distance=2304*65536;uint16_t heading=0;SsaEntity *target=NULL;
    CHECK(g);
    for(level=0;level<16 && !found;++level){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->weapon.unlimited_ranged && g->projectiles.unlimited_player_range);
        for(slot=0;slot<100;++slot)if(g->spawn.actors[slot].owned &&
            g->spawn.actors[slot].entity.type>=0xcd && g->spawn.actors[slot].entity.type<=0xd6)break;
        if(slot<100)for(dir=0;dir<64;++dir){int32_t sine,cosine;SsaEntity *source=&g->players[0].entity;int64_t dx,dy;
            heading=(uint16_t)(dir*1024);ssa_sincos(&g->math,heading,&sine,&cosine);
            CHECK(ssa_weapon_hit_query_unlimited(&g->camera_trace,source,heading,96*65536,0,&hit,1)==1);
            dx=(int64_t)hit.x-source->body.x;dy=(int64_t)hit.y-source->body.y;
            if(!hit.impact || dx*sine/65536+dy*cosine/65536<distance+64*65536)continue;
            target=&g->spawn.actors[slot].entity;CHECK(ssa_world_remove(&g->world,target)==1);
            target->body.x=source->body.x+(int32_t)((int64_t)sine*distance/65536);
            target->body.y=source->body.y+(int32_t)((int64_t)cosine*distance/65536);
            target->body.z=source->body.z;target->body.height=128*65536;target->body.flags|=2;
            CHECK(ssa_world_insert(&g->world,target)==1);
            CHECK(ssa_weapon_hit_query_unlimited(&g->camera_trace,source,heading,96*65536,0,&hit,1)==1 && hit.target==target);
            found=1;break;
        }
        if(!found){ssa_native_game_free(g);memset(g,0,sizeof(*g));}
    }
    CHECK(found);
    for(dir=0;dir<2;++dir){unsigned weapon;
        g->player.free_aim=(uint8_t)dir;g->player.pitch=0;
        g->players[0].motion.movement.heading=heading;g->players[0].motion.facing=(uint8_t)(heading>>8);
        g->player.camera.eye_z=g->players[0].entity.body.z+96*65536;
        for(weapon=1;weapon<=5;++weapon){unsigned tick,before=g->weapon_game.shots[weapon-1];
            g->spawn.actors[slot].activation.health=1000;
            target->body.flags&=UINT16_C(0xfffe); /* Clear prior test shot's hurt flag. */
            g->players[0].inventory.weapons=0x3ff;g->players[0].inventory.ammo[weapon-1]=99;
            g->gameplay.weapon.current=g->gameplay.weapon.requested=(uint16_t)weapon;
            g->gameplay.weapon.flags=0;g->players[0].motion.movement.weapon_flags=0;
            g->players[0].motion.movement.flags=4;g->weapon.object.y=0x5000;
            CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
            for(tick=0;tick<128 && g->weapon_game.shots[weapon-1]==before;++tick){
                CHECK(ssa_weapon_runtime_step(&g->weapon,&g->weapon_game,1,(uint16_t)(tick==0),&g->weapon_ops)==0);
                CHECK(ssa_weapon_runtime_animate(&g->weapon,0,1)==0);
            }
            CHECK(tick<128);
            if(weapon==1)CHECK(g->spawn.actors[slot].activation.health==1000);
            else {if(g->spawn.actors[slot].activation.health>=1000 || !g->weapon.hits[weapon-1]){
                fprintf(stderr,"runtime level%u mode%u weapon%u heading%u health%d hits%u slope%d source%d,%d target%d,%d\n",
                    level-1,dir,weapon,heading,g->spawn.actors[slot].activation.health,g->weapon.hits[weapon-1],
                    g->weapon.controller.aim_slope,g->players[0].entity.body.x/65536,g->players[0].entity.body.y/65536,
                    target->body.x/65536,target->body.y/65536);}
                CHECK(g->spawn.actors[slot].activation.health<1000 && g->weapon.hits[weapon-1]);++shots;}
        }
    }
    ssa_native_game_free(g);free(g);return shots;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);SsaMathTables math;FILE *f;unsigned shots;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);CHECK(ssa_math_tables_load(&math,rom,0x800000));
    long_corridor(&math);projectile_age(&math);shots=distant_runtime(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"maximum_test_distance\":29000,\"distant_runtime_attacks\":%u,\"projectile_ticks_past_expiry\":1000}\n",checks,shots);return 0;
}
