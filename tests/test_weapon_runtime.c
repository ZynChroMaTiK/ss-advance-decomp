#define SSA_CD_TEST_MAIN enemy_cd_fixture_main
#include "test_enemy_cd_runtime.c"
#include "ssa/weapon_runtime.h"

typedef struct WeaponCase {
    Fixture *f;
    SsaPlayerGameplay gameplay;
    SsaWeaponRuntime weapon;
    SsaCombatRuntime combat;
    SsaCombatDamage damage;
    SsaWeaponGame game;
    SsaCombatRuntimeOps combat_ops;
    SsaWeaponRuntimeOps ops;
    SsaPlayerGameplayOps gameplay_ops;
    uint32_t score;
    unsigned fail_event, fail_impact, unsupported;
} WeaponCase;
static unsigned weapon_frames, impacts, wall_impacts, sound_effects, weapon_particles;
static unsigned weapon_guards, weapon_failures, weapon_reentry, weapon_kills, weapon_respawns;
static unsigned switch_cycles, cooldown_frames, nearwall_cases, completed_animations, combat_sounds;
static unsigned shots_by_weapon[5], impact_by_weapon[5], weapon_respawn_frames;
static unsigned multiplayer_kills, multiplayer_respawns, multiplayer_frames, failure_impact_checks, cadence_shots;

