#define SSA_WEAPON_TEST_MAIN weapon_fixture_main
#include "test_weapon_runtime.c"
#include "ssa/projectile_runtime.h"

typedef struct ProjectileCase {
    WeaponCase weapon;
    SsaProjectileRuntime runtime;
    SsaProjectileRuntimeOps ops;
    unsigned fail_particle;
} ProjectileCase;
static unsigned projectile_frames, projectile_shots[3], projectile_hits[3];
static unsigned projectile_moves, projectile_particles, projectile_busy_checks;
static unsigned projectile_guards, projectile_failures, projectile_expirations;
static unsigned projectile_pool_full, projectile_pool_reuses, projectile_freezes;
static unsigned projectile_bounces, projectile_effect_releases;
static unsigned projectile_penetrations;
static unsigned projectile_mp_kills,projectile_mp_packets;

static unsigned projectile_count(const ProjectileCase *t) {
    unsigned i,n=0;for(i=0;i<100;++i)n+=!!t->runtime.active[i];return n;
}
static unsigned projectile_first(const ProjectileCase *t) {
    unsigned i;for(i=0;i<100;++i)if(t->runtime.active[i])return i;return 100;
}
static void projectile_busy(ProjectileCase *t,unsigned slot) {
    WeaponCase *w=&t->weapon;
    CHECK(t->runtime.updating && w->combat.updating && w->f->spawn.updating && w->f->player.updating);
    CHECK(ssa_projectile_runtime_step(&t->runtime,slot,1,&t->ops)==-1);
    CHECK(ssa_projectile_runtime_release(&t->runtime,slot)==-1);
    CHECK(ssa_projectile_runtime_free(&t->runtime)==-1);
    CHECK(ssa_weapon_runtime_step(&w->weapon,&w->game,1,1,&w->ops)==-1);
    CHECK(ssa_combat_runtime_free(&w->combat)==-1);
    CHECK(ssa_script_spawn_free(&w->f->spawn)==-1);
    projectile_guards+=6;++projectile_busy_checks;
}
static int projectile_particle(void *context,const SsaProjectileRuntime *r,unsigned id,
    int32_t x,int32_t y,int32_t z) {
    ProjectileCase *t=context;unsigned slot=projectile_first(t);
    CHECK(r==&t->runtime && slot<100 && (id==4 || id==15));
    (void)x;(void)y;(void)z;
    projectile_busy(t,slot);++projectile_particles;
    return t->fail_particle?-1:0;
}
static void projectile_init(ProjectileCase *t,Fixture *f,const SsaInventoryRules *inv,
    const SsaWeaponRules *rules,const SsaWeaponAnimations *anim,const SsaMathTables *math) {
    memset(t,0,sizeof(*t));weapon_init(&t->weapon,f,inv,rules,anim);
    CHECK(ssa_projectile_runtime_init(&t->runtime,&t->weapon.combat,math)==0);
    CHECK(ssa_weapon_runtime_bind_projectiles(&t->weapon.weapon,&t->runtime)==0);
    t->ops=(SsaProjectileRuntimeOps){&t->weapon.combat_ops,projectile_particle,t};
    t->weapon.damage.god_mode=1;
}
static int projectile_tick(ProjectileCase *t,unsigned slot,unsigned ticks) {
    SsaEntity *e=t->runtime.projectiles[slot].entity;
    int32_t x=e?e->body.x:0,y=e?e->body.y:0;int result;
    ++t->weapon.damage.frame_counter;
    result=ssa_projectile_runtime_step(&t->runtime,slot,ticks,&t->ops);++projectile_frames;
    CHECK(!t->runtime.updating && !t->weapon.combat.updating && !t->weapon.f->spawn.updating);
    CHECK(!t->weapon.f->player.updating);
    if(t->runtime.active[slot] && e && (e->body.x!=x || e->body.y!=y))++projectile_moves;
    return result;
}
static unsigned projectile_fire(ProjectileCase *t,unsigned weapon) {
    WeaponCase *w=&t->weapon;unsigned i,slot,count=projectile_count(t),before=w->game.shots[weapon-1];
    int16_t ammo=w->f->players[0].inventory.ammo[weapon-1];
    select_for_test(w,weapon);
    for(i=0;i<128 && projectile_count(t)==count;++i) {
        CHECK(weapon_tick(w,1,i==0)==0);weapon_animate(w);
    }
    CHECK(projectile_count(t)==count+1 && w->game.shots[weapon-1]==before+1);
    CHECK(w->f->players[0].inventory.ammo[weapon-1]==ammo-1 && !w->unsupported);
    slot=projectile_first(t);CHECK(slot<100);
    CHECK(t->runtime.projectiles[slot].entity->owner==&w->f->world);
    CHECK(t->runtime.projectiles[slot].source==&w->f->players[0].entity);
    CHECK(t->runtime.projectiles[slot].owner==1);
    CHECK(t->runtime.projectiles[slot].entity==&w->f->spawn.actors[slot].entity);
    ++projectile_shots[weapon==6?0:weapon==7?1:2];return slot;
}
static void projectile_cleanup(ProjectileCase *t) {
    CHECK(ssa_projectile_runtime_free(&t->runtime)==0);weapon_cleanup(&t->weapon);
}

