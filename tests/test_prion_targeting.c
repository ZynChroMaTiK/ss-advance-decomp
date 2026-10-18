#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks,cases,frames,selections,melee_hits,shots,unconverted,idle_cases,boss_targets,current_type,current_case;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Prion type%x case%u line%d: %s\n",current_type,current_case,__LINE__,#x);exit(1);}}while(0)
static void place(SsaNativeGame *g,unsigned slot,int32_t x,int32_t y)
{
    SsaEntity *e=&g->spawn.actors[slot].entity;
    CHECK(ssa_world_remove(&g->world,e)==1);e->body.x=x;e->body.y=y;
    CHECK(ssa_world_insert(&g->world,e)==1);e->body.z=(int32_t)e->body.floor*65536;
}
static SsaEnemyNavigation *nav(SsaNativeGame *g,unsigned slot)
{return g->spawn.actors[slot].entity.type==0xcd?&g->entities.enemies[slot]->navigation:&g->enemies.navigation[slot];}
static void tick(SsaNativeGame *g)
{CHECK(ssa_native_game_tick(g)==1);CHECK(!g->error_line);++frames;}
static SsaNativeGame *fixture(const uint8_t *rom,unsigned type,unsigned slots[3],unsigned boss)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i,n=0;int32_t x,y;SsaScriptActor *a;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && g->entities.nearest_converted==1);
    for(i=0;i<100 && n<3;++i)if(g->spawn.actors[i].owned && g->spawn.actors[i].entity.type>=0xcd && g->spawn.actors[i].entity.type<=0xd6)slots[n++]=i;
    CHECK(n==3);
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && i!=slots[0] && i!=slots[1] && i!=slots[2])CHECK(ssa_script_spawn_release(&g->spawn,i)==0);
    g->spawn.actors[slots[0]].entity.type=(uint16_t)type;
    g->spawn.actors[slots[1]].entity.type=g->spawn.actors[slots[2]].entity.type=0xcd;
    if(boss)g->spawn.actors[slots[2]].entity.type=0xd7;
    g->damage.god_mode=1;g->damage.difficulty=1;g->entities.fast_converted=0;
    for(i=0;i<128 && (!g->spawn.actors[slots[0]].activation.state || !g->spawn.actors[slots[1]].activation.state || !g->spawn.actors[slots[2]].activation.state);++i)tick(g);
    CHECK(i<128);x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
    for(i=0;i<3;++i){place(g,slots[i],x+(int32_t)(256+512*i)*65536,y);
        g->spawn.actors[slots[i]].activation.health=5000;g->spawn.actors[slots[i]].entity.body.flags|=0x800;}
    a=&g->spawn.actors[slots[0]];a->entity.body.flags&=UINT16_C(0xf7ff);
    g->players[0].inventory.weapons|=64;g->players[0].inventory.ammo[6]=2;
    g->gameplay.weapon.current=g->gameplay.weapon.requested=7;g->gameplay.weapon.flags=0;
    g->players[0].motion.movement.weapon_flags=0;g->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
    g->players[0].motion.movement.heading=16384;g->players[0].motion.facing=64;g->player.free_aim=1;
    g->player.pitch=(int16_t)ssa_direction(&g->math,(int16_t)((a->entity.body.z+a->entity.body.height/2-g->player.camera.eye_z)/65536),256);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    for(i=0;i<120 && !(a->entity.body.flags&0x400);++i)tick(g);
    CHECK(i<120 && a->activation.health>0 && a->parameter34==30 && g->weapon_game.shots[6]==1);
    for(i=0;i<100;++i)if(g->projectiles.active[i])CHECK(ssa_projectile_runtime_release(&g->projectiles,i)==0);
    return g;
}
static void reset(SsaNativeGame *g,unsigned slots[3])
{
    unsigned i;int32_t x=g->players[0].entity.body.x,y=g->players[0].entity.body.y;
    for(i=0;i<3;++i){SsaScriptActor *a=&g->spawn.actors[slots[i]];
        place(g,slots[i],x+(int32_t)(256+192*i)*65536,y);
        a->activation.health=5000;a->activation.state=1;a->activation.timer=1000;a->parameter36=1000;
        a->entity.body.flags=(uint16_t)((a->entity.body.flags|0x800)&~(1u|0x20u|0x400u));
        a->animation_flags=0;
    }
    g->spawn.actors[slots[0]].entity.body.flags=(uint16_t)((g->spawn.actors[slots[0]].entity.body.flags|0x400)&~0x800u);
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned type;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(type=0xcd;type<=0xd6;++type){
        unsigned slots[3],i;SsaNativeGame *g;SsaEnemyNavigation *n;SsaEntity *expected;
        int32_t x,y;current_type=type;g=fixture(rom,type,slots,0);n=nav(g,slots[0]);
        x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
        for(current_case=0;current_case<7;++current_case){
            reset(g,slots);expected=&g->spawn.actors[slots[1]].entity;
            /* A stale player preference and long original timer must not win. */
            n->target=&g->players[0].entity;n->target_timer=1000;n->phase=0;
            if(current_case==1 || current_case==3 || current_case==4){
                place(g,slots[2],x+416*65536,y);expected=&g->spawn.actors[slots[2]].entity;
            }
            if(current_case==2){place(g,slots[1],x+768*65536,y);expected=&g->players[0].entity;}
            if(current_case==3)g->spawn.actors[slots[1]].activation.health=0;
            if(current_case==4){g->spawn.actors[slots[1]].entity.body.flags|=0x20;g->spawn.actors[slots[2]].entity.body.flags|=0x400;}
            if(current_case==5){g->spawn.actors[slots[1]].activation.health=0;g->spawn.actors[slots[2]].entity.body.flags|=0x20;expected=&g->players[0].entity;}
            if(current_case==6){g->spawn.actors[slots[1]].entity.body.z+=1024*65536;expected=&g->players[0].entity;}
            tick(g);CHECK(n->target==expected && n->target_timer<=600);
            CHECK(g->enemies.game.player==&g->players[0].entity);
            CHECK(g->game.cd_game.player==&g->players[0].entity);++selections;
            /* Change the winner with the old retarget timer still live. */
            if(current_case==0){place(g,slots[2],x+416*65536,y);tick(g);
                CHECK(n->target==&g->spawn.actors[slots[2]].entity);++selections;}
        }
        reset(g,slots);n->target=&g->spawn.actors[slots[1]].entity;n->target_timer=1000;n->phase=2;
        n->waypoint_x=(uint16_t)((x+448*65536)/65536);n->waypoint_y=(uint16_t)(y/65536);
        g->spawn.actors[slots[0]].activation.facing=n->desired_facing=192;
        if(type==0xce || type==0xd0){
            SsaScriptActor *a=&g->spawn.actors[slots[0]],*target=&g->spawn.actors[slots[1]];int16_t old=target->activation.health;
            place(g,slots[1],a->entity.body.x+a->entity.radius+target->entity.radius-65536,y);
            n->waypoint_x=(uint16_t)(target->entity.body.x/65536);g->enemies.crowd_slide=0;
            tick(g);
            if(target->activation.health!=old-(type==0xd0?40:20))fprintf(stderr,"melee health%d->%d phase%u moved%u blocker%p target%p selected%p sourceX%d targetX%d radius%d/%d policy%u\n",old,target->activation.health,n->phase,n->move_result,(void *)g->world.blocking_entity,(void *)&target->entity,(void *)n->target,g->spawn.actors[slots[0]].entity.body.x/65536,target->entity.body.x/65536,g->spawn.actors[slots[0]].entity.radius/65536,target->entity.radius/65536,g->enemies.converted_combat);
            CHECK(target->activation.health==old-(type==0xd0?40:20));++melee_hits;
        }
        if(type==0xcf || type==0xd1 || type==0xd2 || type==0xd3 || type==0xd4 || type==0xd6){
            reset(g,slots);n->target=&g->spawn.actors[slots[1]].entity;n->phase=2;n->navigation_flags=1;
            n->waypoint_x=(uint16_t)((x+448*65536)/65536);n->waypoint_y=(uint16_t)(y/65536);
            g->spawn.actors[slots[0]].activation.facing=n->desired_facing=192;
            g->spawn.actors[slots[0]].activation.timer=0;
            if(type==0xd4){g->spawn.actors[slots[0]].activation.state=2;g->spawn.actors[slots[0]].animation_flags=0xfa;}
            tick(g);
            for(i=0;i<100;++i)if(g->projectiles.active[i] && g->projectiles.projectiles[i].source==&g->spawn.actors[slots[0]].entity)break;
            CHECK(i<100 && g->projectiles.projectiles[i].entity->body.velocity_x>0);++shots;
        }
        reset(g,slots);g->spawn.actors[slots[0]].entity.body.flags&=UINT16_C(0xfbff);
        n->phase=0;n->target=&g->spawn.actors[slots[1]].entity;
        {SsaEnemyMotion facing={&g->spawn.actors[slots[0]].entity,0,0,0,0};
         CHECK(ssa_enemy_face(&facing,&g->players[0].entity,&g->math)==0);tick(g);
         CHECK(n->desired_facing==facing.desired_facing);++unconverted;}
        reset(g,slots);g->spawn.actors[slots[1]].activation.health=g->spawn.actors[slots[2]].activation.health=0;
        g->players[0].inventory.health=0;n->target=&g->spawn.actors[slots[1]].entity;
        tick(g);CHECK(!n->target);++idle_cases;
        ++cases;ssa_native_game_free(g);free(g);
    }
    {unsigned slots[3];SsaNativeGame *g=fixture(rom,0xce,slots,1);SsaEntity *a=&g->spawn.actors[slots[0]].entity;
     current_type=0xce;current_case=8;
     place(g,slots[2],a->body.x+128*65536,a->body.y);
     g->spawn.actors[slots[2]].entity.body.z=a->body.z+a->body.height/2-g->spawn.actors[slots[2]].entity.body.height/2;
     tick(g);CHECK(nav(g,slots[0])->target==&g->spawn.actors[slots[2]].entity);++boss_targets;
     ssa_native_game_free(g);free(g);}
    free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"converted_types\":%u,\"frames\":%u,\"target_selections\":%u,\"melee_hits\":%u,\"ranged_shots\":%u,\"unconverted_cases\":%u,\"no_target_cases\":%u,\"boss_targets\":%u}\n",checks,cases,frames,selections,melee_hits,shots,unconverted,idle_cases,boss_targets);return 0;
}