static void weapon_busy(WeaponCase *t) {
    CHECK(t->weapon.updating && t->gameplay.updating && t->f->player.updating);
    CHECK(t->combat.updating && t->f->spawn.updating);
    CHECK(ssa_weapon_runtime_step(&t->weapon,&t->game,1,1,&t->ops)==-1);
    CHECK(ssa_weapon_runtime_animate(&t->weapon,0,1)==-1);
    CHECK(ssa_weapon_runtime_respawn(&t->weapon,0,3)==-1);
    CHECK(ssa_weapon_runtime_free(&t->weapon)==-1);
    CHECK(ssa_player_gameplay_free(&t->gameplay)==-1);
    CHECK(ssa_combat_runtime_free(&t->combat)==-1);
    CHECK(ssa_script_spawn_free(&t->f->spawn)==-1);
    ++weapon_reentry;weapon_guards+=7;
}
static int weapon_effect(void *context,const SsaWeaponRuntime *runtime,SsaWeaponEvent *ev) {
    WeaponCase *t=context;
    CHECK(runtime==&t->weapon);weapon_busy(t);
    CHECK(ev->kind!=SSA_WEAPON_HITSCAN && ev->kind!=SSA_WEAPON_EXPLOSION);
    if(ev->kind==SSA_WEAPON_SOUND_PLAY || ev->kind==SSA_WEAPON_SOUND_STOP) ++sound_effects;
    else if(ev->kind==SSA_WEAPON_PARTICLE) ++weapon_particles;
    else { ++t->unsupported;return -1; }
    return t->fail_event ? -1 : 0;
}
static int weapon_impact(void *context,const SsaWeaponRuntime *runtime,const SsaWeaponHit *hit) {
    WeaponCase *t=context;
    CHECK(runtime==&t->weapon && hit->impact);weapon_busy(t);
    if(hit->target) {
        unsigned i,found=0;
        for(i=0;i<100;++i) if(t->f->spawn.actors[i].owned && &t->f->spawn.actors[i].entity==hit->target) {
            CHECK(t->f->spawn.actors[i].activation.health>0);found=1;
        }
        CHECK(found);++impacts;
        if(t->gameplay.weapon.current<=5) ++impact_by_weapon[t->gameplay.weapon.current-1];
    } else ++wall_impacts;
    return t->fail_impact ? -1 : 0;
}
static int weapon_combat_effect(void *context,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
    SsaCombatDamageEvent kind,uint32_t a,uint32_t b) {
    WeaponCase *t=context;(void)a;(void)b;
    CHECK(r==&t->combat && v->entity==&t->f->players[0].entity);
    CHECK(kind==SSA_COMBAT_DAMAGE_SOUND);++combat_sounds;
    return 0;
}
static int weapon_life_event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
    int32_t a,int32_t b,int32_t c,int32_t d) {
    WeaponCase *t=context;(void)c;(void)d;
    CHECK(g==&t->gameplay && g->updating && t->f->player.updating);
    if(kind==SSA_LIFECYCLE_WEAPON_ANIMATION) {
        CHECK(a==0 && b==3);
        CHECK(ssa_weapon_runtime_respawn(&t->weapon,(uint16_t)a,(uint16_t)b)==0);
        CHECK(t->weapon.object.animation==0 && t->weapon.object.period==3);
        CHECK(t->weapon.object.y==g->weapon_view.y && g->weapon_action==(uint32_t)g->weapon_view.y);
        ++weapon_respawns;
    } else CHECK(kind==SSA_LIFECYCLE_NOTICE || kind==SSA_LIFECYCLE_MODE || kind==SSA_LIFECYCLE_EFFECT);
    return 0;
}
static int weapon_life_line(void *context,SsaPlayerGameplay *g,unsigned kind,uint16_t line) {
    WeaponCase *t=context;CHECK(g==&t->gameplay && (kind==1 || kind==2) && line<t->f->map.line_count);return 0;
}
static int weapon_life_sector(void *context,SsaPlayerRuntime *r,int16_t sector_id) {
    WeaponCase *t=context;CHECK(r==&t->f->player && sector_id==r->player->entity.sector);return 0;
}
static int weapon_life_animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned period) {
    WeaponCase *t=context;CHECK(r==&t->f->player && (id==0x65 || id==0x66 || id==0x68));
    CHECK(period==(id==0x68?8u:4u));return 0;
}
static int weapon_life_sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    WeaponCase *t=context;CHECK(r==&t->f->player && (id==13 || id==14 || id==17));return 0;
}
static void weapon_init(WeaponCase *t,Fixture *f,const SsaInventoryRules *inv,
    const SsaWeaponRules *rules,const SsaWeaponAnimations *anim) {
    SsaEntity *reserved[]={&f->players[0].entity};SsaPlayerRuntime *players[]={&f->player};unsigned i;
    memset(t,0,sizeof(*t));t->f=f;t->damage.player_count=1;t->damage.difficulty=1;
    CHECK(ssa_player_gameplay_init(&t->gameplay,&f->player,&f->plan)==0);
    t->gameplay.weapon.current=t->gameplay.weapon.requested=1;
    t->gameplay.weapon_view.x=0x7800;t->gameplay.weapon_view.y=0x5000;
    t->gameplay.weapon_view.flags=2;t->gameplay.weapon_view.kind=14;t->gameplay.weapon_view.player_number=1;
    f->players[0].inventory.weapons=0x3ff;
    for(i=0;i<10;++i) f->players[0].inventory.ammo[i]=100;
    f->players[0].motion.movement.flags|=4;
    CHECK(ssa_combat_runtime_init(&t->combat,&f->spawn,reserved,1,players,1,&t->damage,&t->score)==0);
    CHECK(ssa_weapon_runtime_init(&t->weapon,&t->gameplay,&t->combat,inv,rules,anim)==0);
    t->combat_ops=(SsaCombatRuntimeOps){weapon_combat_effect,t};
    t->ops=(SsaWeaponRuntimeOps){&t->combat_ops,weapon_effect,weapon_impact,t};
    t->gameplay_ops=(SsaPlayerGameplayOps){weapon_life_event,weapon_life_line,weapon_life_sector,
        weapon_life_animation,weapon_life_sound,t};
    t->game.players=1;t->game.ticks=1;
    CHECK(t->weapon.trace.walls.visited!=f->world.collision.visited);
    CHECK(t->weapon.interaction.visited!=f->player.view_collision.visited);
}
static int weapon_tick(WeaponCase *t,unsigned held,unsigned pressed) {
    int result;++t->game.frame;t->damage.frame_counter=t->game.frame;
    result=ssa_weapon_runtime_step(&t->weapon,&t->game,(uint16_t)held,(uint16_t)pressed,&t->ops);
    CHECK(!t->weapon.updating && !t->combat.updating && !t->f->spawn.updating);
    CHECK(!t->gameplay.updating && !t->f->player.updating && !t->weapon.controller.updating);
    CHECK(!t->damage.credit_player && !t->damage.shot_hit_count);
    CHECK(t->damage.source==&t->f->players[0].entity);
    CHECK(t->gameplay.weapon_action==(uint32_t)t->weapon.object.y);
    ++weapon_frames;return result;
}
static void weapon_animate(WeaponCase *t) {
    uint8_t done=t->weapon.object.done;
    CHECK(ssa_weapon_runtime_animate(&t->weapon,t->game.flags,t->game.ticks)==0);
    if(!done && t->weapon.object.done) ++completed_animations;
}
static void select_for_test(WeaponCase *t,unsigned id) {
    t->gameplay.weapon.current=t->gameplay.weapon.requested=id;
    t->gameplay.weapon.flags=0;t->f->players[0].motion.movement.weapon_flags=0;
    t->f->players[0].motion.movement.flags=4;t->gameplay.weapon.keys=4;
    t->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&t->weapon.object,&t->gameplay.weapon)==1);
}
static void position_target(WeaponCase *t,CdCase *c) {
    static const int directions[4][2]={{0,1},{1,0},{0,-1},{-1,0}};
    SsaPlayerSpawn *p=&t->f->players[0];SsaEntity *e=c->runtime.navigation.entity;
    unsigned i;int found=0;
    for(i=0;i<4 && !found;++i) {
        int32_t slope;
        int moved=ssa_world_try_move(&t->f->world,e,p->entity.body.x+directions[i][0]*80*65536,
            p->entity.body.y+directions[i][1]*80*65536);
        CHECK(moved>=0);
        if(moved!=1) continue;
        p->motion.facing=(uint8_t)(i*64);p->motion.movement.heading=(uint16_t)(i*16384);
        CHECK(ssa_weapon_aim_query(&t->weapon.trace,&p->entity,0,0,0x600000,
            p->motion.movement.heading,96*65536,&slope)==1);
        found=t->weapon.trace.target==e;
    }
    CHECK(found);
}
static void weapon_cleanup(WeaponCase *t) {
    CHECK(ssa_weapon_runtime_free(&t->weapon)==0);
    CHECK(ssa_player_gameplay_free(&t->gameplay)==0);
    CHECK(ssa_combat_runtime_free(&t->combat)==0);
}

