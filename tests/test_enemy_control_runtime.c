#define SSA_CD_TEST_MAIN enemy_cd_fixture_main
#include "test_enemy_cd_runtime.c"
#include "ssa/enemy_control_runtime.h"
#include "ssa/actor_animation.h"
#include "../src/projectile_runtime_internal.h"

typedef struct TargetGuard {
    SsaProjectileRuntime *runtime;SsaProjectileCreateRequest request;SsaEntity *target;
} TargetGuard;
static int target_guard(void *ctx,SsaCombatScope *scope){
    TargetGuard *p=ctx;
    return ssa_projectile_runtime_enemy_scoped(p->runtime,scope,&p->request,p->target,1);
}

typedef struct GenericCase {
    SsaEnemyControlRuntime runtime;
    SsaEnemyControlRuntimeOps ops;
    SsaEnemyCdFrame frame;
    unsigned slot,events,reentry,fail;
} GenericCase;
static int generic_event(void *ctx,const SsaEnemyControl *v,const SsaEnemyControlEvent *ev) {
    GenericCase *c=ctx;
    CHECK(c->runtime.updating && c->runtime.combat->updating && c->runtime.combat->spawn->updating);
    CHECK(v==&c->runtime.controllers[c->slot]);
    CHECK(ssa_enemy_control_runtime_step(&c->runtime,c->slot,&c->frame,&c->ops)==-1);
    CHECK(ssa_enemy_control_runtime_free(&c->runtime)==-1);
    CHECK(ev->kind!=SSA_ENEMY_CONTROL_DAMAGE && ev->kind!=SSA_ENEMY_CONTROL_PROJECTILE);
    ++c->events;++c->reentry;
    return c->fail?-1:0;
}
static int generic_combat(void *ctx,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
    SsaCombatDamageEvent ev,uint32_t a,uint32_t b) {
    GenericCase *c=ctx;(void)v;(void)ev;(void)a;(void)b;
    CHECK(c->runtime.combat==r && r->updating);return 0;
}
static void animate_actor(SsaScriptActor *a,const SsaActorAnimations *animations) {
    SsaActorAnimation v={a->animation,a->animation_display_frame,a->animation_frame,
        a->animation_counter,a->animation_ticks,a->animation_flags};
    CHECK(ssa_actor_animation_step(&v,animations,a->entity.body.flags,1)==1);
    a->animation_display_frame=v.frame;a->animation_frame=v.index;
    a->animation_counter=v.elapsed;a->animation_ticks=v.period;a->animation_flags=v.done;
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaActorAnimations animations;int32_t speeds[12];unsigned level,type,thing,i,frames=0,moves=0,shots=0;
    unsigned events=0,reentries=0,cycles=0,guards=0,failures=0,reuses=0,damaged=0,retained_targets=0;uint32_t walks=0;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);fclose(file);
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_actor_animations_load(&animations,rom,(size_t)size)==1);
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    for(level=0;level<16;++level) for(type=0xce;type<=0xd6;++type) {
        Fixture f;GenericCase c={0};SsaCombatRuntime combat={0};SsaCombatDamage damage={0};
        SsaProjectileRuntime projectiles={0};SsaEnemyQueries queries={0};uint32_t score=0;
        SsaEntity *reserved[1];SsaPlayerRuntime *players[1];SsaScriptActor *a;
        SsaCombatRuntimeOps cops={generic_combat,&c};
        setup(&f,rom,(size_t)size,level,&math,&defs,100);
        reserved[0]=&f.players[0].entity;players[0]=&f.player;
        damage.player_count=1;damage.difficulty=1;
        CHECK(ssa_combat_runtime_init(&combat,&f.spawn,reserved,1,players,1,&damage,&score)==0);
        CHECK(ssa_projectile_runtime_init(&projectiles,&combat,&math)==0);
        CHECK(ssa_enemy_queries_init(&queries,&f.spawn,&math,reserved,1)==0);
        CHECK(ssa_enemy_control_runtime_init(&c.runtime,&combat,&projectiles,&queries,&math,&walks,rom,(size_t)size)==0);
        c.ops=(SsaEnemyControlRuntimeOps){&cops,generic_event,&c};c.frame=(SsaEnemyCdFrame){1,speeds[type-0xcd],0,0};
        CHECK(ssa_enemy_control_runtime_init(&c.runtime,&combat,&projectiles,&queries,&math,&walks,rom,(size_t)size)==-1);
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,100,&c.frame,&c.ops)==-1);
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,1,&c.frame,&c.ops)==-1);guards+=3;
        for(thing=0;thing<64;++thing)if(defs.thing[thing].type==type && !defs.thing[thing].variant)break;
        CHECK(thing<64);script(&f,rom,thing,1,1);step(&f,1);c.slot=first_owned(&f);a=&f.spawn.actors[c.slot];
        CHECK(a->entity.type==type);
        c.frame.ticks=256;CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==-1);c.frame.ticks=1;
        f.world.query_active=1;CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==-1);f.world.query_active=0;
        c.runtime.updating=1;CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==-1);c.runtime.updating=0;
        guards+=3;
        for(i=0;i<90;++i) CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
        CHECK(a->activation.state==1);
        {
            static const int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
            int moved=0;unsigned d,k;int32_t x=a->entity.body.x,y=a->entity.body.y;
            for(d=96;d<=512 && !moved;d+=32) for(k=0;k<8 && !moved;++k) {
                int result=ssa_world_try_move(&f.world,&a->entity,x+dirs[k][0]*(int32_t)d*65536,
                    y+dirs[k][1]*(int32_t)d*65536);CHECK(result>=0);moved=result==1;
            }
        }
        for(i=0;i<128;++i) {
            int32_t x=a->entity.body.x,y=a->entity.body.y;
            int health=f.players[0].inventory.health;
            c.runtime.game.player_x=f.players[0].motion.x;c.runtime.game.player_y=f.players[0].motion.y;
            CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
            CHECK(!c.runtime.updating && !combat.updating && !f.spawn.updating && !f.player.updating);
            animate_actor(a,&animations);++frames;
            if(x!=a->entity.body.x || y!=a->entity.body.y)++moves;
            if(f.players[0].inventory.health<health)++damaged;
        }
        for(i=0;i<100;++i)if(projectiles.active[i]) { ++shots;CHECK(ssa_projectile_runtime_release(&projectiles,i)==0); }
        if(type==0xd6){unsigned target_slot=100,shot_slot=100;SsaEntity *target;
            /* A Prion victim can retain its target beyond corpse expiry.
             * Release an unbound actor through the real pool/world API, then
             * let the actual Gunrilla controller fire at that stable slot. */
            script(&f,rom,thing,1,1);step(&f,1);
            for(i=0;i<100;++i)if(i!=c.slot && f.spawn.actors[i].owned){target_slot=i;break;}
            CHECK(target_slot<100);target=&f.spawn.actors[target_slot].entity;
            CHECK(ssa_script_spawn_release(&f.spawn,target_slot)==0);
            CHECK(!target->owner && !target->type);
            {SsaEntity foreign={0};unsigned count=f.spawn.pool.active_count;
                TargetGuard guard={&projectiles,{0x67,&a->entity,0x500000,0,0x500000,0,0},&foreign};
                CHECK(ssa_combat_scope_run(&combat,&cops,target_guard,&guard)==-1);
                guard.request.source=target;guard.target=&a->entity;
                CHECK(ssa_combat_scope_run(&combat,&cops,target_guard,&guard)==-1);
                CHECK(f.spawn.pool.active_count==count && f.world.entity_count==count);
                CHECK(!combat.updating && !f.spawn.updating && !projectiles.updating);guards+=2;
            }
            a->entity.body.flags=(uint16_t)((a->entity.body.flags|0x400)&~1u);
            a->activation.state=1;a->activation.timer=0;a->parameter34=30;a->parameter36=1000;
            c.runtime.navigation[c.slot].target=target;c.runtime.navigation[c.slot].target_timer=100;
            c.runtime.navigation[c.slot].phase=0;
            CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
            CHECK(a->activation.state==3 && c.runtime.navigation[c.slot].target==target);
            for(i=0;i<100;++i)if(projectiles.active[i]){CHECK(shot_slot==100);shot_slot=i;}
            CHECK(shot_slot<100 && projectiles.projectiles[shot_slot].entity->type==0x67);
            CHECK(shot_slot==target_slot); /* Allocate first, then read target. */
            CHECK(projectiles.projectiles[shot_slot].source==&a->entity);
            CHECK(!combat.updating && !f.spawn.updating && !projectiles.updating);
            CHECK(ssa_projectile_runtime_release(&projectiles,shot_slot)==0);
            a->entity.body.flags&=UINT16_C(0xfbff);++retained_targets;
        }
        /* Damage is genuine native037F4; AI performs death/grouprelease next. */
        CHECK(ssa_combat_runtime_damage(&combat,&a->entity,&f.players[0].entity,&f.players[0].entity,10000,&cops)==0);
        for(i=0;i<200 && !a->group_released;++i) {
            CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);animate_actor(a,&animations);
        }
        CHECK(a->group_released && !a->activation.health);
        for(i=0;i<361;++i) {
            int result=ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops);
            CHECK(result==0 || result==1);if(result==1)break;animate_actor(a,&animations);
        }
        CHECK(i<361 && !a->owned && !c.runtime.active[c.slot]);++cycles;
        script(&f,rom,thing,1,1);step(&f,1);CHECK(first_owned(&f)==c.slot);++reuses;
        for(i=0;i<90;++i) CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
        CHECK(a->activation.state==1 && !c.runtime.controllers[c.slot].destroy_requested);
        a->entity.body.flags|=1;a->activation.health=10;c.fail=1;
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==-1);
        CHECK(!c.runtime.updating && !combat.updating && !f.spawn.updating && !f.player.updating);
        CHECK(a->script.variant==a->activation.state && a->activation.state!=1);++failures;c.fail=0;
        events+=c.events;reentries+=c.reentry;
        CHECK(ssa_enemy_control_runtime_free(&c.runtime)==0);
        CHECK(ssa_projectile_runtime_free(&projectiles)==0);CHECK(ssa_enemy_queries_free(&queries)==0);
        CHECK(ssa_combat_runtime_free(&combat)==0);cleanup(&f);
    }
    CHECK(moves && shots);free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"types\":9,\"controller_frames\":%u,\"moves\":%u,\"shots\":%u,\"death_cycles\":%u,\"events\":%u,\"reentry\":%u,\"walks\":%u,\"guards\":%u,\"failures\":%u,\"slot_reuses\":%u,\"contact_health_changes\":%u,\"retained_target_shots\":%u}\n",checks,frames,moves,shots,cycles,events,reentries,walks,guards,failures,reuses,damaged,retained_targets);return 0;
}
