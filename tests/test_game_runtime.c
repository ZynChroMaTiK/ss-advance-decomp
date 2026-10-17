#define SSA_ENTITY_TEST_MAIN entity_fixture_main
#include "test_entity_runtime.c"
#include "ssa/game_runtime.h"

typedef struct GameCase {
    Fixture *f;ProjectileCase *p;EntityCase entity;
    SsaGameRuntime runtime;SsaFrame frame;SsaFrameGame game;
    SsaActorAnimationRuntime actor;SsaActorAnimation player_state;
    SsaPickupRespawnRuntime pickup;SsaPickupRespawnRuntimeOps pickup_ops;
    SsaSpriteDispatch sprites;SsaWeaponObject objects[84];SsaSpriteDispatchOps sprite_ops;
    SsaGameRuntimeInput input;SsaGameRuntimeOps ops;SsaScriptSpawnOps script_ops;SsaSectorMoverOps mover_ops;
    SsaFrameStage order[16],fail_before,fail_external;unsigned order_count,visited[100];
    unsigned render_world,render_sprites,submits,events,pickup_particles,spawn_particles;
    unsigned control_pause,control_render_hide,fail_particle;
    CdCase *external_cd;
} GameCase;
static unsigned game_frames,game_reentry,game_failures,game_guards,game_player_frames;
static unsigned game_airborne,game_third,game_moves,game_spawns,game_deaths,game_releases;
static unsigned game_natural_done,game_projectile_next,game_projectile_updates,game_pause_frames;
static unsigned game_hidden_frames,game_script_unblocks,game_mover_changes,game_start_shots;
static unsigned game_multiplayer_frames,game_multiplayer_shots,game_lifecycle_shared,game_live_local_flags;
static unsigned game_external_cd_shared,game_vm_input_gate;
static unsigned game_lifecycle_callback_mirrors;
static unsigned game_u16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static const SsaFrameStage full_order[]={SSA_FRAME_POLL,SSA_FRAME_DISTRIBUTE,SSA_FRAME_CONTROL,
 SSA_FRAME_ENTITIES,SSA_FRAME_MOVERS,SSA_FRAME_SCRIPTS,SSA_FRAME_PICKUP_RESPAWN,
 SSA_FRAME_ENTITY_ANIMATION,SSA_FRAME_SPRITE_CONTROLLERS,SSA_FRAME_SPRITE_ANIMATION,
 SSA_FRAME_RENDER_WORLD,SSA_FRAME_RENDER_SPRITES};
static void game_busy(GameCase *t) {
    CHECK(t->runtime.updating && t->frame.updating);
    CHECK(ssa_game_runtime_step(&t->runtime,&t->input,&t->ops)==-1);
    CHECK(ssa_game_runtime_free(&t->runtime)==-1);game_reentry+=2;
}
static int game_before(void *ctx,const SsaGameRuntime *r,SsaFrameStage stage) {
    GameCase *t=ctx;CHECK(r==&t->runtime && r->stage==stage && t->order_count<16);
    game_busy(t);t->order[t->order_count++]=stage;
    return t->fail_before==stage?-1:0;
}
/* Explicit harness boundaries: these observe render requests, they do not
 * implement rendering/platform input or claim to produce a playable frame. */