static unsigned chainsaw_damage_cases,chainsaw_range_cases;
static void chainsaw_damage_cases_run(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    unsigned difficulty,direct;
    for(difficulty=0;difficulty<3;++difficulty)for(direct=0;direct<2;++direct) {
        static const int health[]={20,23,26};
        Fixture f;WeaponCase t;CdCase c;SsaScriptActor *a;unsigned i,before=impacts;
        setup(&f,rom,size,0,math,defs,100);weapon_init(&t,&f,inv,rules,anim);
        script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
        a=&f.spawn.actors[c.runtime.slot];position_target(&t,&c);select_for_test(&t,1);
        t.damage.difficulty=(uint8_t)difficulty;
        t.weapon.automatic_period=7;t.weapon.automatic_gba_clock=1;t.weapon.clocked_chainsaw=1;
        f.player.free_aim=direct;f.player.pitch=0;
        /* Actual native hitscan/combat against a live, stationary CD target.
         * No bite before the Tommy gun interval; Hard must not round to zero. */
        for(i=0;i<9;++i) {
            CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);
            if(i<8)CHECK(a->activation.health==30 && !t.game.shots[0]);
        }
        CHECK(a->activation.health==health[difficulty] && (a->entity.body.flags&1));
        CHECK(t.game.shots[0]==1 && t.weapon.hits[0]==1 && impacts==before+1);
        CHECK(f.players[0].inventory.ammo[0]==100);
        ++chainsaw_damage_cases;weapon_cleanup(&t);cleanup(&f);
    }
}

static void chainsaw_range_cases_run(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    unsigned difficulty,direct;
    for(difficulty=0;difficulty<3;++difficulty)for(direct=0;direct<2;++direct) {
        static const int health[]={21,24,26};
        Fixture f;WeaponCase t;CdCase c;SsaScriptActor *a;unsigned i,dir,found=0,before=impacts;
        setup(&f,rom,size,0,math,defs,100);weapon_init(&t,&f,inv,rules,anim);
        script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
        a=&f.spawn.actors[c.runtime.slot];
        for(dir=0;dir<4 && !found;++dir){
            static const int dx[]={0,1,0,-1},dy[]={1,0,-1,0};int32_t slope;
            SsaEntity *p=&f.players[0].entity;
            int moved=ssa_world_try_move(&f.world,&a->entity,p->body.x+dx[dir]*256*65536,p->body.y+dy[dir]*256*65536);
            CHECK(moved>=0);if(!moved)continue;
            CHECK(ssa_weapon_aim_query(&t.weapon.trace,p,0,0,96*65536,(uint16_t)(dir*16384),320*65536,&slope)==1);
            if(t.weapon.trace.target!=&a->entity)continue;
            /* Confirm an unobstructed target beyond the new reach, not a
             * blocked target that would accidentally make a range test pass. */
            CHECK(ssa_weapon_aim_query(&t.weapon.trace,p,0,0,96*65536,(uint16_t)(dir*16384),192*65536,&slope)==1);
            CHECK(t.weapon.trace.target!=&a->entity);
            found=1;
            f.players[0].motion.facing=(uint8_t)(dir*64);f.players[0].motion.movement.heading=(uint16_t)(dir*16384);
            select_for_test(&t,1);t.damage.difficulty=(uint8_t)difficulty;
            t.weapon.automatic_period=7;t.weapon.automatic_gba_clock=1;
            t.weapon.clocked_chainsaw=SSA_CHAINSAW_GBA_CADENCE;t.weapon.chainsaw_range=192;
            f.player.free_aim=direct;f.player.pitch=0;
            for(i=0;i<8;++i){CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);}
            CHECK(a->activation.health==30 && t.game.shots[0]==1 && impacts==before);
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
            CHECK(ssa_world_try_move(&f.world,&a->entity,p->body.x+dx[dir]*160*65536,p->body.y+dy[dir]*160*65536)==1);
            CHECK(ssa_weapon_aim_query(&t.weapon.trace,p,0,0,96*65536,(uint16_t)(dir*16384),96*65536,&slope)==1);
            CHECK(t.weapon.trace.target!=&a->entity);
            CHECK(ssa_weapon_aim_query(&t.weapon.trace,p,0,0,96*65536,(uint16_t)(dir*16384),192*65536,&slope)==1);
            CHECK(t.weapon.trace.target==&a->entity);
            for(i=0;i<8;++i){
                CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);
                if(i<7)CHECK(a->activation.health==30 && t.game.shots[0]==1);
            }
            CHECK(a->activation.health==health[difficulty] && t.game.shots[0]==2 && t.weapon.hits[0]==1);
            CHECK(impacts==before+1 && f.players[0].inventory.ammo[0]==100);
            /* Keep this controlled target alive and acknowledge each hurt
             * flag between updates. Verify a complete measured clock period:
             *85 bites carry exactly584 original elapsed damage ticks. */
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
            a->activation.health=1000;
            for(i=0;i<700 && t.game.shots[0]<87;++i){
                a->entity.body.flags&=UINT16_C(0xfffe);
                CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);
            }
            CHECK(i<700 && t.game.shots[0]==87 && !t.weapon.automatic_damage_phase);
            CHECK(a->activation.health==(difficulty==0?161:difficulty==1?416:660));
            CHECK(t.weapon.hits[0]==86 && f.players[0].inventory.ammo[0]==100);
            /* PC boost doubles base bite damage, before original difficulty
             * rounding. Range,85-bite timing and unlimited ammo stay intact. */
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
            t.weapon.double_chainsaw=1;a->activation.health=2500;
            for(i=0;i<700 && t.game.shots[0]<172;++i){
                a->entity.body.flags&=UINT16_C(0xfffe);
                CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);
            }
            CHECK(i<700 && t.game.shots[0]==172 && !t.weapon.automatic_damage_phase);
            CHECK(a->activation.health==2500-(difficulty==0?1752:difficulty==1?1168:680));
            CHECK(t.weapon.hits[0]==171 && f.players[0].inventory.ammo[0]==100);
        }
        CHECK(found);++chainsaw_range_cases;weapon_cleanup(&t);cleanup(&f);
    }
}