static void projectile_extras(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim) {
    Fixture f;ProjectileCase t;unsigned i,slot,dir;
    setup(&f,rom,size,0,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
    /* Real wall reflection, with no injected collision/motion results. */
    for(dir=0;dir<16 && !projectile_bounces;++dir) {
        f.players[0].motion.facing=(uint8_t)(dir*16);
        f.players[0].motion.movement.heading=(uint16_t)(dir*4096);
        slot=projectile_fire(&t,10);
        for(i=0;i<512 && t.runtime.active[slot];++i) {
            SsaEntity *e=t.runtime.projectiles[slot].entity;
            int32_t vx=e->body.velocity_x,vy=e->body.velocity_y,x=e->body.x,y=e->body.y;
            CHECK(projectile_tick(&t,slot,1)>=0);
            if(t.runtime.active[slot] && x==e->body.x && y==e->body.y &&
               t.runtime.bounce.fraction<=0x10000 &&
               (vx!=e->body.velocity_x || vy!=e->body.velocity_y))++projectile_bounces;
        }
        CHECK(!t.runtime.active[slot]);++projectile_expirations;
    }
    CHECK(projectile_bounces);
    /* A failed particle occurs after native damage but before release. */
    slot=projectile_fire(&t,6);t.fail_particle=1;
    for(i=0;i<512 && t.runtime.active[slot];++i) {
        int result=projectile_tick(&t,slot,1);
        if(result<0) {++projectile_failures;break;}
    }
    CHECK(i<512 && t.runtime.active[slot] && f.spawn.actors[slot].owned);
    CHECK(!t.weapon.damage.credit_player && !t.weapon.damage.shot_hit_count);
    t.fail_particle=0;CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
    /* Allocation uses the same 100 slots as script actors and the player. */
    f.players[0].inventory.ammo[5]=200;
    for(i=0;i<99;++i)projectile_fire(&t,6);
    CHECK(f.spawn.pool.active_count==100 && f.world.entity_count==100);
    CHECK(projectile_count(&t)==99);++projectile_pool_full;
    {
        unsigned before=t.weapon.game.shots[5];int16_t ammo=f.players[0].inventory.ammo[5];int result=0;
        select_for_test(&t.weapon,6);
        for(i=0;i<128 && !result;++i) {result=weapon_tick(&t.weapon,1,i==0);weapon_animate(&t.weapon);}
        CHECK(result==-1 && f.players[0].inventory.ammo[5]==ammo && t.weapon.game.shots[5]==before);
        CHECK(f.spawn.pool.active_count==100 && f.world.entity_count==100);++projectile_failures;
    }
    slot=projectile_first(&t);CHECK(slot<100);CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
    CHECK(!f.spawn.pool.occupied[slot] && f.world.entity_count==99);
    projectile_fire(&t,6);CHECK(t.runtime.active[slot] && f.spawn.pool.active_count==100);++projectile_pool_reuses;
    CHECK(ssa_projectile_runtime_step(&t.runtime,100,1,&t.ops)==-1);++projectile_guards;
    CHECK(ssa_projectile_runtime_step(&t.runtime,slot,256,&t.ops)==-1);++projectile_guards;
    CHECK(ssa_projectile_runtime_release(&t.runtime,0)==-1);++projectile_guards;
    CHECK(ssa_projectile_runtime_init(&t.runtime,&t.weapon.combat,math)==-1);++projectile_guards;
    projectile_cleanup(&t);CHECK(f.world.entity_count==1 && f.spawn.pool.active_count==1);cleanup(&f);
}

static void projectile_freeze_case(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    unsigned level,dir,found=0;
    for(level=0;level<16 && !found;++level) {
        Fixture f;ProjectileCase t;CdCase c;SsaScriptActor *a;
        setup(&f,rom,size,level,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
        script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
        a=&f.spawn.actors[c.runtime.slot];
        for(dir=0;dir<4 && !found;++dir) {
            static const int dx[4]={0,1,0,-1},dy[4]={1,0,-1,0};
            SsaPlayerSpawn *p=&f.players[0];unsigned slot,effect=100,i;
            int32_t x=p->entity.body.x+(dx[dir]*208+dy[dir]*40)*65536;
            int32_t y=p->entity.body.y+(dy[dir]*208-dx[dir]*40)*65536;
            int moved=ssa_world_try_move(&f.world,&a->entity,x,y);
            CHECK(moved>=0);if(!moved)continue;
            p->motion.facing=(uint8_t)(dir*64);p->motion.movement.heading=(uint16_t)(dir*16384);
            slot=projectile_fire(&t,7);
            CHECK(projectile_tick(&t,slot,1)>=0);
            if(!(a->entity.body.flags&0x400)) {
                if(t.runtime.active[slot])CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
                continue;
            }
            CHECK(!t.runtime.active[slot] && a->activation.health==30 && a->parameter34==30);++projectile_freezes;++projectile_hits[1];found=1;
            for(i=0;i<100;++i)if(t.runtime.active[i] && t.runtime.projectiles[i].entity->type==0xe2)effect=i;
            CHECK(effect<100 && t.runtime.projectiles[effect].source==&a->entity);
            CHECK(f.spawn.actors[effect].animation==0x5b && f.spawn.actors[effect].animation_ticks==5);
            CHECK(t.runtime.projectiles[effect].entity->owner==&f.world);
            CHECK(projectile_tick(&t,effect,1)==0);
            CHECK(t.runtime.projectiles[effect].entity->body.x==a->entity.body.x);
            CHECK(t.runtime.projectiles[effect].entity->body.y==a->entity.body.y);
            CHECK(t.runtime.projectiles[effect].entity->body.z==a->entity.body.z+a->entity.body.height+48*65536);
            /* Follow uses +48 despite creation's +16; termination follows the
             * actual CD death flag20, not the separate freeze flag400. */
            CHECK(ssa_combat_runtime_damage(&t.weapon.combat,&a->entity,&p->entity,NULL,100,&t.weapon.combat_ops)==0);
            {
                SsaCombatCdOps cd_ops={&t.weapon.combat_ops,cd_event,&c};c.frame.ticks=1;
                CHECK(ssa_combat_cd_step(&t.weapon.combat,&c.runtime,&c.frame,&c.queries,&cd_ops)==0);
                CHECK(a->activation.state==2 && a->entity.body.flags&0x20 && a->group_released);
            }
            CHECK(projectile_tick(&t,effect,1)==1 && !t.runtime.active[effect]);++projectile_effect_releases;
            CHECK(!projectile_count(&t) && f.world.entity_count==2);
            expire(&c);
        }
        projectile_cleanup(&t);cleanup(&f);
    }
    CHECK(found);
}

static void projectile_penetration_case(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed,unsigned incoming_credit) {
    unsigned level,dir,found=0;
    for(level=0;level<16 && !found;++level) {
        Fixture f;ProjectileCase t;CdCase c;SsaScriptActor *a;
        setup(&f,rom,size,level,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
        script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
        a=&f.spawn.actors[c.runtime.slot];
        for(dir=0;dir<4 && !found;++dir) {
            static const int dx[4]={0,1,0,-1},dy[4]={1,0,-1,0};
            SsaPlayerSpawn *p=&f.players[0];unsigned slot,i;int32_t slope;uint32_t incoming_hits=41;
            int moved=ssa_world_try_move(&f.world,&a->entity,p->entity.body.x+dx[dir]*160*65536,
                p->entity.body.y+dy[dir]*160*65536);
            CHECK(moved>=0);if(!moved)continue;
            p->motion.facing=(uint8_t)(dir*64);p->motion.movement.heading=(uint16_t)(dir*16384);
            CHECK(ssa_weapon_aim_query(&t.weapon.weapon.trace,&p->entity,0,0,0x600000,
                p->motion.movement.heading,256*65536,&slope)==1);
            if(t.weapon.weapon.trace.target!=&a->entity)continue;
            slot=projectile_fire(&t,10);
            if(!t.runtime.projectiles[slot].speed) {
                CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);continue;
            }
            if(incoming_credit) {
                t.weapon.damage.credit_player=1;t.weapon.damage.shot_hit_count=&incoming_hits;
            }
            for(i=0;i<32 && t.runtime.active[slot] && a->activation.health>0;++i)
                CHECK(projectile_tick(&t,slot,1)>=0);
            CHECK(!a->activation.health && t.runtime.active[slot]);
            CHECK(t.runtime.projectiles[slot].health==130 && t.runtime.projectiles[slot].speed==16*65536);
            /* Direct67 damage preserves incoming credit globals, including
             * an independently selected hit counter. Its later explosion
             * explicitly installs the projectile owner/weapon credit. */
            CHECK(a->entity.body.flags&1 && t.weapon.score==(incoming_credit?2000u:0u) && !a->group_released);
            CHECK(!t.weapon.weapon.hits[9]);
            CHECK(t.weapon.damage.credit_player==incoming_credit);
            CHECK(t.weapon.damage.shot_hit_count==(incoming_credit?&incoming_hits:NULL));
            CHECK(incoming_hits==(incoming_credit?42u:41u));
            t.weapon.damage.credit_player=0;t.weapon.damage.shot_hit_count=NULL;
            ++projectile_penetrations;++projectile_hits[2];found=1;
            CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
            {
                SsaCombatCdOps cd_ops={&t.weapon.combat_ops,cd_event,&c};c.frame.ticks=1;
                CHECK(ssa_combat_cd_step(&t.weapon.combat,&c.runtime,&c.frame,&c.queries,&cd_ops)==0);
                CHECK(a->activation.state==2 && a->group_released);expire(&c);
            }
        }
        projectile_cleanup(&t);cleanup(&f);
    }
    CHECK(found);
}

typedef struct ProjectileMultiplayer {
    Multiplayer base;
    SsaProjectileRuntime runtime;
    SsaProjectileRuntimeOps ops;
} ProjectileMultiplayer;
static int projectile_mp_combat(void *context,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
    SsaCombatDamageEvent kind,uint32_t a,uint32_t b) {
    ProjectileMultiplayer *t=context;Multiplayer *m=&t->base;
    CHECK(r==&m->combat && v && kind==SSA_COMBAT_DAMAGE_NETWORK && a<2 && b>0 && b<=100);
    CHECK(m->damage.credit_player==m->active+1 && m->damage.credited_kills==&m->kills[m->active]);
    CHECK(m->damage.shot_hit_count==&m->weapon[m->active].hits[5]);
    CHECK(ssa_weapon_runtime_step(&m->weapon[1-m->active],&m->game,1,1,&m->ops)==-1);
    CHECK(ssa_projectile_runtime_free(&t->runtime)==-1);
    ++projectile_mp_packets;++projectile_busy_checks;projectile_guards+=2;return 0;
}
static int projectile_mp_particle(void *context,const SsaProjectileRuntime *r,unsigned id,
    int32_t x,int32_t y,int32_t z) {
    ProjectileMultiplayer *t=context;Multiplayer *m=&t->base;(void)x;(void)y;(void)z;
    CHECK(r==&t->runtime && id==4 && r->updating);
    CHECK(!m->damage.credit_player && !m->damage.shot_hit_count && m->kills[m->active]==1);
    ++projectile_particles;return 0;
}
static void projectile_multiplayer_case(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing) {
    Fixture f;ProjectileMultiplayer t={0};Multiplayer *m=&t.base;
    SsaEntity *reserved[2];SsaPlayerRuntime *players[2];unsigned i,j;
    setup(&f,rom,size,0,math,defs,100);m->f=&f;
    CHECK(ssa_script_spawn_free(&f.spawn)==0);
    f.players[1]=f.players[0];memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=0xca;f.players[1].entity.player_index=1;
    CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);CHECK(ssa_slide_init(&m->slide,&f.map));
    CHECK(ssa_player_runtime_init(&m->second,&f.players[1],&f.world,&m->slide,math)==0);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;players[0]=&f.player;players[1]=&m->second;
    m->damage.player_count=2;m->damage.difficulty=1;m->damage.kill_limit=99;m->damage.credited_kills=&m->borrowed;m->borrowed=17;
    CHECK(ssa_combat_runtime_init(&m->combat,&f.spawn,reserved,2,players,2,&m->damage,&m->score)==0);
    CHECK(ssa_projectile_runtime_init(&t.runtime,&m->combat,math)==0);
    m->effects=(SsaCombatRuntimeOps){projectile_mp_combat,&t};m->ops=(SsaWeaponRuntimeOps){&m->effects,mp_event,mp_impact,m};
    t.ops=(SsaProjectileRuntimeOps){&m->effects,projectile_mp_particle,&t};m->game.players=2;m->game.ticks=1;
    for(i=0;i<2;++i) {
        CHECK(ssa_player_gameplay_init(&m->gp[i],players[i],&f.plan)==0);
        m->gp[i].weapon.current=m->gp[i].weapon.requested=6;m->gp[i].weapon_view.y=0x5000;
        m->gp[i].weapon_view.player_number=(uint8_t)(i+1);m->gp[i].weapon_view.flags=2;
        f.players[i].inventory.weapons=0x3f;f.players[i].inventory.ammo[5]=100;
        f.players[i].motion.movement.flags=4;f.players[i].motion.movement.weapon_flags=0;
        f.players[i].motion.facing=0;f.players[i].motion.movement.heading=0;
        CHECK(ssa_weapon_runtime_init(&m->weapon[i],&m->gp[i],&m->combat,inv,rules,anim)==0);
        m->weapon[i].credited_kills=&m->kills[i];CHECK(ssa_weapon_runtime_bind_projectiles(&m->weapon[i],&t.runtime)==0);
    }
    for(i=0;i<2;++i) {
        SsaScriptActor *a;unsigned enemy,pslot=100;
        m->active=i;m->game.motion_index=(uint8_t)i;m->damage.local_player=(uint16_t)i;
        script(&f,rom,thing,1,1);step(&f,1);enemy=first_owned(&f);a=&f.spawn.actors[enemy];
        CHECK(ssa_script_spawn_activate(&f.spawn,enemy,59,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,enemy,1,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,enemy,29,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,enemy,1,&f.ops)==1);
        CHECK(ssa_world_try_move(&f.world,&a->entity,f.players[i].entity.body.x,f.players[i].entity.body.y+80*65536)==1);
        for(j=0;j<128 && pslot==100;++j) {
            unsigned k;++m->game.frame;
            CHECK(ssa_weapon_runtime_step(&m->weapon[i],&m->game,1,j==0,&m->ops)==0);
            CHECK(ssa_weapon_runtime_animate(&m->weapon[i],0,1)==0);
            for(k=0;k<100;++k)if(t.runtime.active[k])pslot=k;
        }
        CHECK(pslot<100 && t.runtime.projectiles[pslot].owner==i+1 && t.runtime.projectiles[pslot].weapon==6);
        CHECK(m->damage.credited_kills==&m->borrowed && m->borrowed==17);
        for(j=0;j<80 && t.runtime.active[pslot];++j) {
            CHECK(ssa_projectile_runtime_step(&t.runtime,pslot,1,&t.ops)>=0);++projectile_frames;
        }
        CHECK(!a->activation.health && m->kills[i]==1 && !m->score);++projectile_mp_kills;
        CHECK(!m->damage.credit_player && !m->damage.shot_hit_count);
        CHECK(m->damage.credited_kills==&m->borrowed && m->borrowed==17);
        CHECK(!t.runtime.active[pslot]);
        CHECK(f.players[0].inventory.health==100 && f.players[1].inventory.health==100);
        CHECK(ssa_script_spawn_defeat(&f.spawn,enemy)==0);CHECK(ssa_script_spawn_release(&f.spawn,enemy)==0);
    }
    CHECK(m->kills[0]==1 && m->kills[1]==1 && projectile_mp_packets);
    CHECK(ssa_projectile_runtime_free(&t.runtime)==0);
    for(i=0;i<2;++i){CHECK(ssa_weapon_runtime_free(&m->weapon[i])==0);CHECK(ssa_player_gameplay_free(&m->gp[i])==0);}
    CHECK(ssa_combat_runtime_free(&m->combat)==0);CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&m->second)==0);ssa_slide_free(&m->slide);
    CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);cleanup(&f);
}