static int game_external(void *ctx,SsaGameRuntime *r,SsaFrameStage stage) {
    GameCase *t=ctx;CHECK(r==&t->runtime && r->external_active);game_busy(t);
    if(stage==SSA_FRAME_POLL)t->frame.processed_input=0xa5000000u+t->game.counter;
    else if(stage==SSA_FRAME_DISTRIBUTE)CHECK(t->frame.input==0xa5000000u+t->game.counter-1);
    else if(stage==SSA_FRAME_CONTROL) {
        t->game.flags=(uint16_t)((t->game.flags&~1u)|t->control_pause);
        if(t->control_render_hide)t->f->players[0].motion.movement.flags|=8;
        else t->f->players[0].motion.movement.flags&=(uint16_t)~8u;
    } else if(stage==SSA_FRAME_RENDER_WORLD) {
        CHECK(!t->f->player.updating && !t->f->world.query_active);++t->render_world;
    } else if(stage==SSA_FRAME_RENDER_SPRITES) {CHECK(t->sprites.state==3);++t->render_sprites;}
    else CHECK(0);
    return t->fail_external==stage?-1:0;
}
static int game_entity_before(void *ctx,const SsaEntityRuntime *r,uint32_t entry,unsigned slot) {
    GameCase *t=ctx;CHECK(r==&t->entity.runtime && slot<100);game_busy(t);
    CHECK(t->runtime.stage==SSA_FRAME_ENTITIES);++t->visited[slot];
    CHECK(t->p->weapon.damage.frame_counter==t->game.counter);
    if(entry==0x08013978) {
        if(t->external_cd && r->enemies[slot]==&t->external_cd->runtime)CHECK(r->enemies[slot]->game==&t->external_cd->game);
        else {CHECK(r->enemies[slot]==&t->runtime.owned_cd[slot]);CHECK(r->enemies[slot]->game==&t->runtime.cd_game);}
    } else if(t->p->runtime.active[slot])++game_projectile_updates;
    return 0;
}
static int game_player_animation(void *ctx,SsaPlayerRuntime *r,unsigned id,unsigned period) {
    GameCase *t=ctx;CHECK(r==&t->f->player);game_busy(t);
    CHECK(ssa_actor_animation_set(&t->player_state,(uint16_t)id,(uint16_t)period)==1);return 0;
}
static int game_player_event(void *ctx,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent k,
 int32_t a,int32_t b,int32_t c,int32_t d) {
    GameCase *t=ctx;game_busy(t);return weapon_life_event(&t->p->weapon,g,k,a,b,c,d);
}
static int game_player_sound(void *ctx,SsaPlayerRuntime *r,unsigned id) {
    GameCase *t=ctx;game_busy(t);CHECK(r==&t->f->player && (id==13 || id==14 || id==17));return 0;
}
static int game_cd_event(void *ctx,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,
 SsaEntity *other,int32_t a,int32_t b,int32_t d) {
    GameCase *t=ctx;SsaScriptActor *actor=NULL;unsigned i;(void)other;(void)d;game_busy(t);
    for(i=0;i<100;++i)if(t->entity.runtime.enemies[i] && &t->entity.runtime.enemies[i]->controller==cd)actor=&t->f->spawn.actors[i];
    CHECK(actor && actor->owned);++t->events;
    if(t->external_cd)CHECK(t->external_cd->game.loop_source==t->runtime.cd_game.loop_source);
    if(kind==SSA_ENEMY_CD_ANIMATION)CHECK(actor->animation==(unsigned)a && actor->animation_ticks==(unsigned)b && !actor->animation_flags);
    else CHECK(kind==SSA_ENEMY_CD_SOUND || kind==SSA_ENEMY_CD_STOP_SOUND);
    return 0;
}
static int game_particle(void *ctx,const SsaScriptSpawn *s,unsigned id,int32_t x,int32_t y,int32_t z) {
    GameCase *t=ctx;game_busy(t);CHECK(t->runtime.stage==SSA_FRAME_SCRIPTS);
    CHECK(particle(t->f,s,id,x,y,z)==0);++t->spawn_particles;return t->fail_particle?-1:0;
}
static int game_spawn_event(void *ctx,const SsaScriptSpawn *s,unsigned slot,SsaEnemyActivationEvent k,
 int32_t a,int32_t b,int32_t c,int32_t d) {
    GameCase *t=ctx;game_busy(t);return event(t->f,s,slot,k,a,b,c,d);
}
static int game_mover_sound(void *ctx,unsigned id) {
    GameCase *t=ctx;game_busy(t);CHECK(id==41 || id==103 || id==104);return 0;
}
static int game_pickup_particle(void *ctx,const SsaPickupRespawnRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    GameCase *t=ctx;(void)x;(void)y;(void)z;game_busy(t);CHECK(r==&t->pickup && id==5);++t->pickup_particles;return 0;
}
static int game_sprite_submit(void *ctx,SsaSpriteDispatch *d,SsaWeaponObject *o) {
    GameCase *t=ctx;CHECK(d==&t->sprites && o && d->updating && d->state==2);
    CHECK(d->submitted_count && d->submitted[d->submitted_count-1]==(uint8_t)o->active_index);
    game_busy(t);++t->submits;return 0;
}
static void game_start(GameCase *t,Fixture *f,ProjectileCase *p,const SsaMathTables *math,
 const SsaInventoryRules *inv,const SsaWeaponRules *rules,const SsaWeaponAnimations *weapons,
 const SsaWeaponLoadouts *loadouts,const SsaActorAnimations *actors,const SsaPickupRespawnData *pickups,
 unsigned setting,unsigned god,int32_t speed) {
    SsaEntity *reserved[]={&f->players[0].entity};SsaActorAnimation *states[]={&t->player_state};
    SsaWeaponRuntime *w[]={&p->weapon.weapon};SsaPlayerGameplay *gp[]={&p->weapon.gameplay};unsigned i;
    memset(t,0,sizeof(*t));memset(p,0,sizeof(*p));t->f=f;t->p=p;
    weapon_init(&p->weapon,f,inv,rules,weapons);
    CHECK(ssa_weapon_runtime_free(&p->weapon.weapon)==0);
    CHECK(ssa_weapon_runtimes_start(w,gp,&p->weapon.combat,inv,rules,weapons,loadouts,setting,(uint8_t)god)==0);
    CHECK(ssa_projectile_runtime_init(&p->runtime,&p->weapon.combat,math)==0);
    CHECK(ssa_weapon_runtime_bind_projectiles(&p->weapon.weapon,&p->runtime)==0);
    p->ops=(SsaProjectileRuntimeOps){&p->weapon.combat_ops,projectile_particle,p};p->weapon.damage.god_mode=1;
    entity_init(&t->entity,f,p,NULL,math,speed);
    t->entity.player_ops=(SsaLevelPlayerOps){game_player_event,game_player_animation,game_player_sound,t};
    t->entity.cd_ops=(SsaCombatCdOps){&p->weapon.combat_ops,game_cd_event,t};
    t->entity.ops.before=game_entity_before;t->entity.ops.context=t;
    CHECK(ssa_actor_animation_set(&t->player_state,f->players[0].animation,f->players[0].animation_ticks)==1);
    CHECK(ssa_actor_animation_runtime_init(&t->actor,&f->spawn,actors,reserved,states,1)==0);
    CHECK(ssa_pickup_respawn_runtime_init(&t->pickup,&p->weapon.combat,&f->plan,pickups)==0);
    t->pickup_ops=(SsaPickupRespawnRuntimeOps){game_pickup_particle,t};
    for(i=0;i<84;++i) {CHECK(ssa_weapon_object_init(&t->objects[i])==1);t->sprites.objects[i]=&t->objects[i];}
    t->sprites.objects[3]=&p->weapon.weapon.object;t->sprites.active[0]=3;t->sprites.active[1]=5;t->sprites.active_count=2;
    p->weapon.weapon.object.active_index=0;t->objects[5].active_index=1;
    t->objects[5].type=1;t->objects[5].flags=0;CHECK(ssa_weapon_animation_set(&t->objects[5],4,3)==1);
    t->sprite_ops=(SsaSpriteDispatchOps){NULL,game_sprite_submit,t};
    t->game=(SsaFrameGame){0x10000,0,1,0};t->frame.game=&t->game;
    CHECK(ssa_game_runtime_init(&t->runtime,&t->frame,&t->entity.runtime,&t->actor,&t->pickup,
        &t->sprites,weapons,w,&p->weapon.game)==0);
    t->input.entities=t->entity.frame;t->input.entities.enemies.level_counter=7;
    t->script_ops=(SsaScriptSpawnOps){game_particle,game_spawn_event,t};t->mover_ops=(SsaSectorMoverOps){game_mover_sound,t};
    t->ops.entities=&t->entity.ops;t->ops.scripts=&t->script_ops;t->ops.movers=&t->mover_ops;
    t->ops.pickups=&t->pickup_ops;t->ops.weapons[0]=&p->weapon.ops;t->ops.sprites=&t->sprite_ops;
    t->ops.external=game_external;t->ops.before=game_before;t->ops.context=t;
}
static int game_tick(GameCase *t) {
    unsigned i,j=0;uint32_t before=t->game.counter,world=t->entity.runtime.dispatch.frame_counter;
    int result;t->order_count=0;memset(t->visited,0,sizeof(t->visited));t->frame.presentation_latch=0;
    result=ssa_game_runtime_step(&t->runtime,&t->input,&t->ops);++game_frames;
    CHECK(!t->runtime.updating && !t->frame.updating && !t->runtime.external_active);
    CHECK(!t->f->spawn.updating && !t->f->player.updating && !t->p->runtime.updating);
    CHECK(t->game.counter==before+(t->fail_before!=SSA_FRAME_POLL && t->fail_external!=SSA_FRAME_POLL));
    if(!result) {
        for(i=0;i<12;++i) {
            if(t->control_pause && i>=3 && i<=7)continue;
            if(t->control_render_hide && i>=10)continue;
            CHECK(j<t->order_count && t->order[j++]==full_order[i]);
        }
        CHECK(j==t->order_count && t->frame.presentation_latch==16);
        CHECK(t->p->weapon.game.frame==t->game.counter && t->p->weapon.game.ticks==t->game.ticks);
        CHECK(t->p->weapon.damage.frame_counter==t->game.counter);
        CHECK(t->entity.runtime.dispatch.frame_counter==world+!t->control_pause);
        if(t->control_pause)++game_pause_frames;
        if(t->control_render_hide)++game_hidden_frames;
    } else {CHECK(!t->frame.presentation_latch);++game_failures;}
    return result;
}
static void game_finish(GameCase *t) {
    CHECK(ssa_game_runtime_free(&t->runtime)==0);
    CHECK(ssa_pickup_respawn_runtime_free(&t->pickup)==0);
    CHECK(ssa_actor_animation_runtime_free(&t->actor)==0);
    entity_cleanup(&t->entity);projectile_cleanup(t->p);cleanup(t->f);
}
static void game_map(uint8_t *rom,size_t size,unsigned level,const SsaMathTables *math,
 const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
 const SsaWeaponAnimations *weapons,const SsaWeaponLoadouts *loadouts,const SsaActorAnimations *actors,
 const SsaPickupRespawnData *pickups,unsigned thing,int32_t speed) {
    Fixture f;ProjectileCase p;GameCase t;unsigned frame,slot=100,projectile_slot=100,projectile_pending=0;
    unsigned saw_death=0,saw_done=0,saw_release=0,shot=0;
    setup(&f,rom,size,level,math,defs,100);game_start(&t,&f,&p,math,inv,rules,weapons,loadouts,actors,pickups,0,1,speed);
    CHECK(p.weapon.gameplay.weapon.current==10);script(&f,rom,thing,1,1);
    for(frame=0;frame<512;++frame) {
        int32_t x=f.players[0].motion.x,y=f.players[0].motion.y;
        if(frame==16 || frame==112) {
            CHECK(ssa_player_runtime_key(&f.player,'H',1,0,1));
            CHECK(!ssa_player_runtime_key(&f.player,'H',0,0,1));
        }
        if(frame<128) {
            t.input.entities.players[0].jump_down=(uint8_t)(frame%48<3);
            t.input.entities.players[0].yaw_delta=(frame&1)?97:-97;
            t.input.entities.players[0].keys=frame%64<16?0x40:frame%64<32?0x200:frame%64<48?0x80:0x100;
        } else {t.input.entities.players[0].keys=0;t.input.entities.players[0].jump_down=0;t.input.entities.players[0].yaw_delta=0;}
        t.input.held[0]=(uint16_t)!shot;t.input.pressed[0]=(uint16_t)(frame==0);
        CHECK(game_tick(&t)==0);
        if(!frame) {
            slot=first_owned(&f);CHECK(slot<100 && f.spawn.actors[slot].entity.type==0xcd);
            CHECK(!t.visited[slot] && !t.entity.runtime.enemies[slot]);
            CHECK(f.spawn.actors[slot].activation.timer==60);++game_spawns;
        } else if(frame==1) {
            CHECK(t.visited[slot]==1 && t.entity.runtime.enemies[slot]==&t.runtime.owned_cd[slot]);
            CHECK(f.spawn.actors[slot].activation.timer==59);
        }
        if(projectile_pending) {CHECK(t.visited[projectile_slot]==1);++game_projectile_next;projectile_pending=0;}
        if(!shot && p.weapon.game.shots[9]) {
            shot=1;++game_start_shots;projectile_slot=projectile_first(&p);CHECK(projectile_slot<100);
            CHECK(!t.visited[projectile_slot] && f.spawn.actors[projectile_slot].activation.timer==0);projectile_pending=1;
        }
        if(f.spawn.actors[slot].owned) {
            SsaScriptActor *a=&f.spawn.actors[slot];
            if(a->activation.state==1 && frame>=100)
                CHECK(ssa_combat_runtime_damage(&p.weapon.combat,&a->entity,&f.players[0].entity,NULL,100,&p.weapon.combat_ops)==0);
            if(a->activation.state==2 && !saw_death) {saw_death=1;++game_deaths;CHECK(a->group_released);}
            if(saw_death && a->animation_flags && !saw_done) {saw_done=1;++game_natural_done;CHECK(a->animation_flags==0xfe);}
        } else if(!saw_release) {CHECK(saw_death && saw_done && !t.entity.runtime.enemies[slot]);saw_release=1;++game_releases;}
        if(frame<128) {
            ++game_player_frames;if(x!=f.players[0].motion.x || y!=f.players[0].motion.y)++game_moves;
            if(f.players[0].motion.z>(int32_t)f.players[0].entity.body.floor*65536)++game_airborne;
            if(f.player.view.third_person)++game_third;
        }
    }
    CHECK(shot && saw_death && saw_done && saw_release && !f.level.script.count);++game_script_unblocks;
    CHECK(t.render_world==512 && t.render_sprites==512 && t.spawn_particles==1);
    game_finish(&t);
}
static void game_external_cd_case(uint8_t *rom,size_t size,const SsaMathTables *math,const SsaSpawnDefinitions *defs,
 const SsaInventoryRules *inv,const SsaWeaponRules *rules,const SsaWeaponAnimations *weapons,
 const SsaWeaponLoadouts *loadouts,const SsaActorAnimations *actors,const SsaPickupRespawnData *pickups,unsigned thing,int32_t speed) {
    Fixture f;ProjectileCase p;GameCase t;CdCase cd;SsaWeaponRuntime *w[]={&p.weapon.weapon};unsigned slot,new_slot;
    setup(&f,rom,size,0,math,defs,100);game_start(&t,&f,&p,math,inv,rules,weapons,loadouts,actors,pickups,0,1,speed);
    CHECK(ssa_game_runtime_free(&t.runtime)==0);script(&f,rom,thing,1,1);step(&f,1);bind(&cd,&f,math,speed);activate(&cd);
    slot=cd.runtime.slot;CHECK(ssa_entity_runtime_bind_cd(&t.entity.runtime,&cd.runtime)==0);
    cd.game.loop_source=&f.spawn.actors[slot].entity;t.external_cd=&cd;
    CHECK(ssa_game_runtime_init(&t.runtime,&t.frame,&t.entity.runtime,&t.actor,&t.pickup,&t.sprites,weapons,w,&p.weapon.game)==0);
    CHECK(t.runtime.cd_game.loop_source==cd.game.loop_source && t.runtime.cd_game.loop_source);
    script(&f,rom,thing,1,1);f.level.script.alive[0]=1;
    CHECK(game_tick(&t)==0);CHECK(cd.runtime.game==&cd.game && !cd.game.loop_source && !t.runtime.cd_game.loop_source);
    new_slot=f.spawn.pool.active_slots[2];CHECK(new_slot!=slot && !t.entity.runtime.enemies[new_slot]);
    CHECK(game_tick(&t)==0);CHECK(t.entity.runtime.enemies[new_slot]==&t.runtime.owned_cd[new_slot]);
    CHECK(t.runtime.owned_cd[new_slot].game==&t.runtime.cd_game && cd.runtime.game==&cd.game);++game_external_cd_shared;
    game_finish(&t);CHECK(cd.runtime.walk_count==&cd.walks);
}
static void game_extras(uint8_t *rom,size_t size,const SsaMathTables *math,const SsaSpawnDefinitions *defs,
 const SsaInventoryRules *inv,const SsaWeaponRules *rules,const SsaWeaponAnimations *weapons,
 const SsaWeaponLoadouts *loadouts,const SsaActorAnimations *actors,const SsaPickupRespawnData *pickups,unsigned thing,int32_t speed) {
    Fixture f;ProjectileCase p;GameCase t;unsigned i,render;uint32_t elapsed;SsaWeaponObject weapon,passive;SsaActorAnimation actor;
    setup(&f,rom,size,0,math,defs,100);game_start(&t,&f,&p,math,inv,rules,weapons,loadouts,actors,pickups,0,1,speed);
    {
        unsigned *locks[]={&t.runtime.updating,&t.frame.updating,&t.entity.runtime.updating,
            &p.weapon.combat.updating,&p.weapon.damage.updating,&f.spawn.updating,&f.world.query_active,
            &f.level.updating,&f.level.script.updating,&f.level.movers.updating,&f.level.switches.updating,
            &t.actor.updating,&t.pickup.updating,&t.sprites.updating,&p.runtime.updating,
            &p.weapon.weapon.updating,&p.weapon.gameplay.updating,&f.player.updating};
        SsaWeaponObject *saved;uint32_t counter=t.game.counter;SsaGameRuntimeOps invalid=t.ops;
        for(i=0;i<sizeof(locks)/sizeof(locks[0]);++i) {
            *locks[i]=1;CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&t.ops)==-1);
            CHECK(ssa_game_runtime_free(&t.runtime)==-1);*locks[i]=0;game_guards+=2;
        }
        saved=t.sprites.objects[4];t.sprites.objects[4]=t.sprites.objects[3];
        CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&t.ops)==-1);t.sprites.objects[4]=saved;++game_guards;
        saved=t.sprites.objects[3];t.sprites.objects[3]=&t.objects[3];
        CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&t.ops)==-1);t.sprites.objects[3]=saved;++game_guards;
        invalid.external=NULL;CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&invalid)==-1);++game_guards;
        CHECK(t.game.counter==counter);
    }
    CHECK(game_tick(&t)==0);t.control_pause=1;weapon=p.weapon.weapon.object;passive=t.objects[5];actor=t.player_state;elapsed=t.game.elapsed;
    CHECK(game_tick(&t)==0);CHECK(!memcmp(&weapon,&p.weapon.weapon.object,sizeof(weapon)));
    CHECK(memcmp(&passive,&t.objects[5],sizeof(passive)) && !memcmp(&actor,&t.player_state,sizeof(actor)) && t.game.elapsed==elapsed);
    t.control_render_hide=1;render=t.render_world;CHECK(game_tick(&t)==0);CHECK(t.render_world==render);
    t.control_pause=t.control_render_hide=0;
    for(i=0;i<12;++i) {
        t.fail_before=full_order[i];CHECK(game_tick(&t)==-1);CHECK(t.order_count==i+1);++game_guards;
    }
    t.fail_before=0;
    for(i=0;i<12;++i)if(i<3 || i>9) {t.fail_external=full_order[i];CHECK(game_tick(&t)==-1);CHECK(t.order_count==i+1);}
    t.fail_external=0;t.objects[5].type=3;
    CHECK(game_tick(&t)==-1 && t.order[t.order_count-1]==SSA_FRAME_SPRITE_CONTROLLERS);t.objects[5].type=1;
    script(&f,rom,thing,1,1);t.fail_particle=1;
    CHECK(game_tick(&t)==-1 && t.order[t.order_count-1]==SSA_FRAME_SCRIPTS && owned(&f)==1);
    CHECK(!t.entity.runtime.enemies[first_owned(&f)]);t.fail_particle=0;CHECK(game_tick(&t)==0);
    CHECK(owned(&f)==2);CHECK(game_tick(&t)==0);
    for(i=1;i<3;++i) {
        unsigned slot=f.spawn.pool.active_slots[i];CHECK(t.entity.runtime.enemies[slot]);
        CHECK(t.entity.runtime.enemies[slot]->game==&t.runtime.cd_game);
    }
    {
        unsigned slot=first_owned(&f);uint16_t id=f.spawn.actors[slot].animation;
        uint16_t timer=f.spawn.actors[slot].activation.timer;
        f.spawn.actors[slot].animation=0xffff;
        CHECK(game_tick(&t)==-1 && t.order[t.order_count-1]==SSA_FRAME_ENTITY_ANIMATION);
        CHECK(f.spawn.actors[slot].activation.timer==timer-1);
        f.spawn.actors[slot].animation=id;
    }
    /* An actual native mover advances before this frame's VM closes it. */
    if(f.level.movers.count) {
        uint8_t *r=f.level.movers.records[0];unsigned sec=game_u16(r);uint8_t *h=f.map.data+f.map.sectors+20*sec+2;unsigned old=game_u16(h);
        CHECK(ssa_sector_movers_command(&f.level.movers,0,SSA_SECTOR_MOVER_OPEN,NULL)==1);
        CHECK(ssa_level_script_reset(&f.level.script));memset(rom+SCRIPT,0,80);put32(rom+SCRIPT,5);put32(rom+SCRIPT+8,0);put32(rom+SCRIPT+20,10);
        f.level.script.pc[0]=0x08000000u+SCRIPT;f.level.script.count=1;
        CHECK(game_tick(&t)==0);if(game_u16(h)!=old)++game_mover_changes;
    }
    game_finish(&t);
}
typedef struct GameMulti {
    Multiplayer m;SsaGameRuntime runtime;SsaFrame frame;SsaFrameGame game;
    SsaEntityRuntime entities;SsaEnemyQueries queries;SsaProjectileRuntime projectiles;
    SsaActorAnimationRuntime actors;SsaActorAnimation states[2];SsaPickupRespawnRuntime pickups;
    SsaSpriteDispatch sprites;SsaWeaponObject objects[84];
    SsaGameRuntimeInput input;SsaGameRuntimeOps ops;SsaEntityRuntimeOps entity_ops;
    SsaLevelPlayerOps player_ops;SsaProjectileRuntimeOps projectile_ops;SsaPickupRespawnRuntimeOps pickup_ops;
    SsaCombatCdOps cd_ops;
    unsigned stages[12],count,last_slot,last_entry,fail_death_sound;
} GameMulti;
static int multi_game_entity(void *ctx,const SsaEntityRuntime *r,uint32_t entry,unsigned slot) {
    GameMulti *t=ctx;CHECK(r==&t->entities);t->last_slot=slot;t->last_entry=entry;return 0;
}
static void multi_game_busy(GameMulti *t) {
    CHECK(t->runtime.updating && t->frame.updating);
    CHECK(ssa_game_runtime_step(&t->runtime,&t->input,&t->ops)==-1);
    CHECK(ssa_game_runtime_free(&t->runtime)==-1);game_reentry+=2;
}
static int multi_game_before(void *ctx,const SsaGameRuntime *r,SsaFrameStage stage) {
    GameMulti *t=ctx;CHECK(r==&t->runtime && t->count<12);t->stages[t->count++]=(unsigned)stage;multi_game_busy(t);return 0;
}
static int multi_game_external(void *ctx,SsaGameRuntime *r,SsaFrameStage stage) {
    GameMulti *t=ctx;CHECK(r==&t->runtime);multi_game_busy(t);
    CHECK(stage==SSA_FRAME_POLL || stage==SSA_FRAME_DISTRIBUTE || stage==SSA_FRAME_CONTROL ||
        stage==SSA_FRAME_RENDER_WORLD || stage==SSA_FRAME_RENDER_SPRITES);return 0;
}
static int multi_game_animation(void *ctx,SsaPlayerRuntime *r,unsigned id,unsigned period) {
    GameMulti *t=ctx;unsigned i=r->player->entity.player_index;CHECK(i<2);multi_game_busy(t);
    CHECK(ssa_actor_animation_set(&t->states[i],(uint16_t)id,(uint16_t)period)==1);return 0;
}
static int multi_game_sound(void *ctx,SsaPlayerRuntime *r,unsigned id) {
    GameMulti *t=ctx;CHECK(r->player->entity.player_index<2 && (id==13 || id==14 || id==17 || id==103));multi_game_busy(t);
    if(id==17 && r->player->entity.player_index==0) {
        CHECK(t->runtime.life_game.message_ticks==600);++game_lifecycle_callback_mirrors;
        if(t->fail_death_sound)return -1;
    }
    return 0;
}
static int multi_game_life(void *ctx,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
 int32_t a,int32_t b,int32_t c,int32_t d) {
    GameMulti *t=ctx;unsigned i=g->runtime->player->entity.player_index;(void)c;(void)d;CHECK(i<2);multi_game_busy(t);
    CHECK(t->runtime.life_game.input_state==g->life_game.input_state && t->runtime.life_game.message_ticks==g->life_game.message_ticks);
    ++game_lifecycle_callback_mirrors;
    if(kind==SSA_LIFECYCLE_WEAPON_ANIMATION)CHECK(ssa_weapon_runtime_respawn(&t->m.weapon[i],(uint16_t)a,(uint16_t)b)==0);
    else CHECK(kind==SSA_LIFECYCLE_NOTICE || kind==SSA_LIFECYCLE_MODE || kind==SSA_LIFECYCLE_EFFECT);
    return 0;
}
static int multi_game_weapon(void *ctx,const SsaWeaponRuntime *r,SsaWeaponEvent *event) {
    GameMulti *t=ctx;t->m.active=r==&t->m.weapon[0]?0:1;multi_game_busy(t);return mp_event(&t->m,r,event);
}
static int multi_game_impact(void *ctx,const SsaWeaponRuntime *r,const SsaWeaponHit *hit) {
    GameMulti *t=ctx;t->m.active=r==&t->m.weapon[0]?0:1;multi_game_busy(t);return mp_impact(&t->m,r,hit);
}
static int multi_game_particle(void *ctx,const SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    GameMulti *t=ctx;(void)x;(void)y;(void)z;CHECK(r==&t->projectiles && (id==4 || id==15));multi_game_busy(t);return 0;
}
static int multi_game_pickup(void *ctx,const SsaPickupRespawnRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    GameMulti *t=ctx;(void)x;(void)y;(void)z;CHECK(r==&t->pickups && id==5);multi_game_busy(t);return 0;
}
static int multi_game_cd(void *ctx,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,SsaEntity *other,int32_t a,int32_t b,int32_t d) {
    GameMulti *t=ctx;(void)other;(void)a;(void)b;(void)d;CHECK(cd && cd->activation);multi_game_busy(t);
    CHECK(kind==SSA_ENEMY_CD_ANIMATION || kind==SSA_ENEMY_CD_SOUND || kind==SSA_ENEMY_CD_STOP_SOUND);return 0;
}
static void multi_game_tick(GameMulti *t) {
    int result;t->count=0;result=ssa_game_runtime_step(&t->runtime,&t->input,&t->ops);
    if(result)fprintf(stderr,"mp frame%u stages%u last%x entity%u entry%x flags%x,%x life%u health%d,%d weapon%u,%u\n",game_multiplayer_frames,t->count,t->count?t->stages[t->count-1]:0,t->last_slot,t->last_entry,t->m.f->players[0].motion.movement.flags,t->m.f->players[1].motion.movement.flags,t->runtime.life_game.input_state,t->m.f->players[0].inventory.health,t->m.f->players[1].inventory.health,t->m.gp[0].weapon.current,t->m.gp[1].weapon.current);
    CHECK(result==0);++game_multiplayer_frames;
    CHECK(t->count>=10 && t->count<=12 && t->frame.presentation_latch==16);
    CHECK(t->m.game.frame==t->game.counter && t->m.damage.frame_counter==t->game.counter);
    CHECK(t->sprites.submitted_count==2 && t->sprites.submitted[0]==0 && t->sprites.submitted[1]==1);
    CHECK(!t->runtime.updating && !t->m.second.updating && !t->m.f->spawn.updating);
}
static void game_two_players(uint8_t *rom,size_t size,const SsaMathTables *math,const SsaSpawnDefinitions *defs,
 const SsaInventoryRules *inv,const SsaWeaponRules *rules,const SsaWeaponAnimations *weapons,
 const SsaWeaponLoadouts *loadouts,const SsaActorAnimations *actors,const SsaPickupRespawnData *pickups,int32_t speed) {
    Fixture f;GameMulti t;SsaEntity *reserved[2];SsaPlayerRuntime *players[2];SsaPlayerGameplay *games[2];
    SsaWeaponRuntime *runtimes[2];SsaActorAnimation *states[2];const SsaEntity *peers[2];unsigned i,j;
    memset(&t,0,sizeof(t));setup(&f,rom,size,0,math,defs,100);t.m.f=&f;
    CHECK(ssa_script_spawn_free(&f.spawn)==0);f.players[1]=f.players[0];memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=0xca;f.players[1].entity.player_index=1;CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_slide_init(&t.m.slide,&f.map));CHECK(ssa_player_runtime_init(&t.m.second,&f.players[1],&f.world,&t.m.slide,math)==0);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;players[0]=&f.player;players[1]=&t.m.second;
    t.m.damage.player_count=2;t.m.damage.difficulty=1;t.m.damage.god_mode=1;t.m.damage.kill_limit=99;
    CHECK(ssa_combat_runtime_init(&t.m.combat,&f.spawn,reserved,2,players,2,&t.m.damage,&t.m.score)==0);
    for(i=0;i<2;++i) {
        games[i]=&t.m.gp[i];runtimes[i]=&t.m.weapon[i];states[i]=&t.states[i];peers[i]=reserved[i];
        CHECK(ssa_player_gameplay_init(games[i],players[i],&f.plan)==0);f.players[i].motion.movement.flags=4;
        CHECK(ssa_actor_animation_set(states[i],f.players[i].animation,f.players[i].animation_ticks)==1);
    }
    CHECK(ssa_weapon_runtimes_start(runtimes,games,&t.m.combat,inv,rules,weapons,loadouts,0,1)==0);
    CHECK(ssa_projectile_runtime_init(&t.projectiles,&t.m.combat,math)==0);
    for(i=0;i<2;++i) {t.m.weapon[i].credited_kills=&t.m.kills[i];CHECK(ssa_weapon_runtime_bind_projectiles(&t.m.weapon[i],&t.projectiles)==0);}
    CHECK(ssa_enemy_queries_init(&t.queries,&f.spawn,math,reserved,2)==0);
    CHECK(ssa_entity_runtime_init(&t.entities,&t.m.combat,&t.projectiles,&t.queries,games,0)==0);
    CHECK(ssa_actor_animation_runtime_init(&t.actors,&f.spawn,actors,reserved,states,2)==0);
    CHECK(ssa_pickup_respawn_runtime_init(&t.pickups,&t.m.combat,&f.plan,pickups)==0);
    for(i=0;i<84;++i) {CHECK(ssa_weapon_object_init(&t.objects[i])==1);t.sprites.objects[i]=&t.objects[i];}
    for(i=0;i<2;++i) {t.sprites.objects[i]=&t.m.weapon[i].object;t.sprites.active[i]=i;t.m.weapon[i].object.active_index=(uint16_t)i;}
    t.sprites.active_count=2;t.game.ticks=1;t.frame.game=&t.game;
    CHECK(ssa_game_runtime_init(&t.runtime,&t.frame,&t.entities,&t.actors,&t.pickups,&t.sprites,weapons,runtimes,&t.m.game)==0);
    t.m.effects=(SsaCombatRuntimeOps){mp_combat,&t.m};t.m.ops=(SsaWeaponRuntimeOps){&t.m.effects,multi_game_weapon,multi_game_impact,&t};
    t.player_ops=(SsaLevelPlayerOps){multi_game_life,multi_game_animation,multi_game_sound,&t};
    t.projectile_ops=(SsaProjectileRuntimeOps){&t.m.effects,multi_game_particle,&t};t.entity_ops.projectiles=&t.projectile_ops;
    t.entity_ops.before=multi_game_entity;t.entity_ops.context=&t;
    t.cd_ops=(SsaCombatCdOps){&t.m.effects,multi_game_cd,&t};t.entity_ops.enemies=&t.cd_ops;
    t.pickup_ops=(SsaPickupRespawnRuntimeOps){multi_game_pickup,&t};
    for(i=0;i<2;++i) {
        t.entity_ops.players[i]=&t.player_ops;t.ops.weapons[i]=&t.m.ops;t.input.entities.players[i]=f.input;
        t.input.entities.players[i].motion.players=peers;t.input.entities.players[i].motion.slot=(uint8_t)i;
    }
    t.input.entities.enemies=(SsaEnemyCdFrame){1,speed,0,0};
    t.ops.entities=&t.entity_ops;t.ops.scripts=&f.ops;t.ops.movers=&f.movers;t.ops.pickups=&t.pickup_ops;
    t.ops.external=multi_game_external;t.ops.before=multi_game_before;t.ops.context=&t;
    for(j=0;j<128;++j) {
        for(i=0;i<2;++i) {
            t.input.entities.players[i].keys=j<64?(i?0x200:0x40):0;
            t.input.entities.players[i].jump_down=(uint8_t)(j%48<3);
            t.input.held[i]=(uint16_t)(j<64);t.input.pressed[i]=(uint16_t)(j==0);
        }
        multi_game_tick(&t);
    }
    CHECK(t.m.game.shots[9]>=2);game_multiplayer_shots=t.m.game.shots[9];
    f.players[0].inventory.health=0;f.players[0].motion.movement.flags=4;t.fail_death_sound=1;
    t.frame.presentation_latch=0;t.count=0;
    CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&t.ops)==-1);++game_multiplayer_frames;++game_failures;
    CHECK(t.count==4 && !t.frame.presentation_latch && !t.runtime.updating && !t.frame.updating);
    CHECK(t.runtime.life_game.message_ticks==600 && t.m.gp[0].life_game.message_ticks==600);t.fail_death_sound=0;
    /* Shared global input state set by local death must gate the later alive
     * player's weapon in this same frame. Root binds one canonical life_game. */
    f.players[0].inventory.health=0;f.players[0].motion.movement.flags=0x18;
    t.m.gp[0].lifecycle.death_ticks=300;t.runtime.life_game.exit_flag=1;t.runtime.life_game.input_state=0;
    t.m.gp[1].weapon.flags=f.players[1].motion.movement.weapon_flags=0;
    t.m.weapon[1].object.y=0x5000;CHECK(ssa_weapon_animation_idle(&t.m.weapon[1].object,&t.m.gp[1].weapon)==1);
    t.input.held[1]=t.input.pressed[1]=1;i=t.m.game.shots[9];multi_game_tick(&t);
    CHECK(t.runtime.life_game.input_state==3 && t.m.gp[0].life_game.input_state==3 && t.m.gp[1].life_game.input_state==3);
    CHECK(t.m.game.shots[9]==i && !(t.m.gp[1].weapon.flags&0x400));++game_lifecycle_shared;
    {
        SsaSpawnMarker marker,*saved=f.plan.player_starts;size_t count=f.plan.player_count;
        marker=(SsaSpawnMarker){(int16_t)(f.players[0].motion.x/65536),(int16_t)(f.players[0].motion.y/65536),f.players[0].entity.body.floor,0};
        f.plan.player_starts=&marker;f.plan.player_count=1;t.runtime.life_game.exit_flag=t.runtime.life_game.input_state=0;
        t.m.gp[0].lifecycle.death_ticks=300;t.input.held[1]=t.input.pressed[1]=0;multi_game_tick(&t);
        CHECK(f.players[0].inventory.health==100 && !(f.players[0].motion.movement.flags&8));
        CHECK(!(t.m.gp[1].hazard_game.local_flags&8));++game_live_local_flags;
        f.plan.player_starts=saved;f.plan.player_count=count;
    }
    /* Native VM opcode9 writes the same input mode consumed by F150 later in
     * the frame, so a still-living player's held fire must be suppressed. */
    CHECK(ssa_level_script_reset(&f.level.script));memset(rom+SCRIPT,0,40);put32(rom+SCRIPT,9);
    f.level.script.pc[0]=0x08000000u+SCRIPT;f.level.script.count=1;t.runtime.life_game.input_state=0;
    t.m.gp[1].weapon.flags=f.players[1].motion.movement.weapon_flags=0;
    t.m.weapon[1].object.y=0x5000;CHECK(ssa_weapon_animation_idle(&t.m.weapon[1].object,&t.m.gp[1].weapon)==1);
    t.input.held[1]=t.input.pressed[1]=1;i=t.m.game.shots[9];multi_game_tick(&t);
    CHECK(t.runtime.life_game.input_state==3 && t.m.gp[1].life_game.input_state==3 && t.m.game.shots[9]==i);
    CHECK(!(t.m.gp[1].weapon.flags&0x400));++game_vm_input_gate;
    CHECK(ssa_game_runtime_free(&t.runtime)==0);CHECK(ssa_pickup_respawn_runtime_free(&t.pickups)==0);
    CHECK(ssa_actor_animation_runtime_free(&t.actors)==0);CHECK(ssa_entity_runtime_free(&t.entities)==0);
    CHECK(ssa_enemy_queries_free(&t.queries)==0);CHECK(ssa_projectile_runtime_free(&t.projectiles)==0);
    for(i=0;i<2;++i) {CHECK(ssa_weapon_runtime_free(&t.m.weapon[i])==0);CHECK(ssa_player_gameplay_free(&t.m.gp[i])==0);}
    CHECK(ssa_combat_runtime_free(&t.m.combat)==0);CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&t.m.second)==0);ssa_slide_free(&t.m.slide);CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);cleanup(&f);
}
#ifndef SSA_GAME_TEST_MAIN
#define SSA_GAME_TEST_MAIN main
#endif
int SSA_GAME_TEST_MAIN(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations weapons;SsaWeaponLoadouts loadouts;
    SsaActorAnimations actors;SsaPickupRespawnData pickups;int32_t speeds[12];unsigned i,level,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);CHECK(ssa_inventory_rules_load(&inventory,rom,(size_t)size)==0);
    CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);CHECK(ssa_weapon_animations_load(&weapons,rom,(size_t)size)==1);
    CHECK(ssa_weapon_loadouts_load(&loadouts,rom,(size_t)size)==1);CHECK(ssa_actor_animations_load(&actors,rom,(size_t)size)==1);
    CHECK(ssa_pickup_respawn_data_load(&pickups,rom,(size_t)size)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level)game_map(rom,(size_t)size,level,&math,&defs,&inventory,&rules,&weapons,&loadouts,&actors,&pickups,thing,speeds[0]);
    game_extras(rom,(size_t)size,&math,&defs,&inventory,&rules,&weapons,&loadouts,&actors,&pickups,thing,speeds[0]);
    game_external_cd_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&weapons,&loadouts,&actors,&pickups,thing,speeds[0]);
    game_two_players(rom,(size_t)size,&math,&defs,&inventory,&rules,&weapons,&loadouts,&actors,&pickups,speeds[0]);
    CHECK(game_airborne && game_third && game_moves && game_start_shots==16 && game_projectile_next==16);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"frames\":%u,\"player_frames\":%u,\"player_moves\":%u,\"airborne\":%u,\"third_person\":%u,\"scripted_spawns\":%u,\"native_deaths\":%u,\"natural_completions\":%u,\"corpse_releases\":%u,\"script_unblocks\":%u,\"startup_projectile_shots\":%u,\"first_projectile_updates_next_frame\":%u,\"projectile_updates\":%u,\"paused_frames\":%u,\"hidden_frames\":%u,\"mover_changes\":%u,\"failure_prefixes\":%u,\"guards\":%u,\"reentry_checks\":%u,\"multiplayer_frames\":%u,\"multiplayer_shots\":%u,\"shared_lifecycle_checks\":%u,\"live_local_flag_checks\":%u,\"external_cd_shared_checks\":%u,\"vm_weapon_gate_checks\":%u,\"lifecycle_callback_mirrors\":%u}\n",
      checks,game_frames,game_player_frames,game_moves,game_airborne,game_third,game_spawns,game_deaths,game_natural_done,game_releases,game_script_unblocks,game_start_shots,game_projectile_next,game_projectile_updates,game_pause_frames,game_hidden_frames,game_mover_changes,game_failures,game_guards,game_reentry,game_multiplayer_frames,game_multiplayer_shots,game_lifecycle_shared,game_live_local_flags,game_external_cd_shared,game_vm_input_gate,game_lifecycle_callback_mirrors);
    return 0;
}