static void failure_and_gate_cases(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    Fixture f;WeaponCase t;CdCase c;SsaScriptActor *a;unsigned i;
    setup(&f,rom,size,0,math,defs,100);weapon_init(&t,&f,inv,rules,anim);
    script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
    a=&f.spawn.actors[c.runtime.slot];position_target(&t,&c);select_for_test(&t,2);
    t.fail_event=1;
    CHECK(weapon_tick(&t,1,1)==-1);++weapon_failures;
    CHECK(t.gameplay.weapon.flags&0x400 && t.weapon.object.animation==11);
    CHECK(a->activation.health==30 && f.players[0].inventory.ammo[1]==100);
    t.fail_event=0;t.fail_impact=1;
    CHECK(weapon_tick(&t,1,0)==-1);++weapon_failures;++failure_impact_checks;
    CHECK(a->activation.health==30 && f.players[0].inventory.ammo[1]==100 && !t.game.shots[1]);
    CHECK(!(t.gameplay.weapon.flags&0x1000));
    t.fail_impact=0;
    CHECK(weapon_tick(&t,1,0)==0);weapon_animate(&t);
    CHECK(a->activation.health==20 && f.players[0].inventory.ammo[1]==99 && t.game.shots[1]==1);
    /* Release only this test target; later cadence tests shoot actual walls. */
    CHECK(ssa_script_spawn_defeat(&f.spawn,c.runtime.slot)==0);
    CHECK(ssa_script_spawn_release(&f.spawn,c.runtime.slot)==0);
    for(i=0;i<3;++i) {
        static const unsigned projectile[3]={6,7,10};int16_t before;
        select_for_test(&t,projectile[i]);before=f.players[0].inventory.ammo[projectile[i]-1];
        CHECK(weapon_tick(&t,1,1)==0);weapon_animate(&t);
        CHECK(weapon_tick(&t,1,0)==-1);++weapon_failures;
        CHECK(f.players[0].inventory.ammo[projectile[i]-1]==before);
        CHECK(!(t.gameplay.weapon.flags&0x1000));
    }
    CHECK(t.unsupported==3);
    select_for_test(&t,2);
    {
        unsigned before=t.game.shots[1];
        for(i=0;i<100;++i) {CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);}
        CHECK(t.game.shots[1]>before+2);cadence_shots=t.game.shots[1]-before;
    }
    select_for_test(&t,1);
    /* F150 pause skips visibility, whereas input-state3 still updates it. */
    {
        SsaWeaponObject before=t.weapon.object;
        t.game.flags=1;f.players[0].motion.movement.flags|=8;
        CHECK(ssa_weapon_runtime_step(&t.weapon,&t.game,1,1,&t.ops)==0);
        CHECK(!memcmp(&before,&t.weapon.object,sizeof(before)));
        t.game.flags=0;t.gameplay.life_game.input_state=3;
        CHECK(ssa_weapon_runtime_step(&t.weapon,&t.game,1,1,&t.ops)==0);
        CHECK(!(t.weapon.object.flags&1));
        f.players[0].motion.movement.flags&=(uint16_t)~8u;
        CHECK(ssa_weapon_runtime_step(&t.weapon,&t.game,1,1,&t.ops)==0);
        CHECK(t.weapon.object.flags&1);t.gameplay.life_game.input_state=0;
    }
#define WGUARD(field,value) do { unsigned previous=(unsigned)(field);field=(value); \
    CHECK(ssa_weapon_runtime_step(&t.weapon,&t.game,1,1,&t.ops)==-1); \
    field=previous;++weapon_guards; } while(0)
    WGUARD(t.weapon.object.owner,0);WGUARD(t.game.players,2);
    WGUARD(t.damage.local_player,1);WGUARD(t.gameplay.weapon.current,0);
    WGUARD(t.weapon.updating,1);WGUARD(t.weapon.controller.updating,1);
    WGUARD(t.gameplay.updating,1);WGUARD(f.player.updating,1);
    WGUARD(t.combat.updating,1);WGUARD(f.spawn.updating,1);
    WGUARD(f.world.query_active,1);WGUARD(t.weapon.trace.updating,1);
