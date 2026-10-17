#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "ssa/actor_animation_runtime.h"
#include "ssa/enemy_queries.h"

static SsaActorAnimation *animated_player;
static unsigned actor_ticks,actor_completions,actor_releases,actor_reuses,actor_guards;
static unsigned player_setters,corpse_completion_tick;
static int animate_player(void *ctx,SsaPlayerRuntime *r,unsigned id,unsigned period) {
    CHECK(animation(ctx,r,id,period)==0);
    CHECK(ssa_actor_animation_set(animated_player,(uint16_t)id,(uint16_t)period)==1);
    ++player_setters;return 0;
}
static void actor_guard_test(SsaActorAnimationRuntime *r,Fixture *f,SsaScriptActor *a) {
    SsaActorAnimation before=*r->states[0];uint16_t id=a->animation;unsigned *locks[7];unsigned i;
    locks[0]=&r->updating;locks[1]=&f->spawn.updating;locks[2]=&f->world.query_active;
    locks[3]=&f->level.updating;locks[4]=&f->level.script.updating;
    locks[5]=&f->level.movers.updating;locks[6]=&f->level.switches.updating;
    for(i=0;i<7;++i) {
        *locks[i]=1;CHECK(ssa_actor_animation_runtime_step(r,1)==-1);
        CHECK(ssa_actor_animation_runtime_free(r)==-1);*locks[i]=0;actor_guards+=2;
    }
    CHECK(ssa_actor_animation_runtime_step(r,256)==-1);++actor_guards;
    a->animation=0xffff;CHECK(ssa_actor_animation_runtime_step(r,1)==-1);++actor_guards;
    CHECK(!memcmp(&before,r->states[0],sizeof(before)));
    a->entity.body.flags|=0x800;CHECK(ssa_actor_animation_runtime_step(r,1)==0);
    CHECK(a->animation==0xffff);a->entity.body.flags&=(uint16_t)~0x800;a->animation=id;
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations weapons;SsaActorAnimations animations;
    int32_t speeds[12];unsigned i,level,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    CHECK(ssa_inventory_rules_load(&inventory,rom,(size_t)size)==0);
    CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);
    CHECK(ssa_weapon_animations_load(&weapons,rom,(size_t)size)==1);
    CHECK(ssa_actor_animations_load(&animations,rom,(size_t)size)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;ProjectileCase p;SsaEnemyQueries queries={0};SsaActorAnimationRuntime runtime={0};
        SsaActorAnimation state={0};SsaActorAnimation *states[]={&state};SsaEntity *reserved[1];
        unsigned cycle,last_slot=100;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);projectile_init(&p,&f,&inventory,&rules,&weapons,&math);
        reserved[0]=&f.players[0].entity;
        CHECK(ssa_actor_animation_set(&state,f.players[0].animation,f.players[0].animation_ticks)==1);
        CHECK(ssa_actor_animation_runtime_init(&runtime,&f.spawn,&animations,reserved,states,0)==-1);++actor_guards;
        CHECK(ssa_actor_animation_runtime_init(&runtime,&f.spawn,&animations,reserved,states,1)==0);
        CHECK(ssa_actor_animation_runtime_init(&runtime,&f.spawn,&animations,reserved,states,1)==-1);++actor_guards;
        CHECK(ssa_enemy_queries_init(&queries,&f.spawn,&math,reserved,1)==0);
        animated_player=&state;f.player_ops.animation=animate_player;
        for(cycle=0;cycle<2;++cycle) {
            CdCase cd;SsaScriptActor *a;SsaCombatCdOps ops={&p.weapon.combat_ops,cd_event,&cd};
            unsigned frame,done_seen=0,slot;
            script(&f,rom,thing,1,1);step(&f,1);bind(&cd,&f,&math,speeds[0]);activate(&cd);
            slot=cd.runtime.slot;a=&f.spawn.actors[slot];
            if(cycle){CHECK(slot==last_slot);++actor_reuses;}last_slot=slot;
            CHECK(!a->animation_display_frame && !a->animation_flags);
            actor_guard_test(&runtime,&f,a);
            CHECK(ssa_combat_runtime_damage(&p.weapon.combat,&a->entity,&f.players[0].entity,NULL,100,&p.weapon.combat_ops)==0);
            CHECK(a->activation.health<=0 && (a->entity.body.flags&1));
            cd.frame.ticks=1;
            CHECK(ssa_combat_cd_step(&p.weapon.combat,&cd.runtime,&cd.frame,&queries.ops,&ops)==0);
            CHECK(a->activation.state==2 && a->group_released && a->animation==0x1c && a->animation_ticks==6);
            CHECK(!a->animation_flags && a->activation.timer==360);
            /* Actual 00908 after the death controller; first advance is part of
             * that same frame. No fixture writes a completion byte. */
            CHECK(ssa_actor_animation_runtime_step(&runtime,1)==0);++actor_ticks;
            for(frame=1;frame<=361;++frame) {
                unsigned prior_done=a->animation_flags;
                if(!cycle && frame<=128)player_step(&f,frame-1);
                cd.frame.frame_counter++;p.weapon.damage.frame_counter++;
                CHECK(ssa_combat_cd_step(&p.weapon.combat,&cd.runtime,&cd.frame,&queries.ops,&ops)==(frame==361?1:0));
                if(frame<361) {
                    CHECK(a->activation.timer==360-frame && a->owned);
                    if(prior_done)CHECK(!(a->entity.body.flags&1));
                } else {
                    CHECK(!a->owned && !cd.runtime.active && !f.spawn.pool.occupied[slot]);++actor_releases;
                }
                CHECK(ssa_actor_animation_runtime_step(&runtime,1)==0);++actor_ticks;
                CHECK(!runtime.updating && !f.spawn.updating);
                if(frame<361 && a->animation_flags && !done_seen) {
                    CHECK(a->animation_flags==0xfe && a->animation_ticks==255);
                    done_seen=1;++actor_completions;
                    if(!corpse_completion_tick)corpse_completion_tick=frame+1;
                    CHECK(corpse_completion_tick==frame+1);
                }
            }
            CHECK(done_seen && f.world.entity_count==1);
            CHECK(!a->animation_display_frame && !a->animation_counter && !a->animation_flags);
        }
        CHECK(ssa_actor_animation_runtime_free(&runtime)==0);
        CHECK(ssa_actor_animation_runtime_step(&runtime,1)==-1);++actor_guards;
        CHECK(ssa_enemy_queries_free(&queries)==0);projectile_cleanup(&p);cleanup(&f);
    }
    CHECK(actor_completions==32 && actor_releases==32 && player_setters && airborne && third_frames);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"actor_frames\":%u,\"natural_corpse_completions\":%u,\"corpse_completion_tick\":%u,\"corpse_releases\":%u,\"reused_slots\":%u,\"player_frames\":%u,\"player_animation_setters\":%u,\"airborne\":%u,\"third_person\":%u,\"guards\":%u}\n",
        checks,actor_ticks,actor_completions,corpse_completion_tick,actor_releases,actor_reuses,player_frames,player_setters,airborne,third_frames,actor_guards);
    return 0;
}