#ifndef SSA_PROJECTILE_TEST_MAIN
#define SSA_PROJECTILE_TEST_MAIN main
#endif
int SSA_PROJECTILE_TEST_MAIN(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations animations;
    int32_t speeds[12];unsigned level,i,w,thing=100;
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
        Fixture f;ProjectileCase t;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);
        projectile_init(&t,&f,&inventory,&rules,&animations,&math);
        for(w=0;w<2;++w) {
            CdCase c;SsaScriptActor *a;SsaCombatCdOps cd_ops;unsigned slot;
            script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,&math,speeds[0]);activate(&c);
            a=&f.spawn.actors[c.runtime.slot];position_target(&t.weapon,&c);
            slot=projectile_fire(&t,w?10:6);
            for(i=0;i<80 && t.runtime.active[slot] && a->activation.health>0;++i)
                CHECK(projectile_tick(&t,slot,1)>=0);
            /* Close aim sets travel=0. Type67's explosion quarters its damage
             * while processing the player first, retaining25 for later CD. */
            CHECK(a->activation.health==(w?5:0) && a->entity.body.flags&1);++projectile_hits[w?2:0];
            CHECK(!a->group_released && f.level.script.alive[0]==1);
            if(t.runtime.active[slot])CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
            CHECK(!projectile_count(&t) && f.world.entity_count==2);
            cd_ops=(SsaCombatCdOps){&t.weapon.combat_ops,cd_event,&c};c.frame.ticks=1;
            CHECK(ssa_combat_cd_step(&t.weapon.combat,&c.runtime,&c.frame,&c.queries,&cd_ops)==0);
            CHECK(a->activation.state==2 && a->group_released);expire(&c);
        }
        {
            unsigned slot=projectile_fire(&t,7);
            for(i=0;i<512 && t.runtime.active[slot];++i)CHECK(projectile_tick(&t,slot,1)>=0);
            CHECK(!t.runtime.active[slot] && !projectile_count(&t));++projectile_expirations;
        }
        for(i=0;i<128;++i)player_step(&f,i);
        projectile_cleanup(&t);cleanup(&f);
    }
    CHECK(projectile_shots[0]==16 && projectile_shots[1]==16 && projectile_shots[2]==16);
    CHECK(projectile_hits[0]==16 && projectile_hits[2]==16 && projectile_particles && projectile_moves);
    projectile_extras(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations);
    projectile_freeze_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    projectile_penetration_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0],0);
    projectile_penetration_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0],1);
    projectile_multiplayer_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"projectile_frames\":%u,\"projectile_moves\":%u,\"shots_6_7_10\":[%u,%u,%u],\"hits_6_7_10\":[%u,%u,%u],\"particles\":%u,\"callback_reentry_checks\":%u,\"guards\":%u,\"failures\":%u,\"expired\":%u,\"pool_full\":%u,\"pool_reuses\":%u,\"freezes\":%u,\"bounces\":%u,\"effect_releases\":%u,\"penetrations\":%u,\"corpse_releases\":%u,\"multiplayer_kills\":%u,\"multiplayer_packets\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u}\n",
        checks,projectile_frames,projectile_moves,projectile_shots[0],projectile_shots[1],projectile_shots[2],
        projectile_hits[0],projectile_hits[1],projectile_hits[2],projectile_particles,projectile_busy_checks,
        projectile_guards,projectile_failures,projectile_expirations,projectile_pool_full,projectile_pool_reuses,
        projectile_freezes,projectile_bounces,projectile_effect_releases,projectile_penetrations,released,
        projectile_mp_kills,projectile_mp_packets,player_frames,airborne,third_frames);
    return 0;
}