#undef WGUARD
    CHECK(ssa_weapon_runtime_step(NULL,&t.game,1,1,&t.ops)==-1);
    CHECK(ssa_weapon_runtime_step(&t.weapon,NULL,1,1,&t.ops)==-1);
    CHECK(ssa_weapon_runtime_step(&t.weapon,&t.game,1,1,NULL)==-1);
    CHECK(ssa_weapon_runtime_animate(&t.weapon,0,256)==-1);
    CHECK(ssa_weapon_runtime_respawn(&t.weapon,233,3)==-1);
    CHECK(ssa_weapon_runtime_init(&t.weapon,&t.gameplay,&t.combat,inv,rules,anim)==-1);
    weapon_guards+=6;
    for(i=0;i<16 && !wall_impacts;++i) {
        select_for_test(&t,4);f.players[0].motion.facing=(uint8_t)(i*16);
        f.players[0].motion.movement.heading=(uint16_t)(i*4096);
        CHECK(weapon_tick(&t,1,1)==0);weapon_animate(&t);
        CHECK(weapon_tick(&t,1,0)==0);weapon_animate(&t);
    }
    /* Actual line endpoint sweep, changing only the interaction kind in a
     * private decoded map. This verifies native12230 near-wall lowering. */
    for(i=0;i<f.map.line_count && !nearwall_cases;++i) {
        uint8_t *line=f.map.data+f.map.lines+i*40;
        unsigned vertex=line[0]|(unsigned)line[1]<<8;
        const uint8_t *v=f.map.data+f.map.vertices+vertex*4;
        SsaEntity probe=f.players[0].entity;uint8_t lo=line[36],hi=line[37];
        int16_t x=(int16_t)(v[0]|(unsigned)v[1]<<8),y=(int16_t)(v[2]|(unsigned)v[3]<<8);
        probe.body.x=(int32_t)x*65536;probe.body.y=(int32_t)y*65536;
        line[36]=2;line[37]=0;
        if(ssa_player_interaction_query(&t.weapon.interaction,&probe,0,math)==0 && t.weapon.interaction.special_count) {
            CHECK(ssa_world_remove(&f.world,&f.players[0].entity)==1);
            f.players[0].entity.body.x=f.players[0].motion.x=probe.body.x;
            f.players[0].entity.body.y=f.players[0].motion.y=probe.body.y;
            CHECK(ssa_world_insert(&f.world,&f.players[0].entity)==1);
            f.players[0].motion.facing=0;f.players[0].motion.movement.heading=0;
            select_for_test(&t,1);
            CHECK(weapon_tick(&t,0,0)==0);
            CHECK(t.weapon.object.y==0x5800 && t.gameplay.weapon.flags&0x100);++nearwall_cases;
        }
        line[36]=lo;line[37]=hi;
    }
    CHECK(nearwall_cases && wall_impacts);
    weapon_cleanup(&t);cleanup(&f);
}

