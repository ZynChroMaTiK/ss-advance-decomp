#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "ssa/entity_runtime.h"

typedef struct EntityCase {
    Fixture *f;ProjectileCase *projectile;CdCase *cd;
    SsaEnemyQueries queries;SsaEntityRuntime runtime;SsaEntityRuntimeFrame frame;
    SsaEntityRuntimeOps ops;SsaLevelPlayerOps player_ops;SsaCombatCdOps cd_ops;
    unsigned visited[100],order[100],order_count,fail_slot,fail_animation;
} EntityCase;
static unsigned entity_frames,entity_player_frames,entity_airborne,entity_third;
static unsigned entity_handlers,entity_guards,entity_failures,entity_moves;
static unsigned entity_projectiles,entity_enemies,entity_timers,entity_swap_skips;
static unsigned entity_detaches,entity_unsupported,entity_noops,entity_presentations;
static unsigned entity_cd_moves,entity_walks;

static void entity_busy(EntityCase *t) {
    CHECK(t->runtime.updating && t->runtime.dispatch.updating);
    CHECK(ssa_entity_runtime_step(&t->runtime,&t->frame,&t->ops)==-1);
    CHECK(ssa_entity_runtime_free(&t->runtime)==-1);
    CHECK(ssa_entity_runtime_unbind_cd(&t->runtime,t->cd?t->cd->runtime.slot:0)==-1);
    if(t->cd)CHECK(ssa_entity_runtime_bind_cd(&t->runtime,&t->cd->runtime)==-1);
    entity_guards+=t->cd?4:3;
}
static int entity_before(void *context,const SsaEntityRuntime *r,uint32_t entry,unsigned slot) {
    EntityCase *t=context;
    CHECK(r==&t->runtime && slot<100 && t->order_count<100);
    CHECK(r->dispatch.current==r->entities[slot]);
    CHECK(t->projectile->weapon.damage.source==r->entities[slot]);
    CHECK(entry==ssa_entity_dispatch_handler(r->entities[slot]->type));
    ++t->visited[slot];t->order[t->order_count++]=slot;++entity_handlers;
    if(entry==0x08013978)++entity_enemies;
    else if(entry==0x0801b848)++entity_timers;
    else if(entry!=0x08005ccc && t->projectile->runtime.active[slot])++entity_projectiles;
    entity_busy(t);
    return t->fail_slot==slot?-1:0;
}
static int entity_player_event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
    int32_t a,int32_t b,int32_t c,int32_t d) {
    EntityCase *t=context;entity_busy(t);++entity_presentations;
    return weapon_life_event(&t->projectile->weapon,g,kind,a,b,c,d);
}
static int entity_player_animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned period) {
    EntityCase *t=context;CHECK(r==&t->f->player && period && (id==0x65 || id==0x66 || id==0x68));
    entity_busy(t);++entity_presentations;return t->fail_animation?-1:0;
}
static int entity_player_sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    EntityCase *t=context;CHECK(r==&t->f->player && id);entity_busy(t);++entity_presentations;return 0;
}
static void entity_init(EntityCase *t,Fixture *f,ProjectileCase *p,CdCase *cd,const SsaMathTables *math,int32_t speed) {
    SsaEntity *reserved[]={&f->players[0].entity};SsaPlayerGameplay *players[]={&p->weapon.gameplay};
    memset(t,0,sizeof(*t));t->f=f;t->projectile=p;t->cd=cd;t->fail_slot=100;
    CHECK(ssa_enemy_queries_init(&t->queries,&f->spawn,math,reserved,1)==0);
    CHECK(ssa_entity_runtime_init(&t->runtime,&p->weapon.combat,&p->runtime,&t->queries,players,0x1234)==0);
    t->frame.players[0]=f->input;t->frame.enemies=(SsaEnemyCdFrame){1,speed,0,0};
    t->player_ops=(SsaLevelPlayerOps){entity_player_event,entity_player_animation,entity_player_sound,t};
    t->cd_ops=(SsaCombatCdOps){&p->weapon.combat_ops,cd_event,cd};
    t->ops.players[0]=&t->player_ops;t->ops.enemies=&t->cd_ops;t->ops.projectiles=&p->ops;
    t->ops.before=entity_before;t->ops.context=t;
    if(cd) {
        CHECK(ssa_entity_runtime_bind_cd(&t->runtime,&cd->runtime)==0);
        CHECK(cd->runtime.walk_count==&t->runtime.dispatch.walk_count);
        CHECK(t->runtime.previous_walk_count[cd->runtime.slot]==&cd->walks);
    }
}
static int entity_tick(EntityCase *t,unsigned ticks) {
    Fixture *f=t->f;uint32_t counter=t->runtime.dispatch.frame_counter;
    int32_t x=f->players[0].motion.x,y=f->players[0].motion.y,cx=0,cy=0;int result;
    if(t->cd && t->cd->runtime.active) {cx=t->cd->runtime.navigation.entity->body.x;cy=t->cd->runtime.navigation.entity->body.y;}
    t->frame.players[0].motion.ticks=(uint8_t)ticks;t->frame.enemies.ticks=ticks;
    ++t->frame.enemies.frame_counter;++t->projectile->weapon.damage.frame_counter;
    memset(t->visited,0,sizeof(t->visited));t->order_count=0;
    result=ssa_entity_runtime_step(&t->runtime,&t->frame,&t->ops);++entity_frames;
    CHECK(t->runtime.dispatch.frame_counter==counter+1);
    CHECK(!t->runtime.updating && !t->runtime.dispatch.updating && !f->player.updating && !f->spawn.updating);
    CHECK(!f->level.updating && !t->projectile->weapon.combat.updating && !t->projectile->runtime.updating);
    CHECK(t->runtime.dispatch.current==&f->players[0].entity);
    CHECK(t->projectile->weapon.damage.source==&f->players[0].entity && !t->projectile->weapon.damage.source_owner);
    CHECK(!t->runtime.dispatch.auxiliary[t->runtime.dispatch.auxiliary_count]);
    if(x!=f->players[0].motion.x || y!=f->players[0].motion.y)++entity_moves;
    if(t->cd && t->cd->runtime.active && (cx!=t->cd->runtime.navigation.entity->body.x || cy!=t->cd->runtime.navigation.entity->body.y))++entity_cd_moves;
    entity_walks+=t->runtime.dispatch.walk_count;
    return result;
}
static unsigned entity_allocate(Fixture *f,uint16_t type,uint16_t timer,uint16_t flags) {
    unsigned slot;SsaScriptActor *a;
    CHECK(ssa_entity_pool_allocate(&f->spawn.pool,&slot)==1);CHECK(slot>=f->spawn.reserved);
    a=&f->spawn.actors[slot];CHECK(!a->owned);memset(a,0,sizeof(*a));a->owned=1;
    a->activation.entity=&a->entity;a->activation.timer=timer;a->entity.type=type;a->entity.body.flags=flags;
    a->entity.body.x=f->players[0].entity.body.x;a->entity.body.y=f->players[0].entity.body.y;
    a->entity.body.z=f->players[0].entity.body.z;a->script.type=type;a->script.timer=timer;a->script.flags=flags;
    CHECK(ssa_world_insert(&f->world,&a->entity)==1);return slot;
}
static void entity_cleanup(EntityCase *t) {
    CdCase *cd=t->cd;CHECK(ssa_entity_runtime_free(&t->runtime)==0);
    if(cd)CHECK(cd->runtime.walk_count==&cd->walks);
    CHECK(ssa_enemy_queries_free(&t->queries)==0);
}
static void entity_movement(EntityCase *t,unsigned frame) {
    Fixture *f=t->f;
    if(frame==16 || frame==112) {
        CHECK(ssa_player_runtime_key(&f->player,'H',1,0,1));
        CHECK(!ssa_player_runtime_key(&f->player,'H',0,0,1));
    }
    t->frame.players[0].jump_down=(uint8_t)(frame%48<3);
    t->frame.players[0].yaw_delta=(frame&1)?97:-97;
    t->frame.players[0].keys=frame%64<16?0x40:frame%64<32?0x200:frame%64<48?0x80:0x100;
    CHECK(entity_tick(t,1)==0);
    CHECK(f->players[0].entity.owner==&f->world && f->players[0].motion.z==f->players[0].entity.body.z);
    CHECK(f->players[0].entity_health==f->players[0].inventory.health);
    if(f->players[0].motion.z>(int32_t)f->players[0].entity.body.floor*65536)++entity_airborne;
    if(f->player.view.third_person)++entity_third;
    ++entity_player_frames;
}
static void entity_extras(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    Fixture f;ProjectileCase p;EntityCase t;unsigned first,second,i;
    setup(&f,rom,size,0,math,defs,100);projectile_init(&p,&f,inv,rules,anim,math);
    entity_init(&t,&f,&p,NULL,math,speed);
    first=projectile_fire(&p,6);projectile_fire(&p,6);second=f.spawn.pool.active_slots[2];CHECK(first!=second);
    f.spawn.actors[first].activation.timer=480;f.spawn.actors[second].activation.timer=480;
    CHECK(entity_tick(&t,1)==0);
    CHECK(!p.runtime.active[first] && p.runtime.active[second]);
    CHECK(t.visited[first]==1 && !t.visited[second] && f.spawn.actors[second].activation.timer==480);
    CHECK(f.spawn.pool.active_slots[1]==second && t.runtime.dispatch.auxiliary_count==1);
    CHECK(t.runtime.dispatch.auxiliary[0]==first+1);++entity_swap_skips;
    CHECK(entity_tick(&t,1)==0 && t.visited[second]==1 && !p.runtime.active[second]);
    first=entity_allocate(&f,0xe1,2,0x80);second=entity_allocate(&f,0xd9,77,0);
    CHECK(entity_tick(&t,1)==0 && f.spawn.actors[first].activation.timer==1);
    CHECK(!t.visited[second] && f.spawn.actors[second].activation.timer==77);++entity_noops;
    CHECK(entity_tick(&t,1)==0 && f.spawn.actors[first].activation.timer==30 && !(f.spawn.actors[first].entity.body.flags&0x80));
    CHECK(entity_tick(&t,30)==0 && !f.spawn.actors[first].activation.timer);
    CHECK(f.spawn.actors[first].script.timer==0 && f.spawn.actors[first].entity.owner==&f.world);
    CHECK(ssa_script_spawn_release(&f.spawn,second)==0);
    t.fail_slot=first;f.spawn.actors[first].activation.timer=9;
    CHECK(entity_tick(&t,1)==-1 && f.spawn.actors[first].activation.timer==9);++entity_failures;
    CHECK(t.visited[0]==1 && t.visited[first]==1);t.fail_slot=100;
    t.fail_animation=1;f.players[0].motion.movement.flags=4;
    t.frame.players[0].keys=0x40;f.player.frame.animation_state=0;
    CHECK(entity_tick(&t,1)==-1);++entity_failures;t.fail_animation=0;
    CHECK(ssa_script_spawn_release(&f.spawn,first)==0);
    second=entity_allocate(&f,0xce,0,0);
    CHECK(entity_tick(&t,1)==-1 && t.visited[second]==1);++entity_unsupported;
    CHECK(f.spawn.actors[second].owned && f.spawn.actors[second].entity.owner==&f.world);
    CHECK(ssa_script_spawn_release(&f.spawn,second)==0);
    t.frame.players[0].keys=0;
    CHECK(ssa_entity_runtime_bind_cd(&t.runtime,NULL)==-1);++entity_guards;
    CHECK(ssa_entity_runtime_unbind_cd(&t.runtime,100)==-1);++entity_guards;
    t.frame.enemies.ticks=256;CHECK(ssa_entity_runtime_step(&t.runtime,&t.frame,&t.ops)==-1);t.frame.enemies.ticks=1;++entity_guards;
    t.frame.players[0].motion.ticks=2;CHECK(ssa_entity_runtime_step(&t.runtime,&t.frame,&t.ops)==-1);t.frame.players[0].motion.ticks=1;++entity_guards;
    f.world.query_active=1;CHECK(ssa_entity_runtime_free(&t.runtime)==-1);f.world.query_active=0;++entity_guards;
    entity_cleanup(&t);projectile_cleanup(&p);cleanup(&f);
    /* Actual CD corpse destruction restores its borrowed walk-count pointer;
     * the swapped E1 waits until the following entity phase. */
    {
        CdCase cd;SsaScriptActor *a;
        setup(&f,rom,size,0,math,defs,100);projectile_init(&p,&f,inv,rules,anim,math);
        script(&f,rom,thing,1,1);step(&f,1);bind(&cd,&f,math,speed);activate(&cd);
        entity_init(&t,&f,&p,&cd,math,speed);a=&f.spawn.actors[cd.runtime.slot];first=cd.runtime.slot;
        CHECK(ssa_entity_runtime_bind_cd(&t.runtime,&cd.runtime)==-1);++entity_guards;
        CHECK(ssa_combat_runtime_damage(&p.weapon.combat,&a->entity,&f.players[0].entity,NULL,100,&p.weapon.combat_ops)==0);
        CHECK(entity_tick(&t,1)==0 && a->activation.state==2 && a->group_released);
        second=entity_allocate(&f,0xe1,9,0);a->animation_flags=1;a->activation.timer=0;
        CHECK(entity_tick(&t,1)==0 && !cd.runtime.active && !t.runtime.enemies[first]);
        CHECK(cd.runtime.walk_count==&cd.walks && !t.runtime.previous_walk_count[first]);++entity_detaches;
        CHECK(!t.visited[second] && f.spawn.actors[second].activation.timer==9);++entity_swap_skips;
        CHECK(entity_tick(&t,1)==0 && t.visited[second]==1 && f.spawn.actors[second].activation.timer==8);
        CHECK(ssa_script_spawn_release(&f.spawn,second)==0);
        for(i=0;i<3;++i)CHECK(entity_tick(&t,1)==0);
        entity_cleanup(&t);projectile_cleanup(&p);cleanup(&f);
    }
}
#ifndef SSA_ENTITY_TEST_MAIN
#define SSA_ENTITY_TEST_MAIN main
#endif
int SSA_ENTITY_TEST_MAIN(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations animations;
    int32_t speeds[12];unsigned level,i,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    CHECK(ssa_inventory_rules_load(&inventory,rom,(size_t)size)==0);
    CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);
    CHECK(ssa_weapon_animations_load(&animations,rom,(size_t)size)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;ProjectileCase p;CdCase cd;EntityCase t;SsaScriptActor *a;unsigned pickup;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);projectile_init(&p,&f,&inventory,&rules,&animations,&math);
        script(&f,rom,thing,1,1);step(&f,1);bind(&cd,&f,&math,speeds[0]);activate(&cd);separate(&cd);
        a=&f.spawn.actors[cd.runtime.slot];entity_init(&t,&f,&p,&cd,&math,speeds[0]);
        CHECK(ssa_entity_runtime_unbind_cd(&t.runtime,cd.runtime.slot)==0 && cd.runtime.walk_count==&cd.walks);
        CHECK(ssa_entity_runtime_bind_cd(&t.runtime,&cd.runtime)==0);
        pickup=entity_allocate(&f,0xe1,2,0x80);projectile_fire(&p,6);
        for(i=0;i<128;++i)entity_movement(&t,i);
        CHECK(!f.spawn.actors[pickup].activation.timer && !(f.spawn.actors[pickup].entity.body.flags&0x80));
        CHECK(f.spawn.actors[pickup].entity.owner==&f.world && a->owned);
        CHECK(t.runtime.enemies[cd.runtime.slot]==&cd.runtime && cd.runtime.walk_count==&t.runtime.dispatch.walk_count);
        entity_cleanup(&t);CHECK(cd.runtime.walk_count==&cd.walks);
        projectile_cleanup(&p);cleanup(&f);
    }
    entity_extras(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    CHECK(entity_airborne && entity_third && entity_moves && entity_cd_moves && entity_walks);
    CHECK(entity_projectiles && entity_enemies && entity_timers && entity_swap_skips==2);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"entity_frames\":%u,\"player_frames\":%u,\"player_moves\":%u,\"airborne\":%u,\"third_person\":%u,\"handler_calls\":%u,\"projectile_updates\":%u,\"cd_updates\":%u,\"cd_moves\":%u,\"walk_calls\":%u,\"pickup_timer_updates\":%u,\"swap_skips\":%u,\"binding_detaches\":%u,\"unsupported_errors\":%u,\"rom_noops\":%u,\"callback_failures\":%u,\"reentry_and_guards\":%u,\"presentation_events\":%u}\n",
        checks,entity_frames,entity_player_frames,entity_moves,entity_airborne,entity_third,
        entity_handlers,entity_projectiles,entity_enemies,entity_cd_moves,entity_walks,entity_timers,
        entity_swap_skips,entity_detaches,entity_unsupported,entity_noops,entity_failures,entity_guards,entity_presentations);
    return 0;
}