typedef struct Multiplayer {
    Fixture *f;SsaPlayerRuntime second;SsaSlide slide;SsaPlayerGameplay gp[2];
    SsaWeaponRuntime weapon[2];SsaCombatRuntime combat;SsaCombatDamage damage;
    SsaWeaponGame game;SsaCombatRuntimeOps effects;SsaWeaponRuntimeOps ops;
    SsaPlayerGameplayOps life_ops;uint32_t score;uint8_t kills[2],borrowed;
    unsigned active;
} Multiplayer;
static int mp_event(void *context,const SsaWeaponRuntime *r,SsaWeaponEvent *e) {
    Multiplayer *m=context;
    CHECK(r==&m->weapon[m->active]);
    CHECK(e->kind==SSA_WEAPON_SOUND_PLAY || e->kind==SSA_WEAPON_SOUND_STOP || e->kind==SSA_WEAPON_PARTICLE);
    CHECK(m->damage.credited_kills==&m->kills[m->active]);
    CHECK(m->f->player.updating && m->second.updating && m->f->spawn.updating);
    return 0;
}
static int mp_impact(void *context,const SsaWeaponRuntime *r,const SsaWeaponHit *hit) {
    Multiplayer *m=context;CHECK(r==&m->weapon[m->active] && hit->impact);
    CHECK(m->damage.credit_player==m->active+1 && m->damage.credited_kills==&m->kills[m->active]);
    CHECK(ssa_weapon_runtime_step(&m->weapon[1-m->active],&m->game,1,1,&m->ops)==-1);++weapon_guards;
    return 0;
}
static int mp_combat(void *context,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
    SsaCombatDamageEvent kind,uint32_t a,uint32_t b) {
    Multiplayer *m=context;(void)a;(void)b;CHECK(r==&m->combat && v);
    CHECK(kind==SSA_COMBAT_DAMAGE_SOUND || kind==SSA_COMBAT_DAMAGE_NETWORK);return 0;
}
static int mp_life_event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
    int32_t a,int32_t b,int32_t c,int32_t d) {
    Multiplayer *m=context;(void)c;(void)d;CHECK(g==&m->gp[0]);
    if(kind==SSA_LIFECYCLE_WEAPON_ANIMATION) {
        CHECK(a==0 && b==3 && g->updating && m->f->player.updating);
        CHECK(ssa_weapon_runtime_respawn(&m->weapon[0],(uint16_t)a,(uint16_t)b)==0);
        ++multiplayer_respawns;
    }
    return 0;
}
static int mp_line(void *c,SsaPlayerGameplay *g,unsigned k,uint16_t l) {(void)c;(void)g;(void)k;(void)l;return 0;}
static int mp_sector(void *c,SsaPlayerRuntime *r,int16_t s) {(void)c;(void)r;(void)s;return 0;}
static int mp_animation(void *c,SsaPlayerRuntime *r,unsigned a,unsigned p) {(void)c;(void)r;(void)a;(void)p;return 0;}
static int mp_sound(void *c,SsaPlayerRuntime *r,unsigned s) {(void)c;(void)r;(void)s;return 0;}
static void multiplayer_cases(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing) {
    Fixture f;Multiplayer m={0};SsaEntity *reserved[2];SsaPlayerRuntime *players[2];unsigned i,j;
    setup(&f,rom,size,0,math,defs,100);m.f=&f;
    CHECK(ssa_script_spawn_free(&f.spawn)==0);
    f.players[1]=f.players[0];memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=0xca;f.players[1].entity.player_index=1;
    CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_slide_init(&m.slide,&f.map));
    CHECK(ssa_player_runtime_init(&m.second,&f.players[1],&f.world,&m.slide,math)==0);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;
    players[0]=&f.player;players[1]=&m.second;
    m.damage.player_count=2;m.damage.difficulty=1;m.damage.kill_limit=99;
    m.damage.credited_kills=&m.borrowed;m.borrowed=17;
    CHECK(ssa_combat_runtime_init(&m.combat,&f.spawn,reserved,2,players,2,&m.damage,&m.score)==0);
    m.effects=(SsaCombatRuntimeOps){mp_combat,&m};m.ops=(SsaWeaponRuntimeOps){&m.effects,mp_event,mp_impact,&m};
    m.game.players=2;m.game.ticks=1;
    for(i=0;i<2;++i) {
        CHECK(ssa_player_gameplay_init(&m.gp[i],players[i],&f.plan)==0);
        m.gp[i].weapon.current=m.gp[i].weapon.requested=3;m.gp[i].weapon_view.y=0x5000;
        m.gp[i].weapon_view.player_number=(uint8_t)(i+1);m.gp[i].weapon_view.flags=2;
        f.players[i].inventory.weapons=7;f.players[i].inventory.ammo[2]=100;
        f.players[i].motion.movement.flags=4;f.players[i].motion.movement.weapon_flags=0;
        f.players[i].motion.facing=0;f.players[i].motion.movement.heading=0;
        CHECK(ssa_weapon_runtime_init(&m.weapon[i],&m.gp[i],&m.combat,inv,rules,anim)==0);
        CHECK(ssa_weapon_runtime_step(&m.weapon[i],&m.game,1,1,&m.ops)==-1);++weapon_guards;
        m.weapon[i].credited_kills=&m.kills[i];
    }
    for(i=0;i<2;++i) {
        SsaScriptActor *actor;unsigned slot;m.active=i;m.game.motion_index=(uint8_t)i;m.damage.local_player=(uint8_t)i;
        script(&f,rom,thing,1,1);step(&f,1);slot=first_owned(&f);actor=&f.spawn.actors[slot];
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,59,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,1,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,29,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,1,&f.ops)==1);
        CHECK(ssa_world_try_move(&f.world,&actor->entity,f.players[i].entity.body.x,f.players[i].entity.body.y+80*65536)==1);
        for(j=0;j<2;++j) {
            ++m.game.frame;CHECK(ssa_weapon_runtime_step(&m.weapon[i],&m.game,1,j==0,&m.ops)==0);
            ++multiplayer_frames;
            CHECK(ssa_weapon_runtime_animate(&m.weapon[i],0,1)==0);
            CHECK(m.damage.credited_kills==&m.borrowed && m.borrowed==17);
        }
        CHECK(!actor->activation.health && m.kills[i]==1 && !m.score);++multiplayer_kills;
        CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==0);CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);
    }
    CHECK(m.kills[0]==1 && m.kills[1]==1);
    {
        static const int direction[4][2]={{0,1},{1,0},{0,-1},{-1,0}};int moved=0;
        int32_t x=f.players[1].entity.body.x,y=f.players[1].entity.body.y;
        for(i=0;i<4 && !moved;++i) {
            moved=ssa_world_try_move(&f.world,&f.players[1].entity,x+direction[i][0]*256*65536,y+direction[i][1]*256*65536);
            CHECK(moved>=0);
        }
        CHECK(moved==1);f.players[1].motion.x=f.players[1].entity.body.x;f.players[1].motion.y=f.players[1].entity.body.y;
    }
    m.life_ops=(SsaPlayerGameplayOps){mp_life_event,mp_line,mp_sector,mp_animation,mp_sound,&m};
    {
        SsaSpawnMarker marker;SsaSpawnMarker *saved=f.plan.player_starts;size_t count=f.plan.player_count;
        const SsaEntity *peers[2]={reserved[0],reserved[1]};
        marker.x=(int16_t)(f.players[0].motion.x/65536);marker.y=(int16_t)(f.players[0].motion.y/65536);
        marker.z=f.players[0].entity.body.floor;marker.heading=0;
        f.plan.player_starts=&marker;f.plan.player_count=1;
        f.players[0].inventory.health=f.players[0].entity_health=0;
        f.input.motion.player_count=2;f.input.motion.players=peers;f.input.motion.slot=0;
        f.input.jump_down=1;f.input.keys=0;f.input.yaw_delta=0;
        CHECK(ssa_player_runtime_key(&f.player,'H',1,0,1));f.player.jump.launch_velocity=7*65536;
        m.active=0;m.game.motion_index=0;m.damage.local_player=0;
        for(j=0;j<301;++j) {
            int result=ssa_player_gameplay_step(&m.gp[0],&f.input,&m.life_ops);
            if(result) fprintf(stderr,"mpdeath frame%u health%d ticks%u bodyowner%p world%p xy%d,%d body%d,%d\n",j,f.players[0].inventory.health,m.gp[0].lifecycle.death_ticks,(void*)f.players[0].entity.owner,(void*)&f.world,f.players[0].motion.x,f.players[0].motion.y,f.players[0].entity.body.x,f.players[0].entity.body.y);
            CHECK(result==0);
            CHECK(f.player.view.third_person && f.player.jump.launch_velocity==7*65536 && f.player.jump.was_down);
            CHECK(ssa_weapon_runtime_step(&m.weapon[0],&m.game,0,0,&m.ops)==0);
            ++multiplayer_frames;
            CHECK(ssa_weapon_runtime_animate(&m.weapon[0],0,1)==0);
        }
        CHECK(multiplayer_respawns==1 && f.players[0].inventory.health==100);
        CHECK(m.gp[0].weapon.current==1 && m.weapon[0].object.animation==0 && m.weapon[0].object.period==3);
        CHECK(m.weapon[0].object.owner==1 && m.weapon[0].object.type==2);
        f.plan.player_starts=saved;f.plan.player_count=count;
    }
    for(i=0;i<2;++i) {CHECK(ssa_weapon_runtime_free(&m.weapon[i])==0);CHECK(ssa_player_gameplay_free(&m.gp[i])==0);}
    CHECK(ssa_combat_runtime_free(&m.combat)==0);CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&m.second)==0);ssa_slide_free(&m.slide);
    CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);cleanup(&f);
}

#ifndef SSA_WEAPON_TEST_MAIN
#define SSA_WEAPON_TEST_MAIN main
#endif
int SSA_WEAPON_TEST_MAIN(int argc,char **argv) {
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
    for(i=0;i<64;++i) if(defs.thing[i].type==0xcd && !defs.thing[i].variant) {thing=i;break;}
    CHECK(thing<64);
    chainsaw_damage_cases_run(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    chainsaw_range_cases_run(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    for(level=0;level<16;++level) {
        Fixture f;WeaponCase t;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);
        weapon_init(&t,&f,&inventory,&rules,&animations);
        for(w=1;w<=5;++w) {
            CdCase c;SsaScriptActor *actor;SsaCombatCdOps cd_ops;
            unsigned first_shots=t.game.shots[w-1],first_impacts=impacts;uint32_t score=t.score;
            select_for_test(&t,w);script(&f,rom,thing,1,1);step(&f,1);
            bind(&c,&f,&math,speeds[0]);activate(&c);actor=&f.spawn.actors[c.runtime.slot];
            position_target(&t,&c);
            for(i=0;i<240 && actor->activation.health==30;++i) {
                CHECK(weapon_tick(&t,1,i==0)==0);weapon_animate(&t);
            }
            {
                static const int expected[5]={29,20,0,27,26};
                CHECK(actor->activation.health==expected[w-1] && actor->entity.body.flags&1);
            }
            CHECK(t.score==score+(w==3?2000u:0u) && !actor->group_released && f.level.script.alive[0]==1);
            CHECK(impacts>first_impacts && t.game.shots[w-1]>first_shots);
            shots_by_weapon[w-1]+=t.game.shots[w-1]-first_shots;if(w==3) ++weapon_kills;
            CHECK(t.weapon.hits[w-1]>0);
            /* Keep the player alive for the native CD radial death effect. */
            f.players[0].inventory.health=f.players[0].entity_health=100;
            f.players[0].entity.body.flags&=(uint16_t)~1u;
            cd_ops=(SsaCombatCdOps){&t.combat_ops,cd_event,&c};
            c.frame.ticks=1;
            CHECK(ssa_combat_cd_step(&t.combat,&c.runtime,&c.frame,&c.queries,&cd_ops)==0);
            CHECK(actor->activation.state==2 && actor->group_released && !f.level.script.alive[0]);
            expire(&c);
            CHECK(t.score==score+(w==3?2000u:0u));
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
        }
        /* Queue selection through lower/commit/raise, then keep firing inhibited
         * until the real cooldown expires. */
        select_for_test(&t,1);f.players[0].inventory.weapons=1;
        f.players[0].inventory.pending_count=1;f.players[0].inventory.pending_weapons[0]=2;
        CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
        CHECK(!f.players[0].inventory.pending_count && t.gameplay.weapon.requested==2);
        for(i=0;i<64 && (t.gameplay.weapon.current!=2 || t.weapon.object.y!=0x5000);++i) {
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
        }
        CHECK(t.gameplay.weapon.current==2 && t.weapon.object.y==0x5000);
        CHECK(t.weapon.object.animation==1 && !(t.gameplay.weapon.flags&0x2100));++switch_cycles;
        t.weapon.controller.cooldown[1]=6;
        for(i=0;i<5;++i) {
            CHECK(weapon_tick(&t,1,0)==0);weapon_animate(&t);
            CHECK(!(t.gameplay.weapon.flags&0x400));++cooldown_frames;
        }
        CHECK(weapon_tick(&t,1,1)==0);weapon_animate(&t);
        CHECK(t.gameplay.weapon.flags&0x400);
        select_for_test(&t,1);f.players[0].inventory.health=f.players[0].entity_health=100;
        f.players[0].entity.body.flags&=(uint16_t)~1u;
        for(i=0;i<128;++i) {
            player_step(&f,i);
            CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);
        }
        /* Single-player death reaches input-state4 at301, not an automatic
         * respawn. Separately exercise the bound respawn callback with the
         * actual recovered view defaults while gameplay owns its update lock. */
        {
            unsigned view=f.player.view.third_person;int32_t launch=7*65536;
            SsaSpawnMarker marker;SsaSpawnMarker *saved=f.plan.player_starts;size_t count=f.plan.player_count;
            marker.x=(int16_t)(f.players[0].entity.body.x/65536);
            marker.y=(int16_t)(f.players[0].entity.body.y/65536);
            marker.z=f.players[0].entity.body.floor;marker.heading=f.players[0].motion.facing;
            f.plan.player_starts=&marker;f.plan.player_count=1;
            f.player.jump.launch_velocity=launch;f.input.jump_down=1;f.input.keys=0;f.input.yaw_delta=0;
            f.players[0].inventory.health=0;f.players[0].entity_health=0;
            for(i=0;i<301;++i) {
                CHECK(ssa_player_gameplay_step(&t.gameplay,&f.input,&t.gameplay_ops)==0);
                CHECK(f.player.view.third_person==view && f.player.jump.launch_velocity==launch);
                CHECK(weapon_tick(&t,0,0)==0);weapon_animate(&t);++weapon_respawn_frames;
            }
            CHECK(!f.players[0].inventory.health && t.gameplay.life_game.input_state==4);
            t.gameplay.weapon_view.x=0x7800;t.gameplay.weapon_view.y=0xa000;
            t.gameplay.weapon_view.flags=2;t.gameplay.weapon_view.kind=14;t.gameplay.weapon_view.player_number=1;
            t.gameplay.updating=f.player.updating=1;
            CHECK(weapon_life_event(&t,&t.gameplay,SSA_LIFECYCLE_WEAPON_ANIMATION,0,3,0,0)==0);
            t.gameplay.updating=f.player.updating=0;
            CHECK(t.weapon.object.animation==0 && t.weapon.object.period==3 && f.player.jump.was_down);
            f.plan.player_starts=saved;f.plan.player_count=count;
        }
        weapon_cleanup(&t);cleanup(&f);
    }
    failure_and_gate_cases(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    multiplayer_cases(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing);
    CHECK(weapon_kills==16 && weapon_respawns==16 && airborne && third_frames && completed_animations);
    for(i=0;i<5;++i) CHECK(shots_by_weapon[i] && impact_by_weapon[i]);
    CHECK(chainsaw_damage_cases==6 && chainsaw_range_cases==6);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"singleplayer_weapon_frames\":%u,\"singleplayer_direct_kills\":%u,\"corpse_releases\":%u,\"singleplayer_target_impacts\":%u,\"wall_impacts\":%u,\"sound_effects\":%u,\"particles\":%u,\"completed_animations\":%u,\"switch_cycles\":%u,\"cooldown_frames\":%u,\"nearwall_cases\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"respawn_helper_checks\":%u,\"singleplayer_death_frames\":%u,\"guards\":%u,\"reentry_checks\":%u,\"failures\":%u,\"combat_sounds\":%u,\"initial_shots_by_weapon\":[%u,%u,%u,%u,%u],\"singleplayer_impacts_by_weapon\":[%u,%u,%u,%u,%u],\"multiplayer_kills\":%u,\"multiplayer_respawns\":%u,\"multiplayer_weapon_frames\":%u,\"pre_damage_failure_checks\":%u,\"cadence_shots\":%u,\"cd_proximity_deaths\":64}\n",
        checks,weapon_frames,weapon_kills,released,impacts,wall_impacts,sound_effects,weapon_particles,completed_animations,
        switch_cycles,cooldown_frames,nearwall_cases,player_frames,airborne,third_frames,weapon_respawns,weapon_respawn_frames,
        weapon_guards,weapon_reentry,weapon_failures,combat_sounds,shots_by_weapon[0],shots_by_weapon[1],shots_by_weapon[2],shots_by_weapon[3],shots_by_weapon[4],
        impact_by_weapon[0],impact_by_weapon[1],impact_by_weapon[2],impact_by_weapon[3],impact_by_weapon[4],
        multiplayer_kills,multiplayer_respawns,multiplayer_frames,failure_impact_checks,cadence_shots);
    return 0;
}
