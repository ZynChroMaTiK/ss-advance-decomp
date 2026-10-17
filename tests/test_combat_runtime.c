#define SSA_CD_TEST_MAIN enemy_cd_fixture_main
#include "test_enemy_cd_runtime.c"
#include "ssa/enemy_queries.h"
#include "ssa/combat_runtime.h"

typedef struct CombatCase {
    Fixture *fixture;
    SsaCombatRuntime combat;
    SsaCombatDamage damage;
    SsaCombatRuntimeOps effects;
    SsaCombatCdOps cd_ops;
    SsaEnemyQueries queries;
    CdCase enemies[2];
    unsigned current, fail_sound;
    uint32_t score, hits;
} CombatCase;
static unsigned combat_frames, combat_guards, combat_failures, combat_callbacks;
static unsigned damage_hits, chain_deaths, player_deaths, player_death_frames, rewards;
static void combat_busy(CombatCase *t) {
    Fixture *f=t->fixture;
    CHECK(t->combat.updating && f->spawn.updating && f->player.updating);
    CHECK(ssa_combat_runtime_free(&t->combat)==-1);
    CHECK(ssa_combat_runtime_damage(&t->combat,NULL,NULL,NULL,1,&t->effects)==-1);
    CHECK(ssa_combat_runtime_explode(&t->combat,&f->players[0].entity,NULL,NULL,1,1,1,&t->effects)==-1);
    CHECK(ssa_player_runtime_free(&f->player)==-1);
    CHECK(ssa_player_runtime_step(&f->player,&f->input,&f->player_ops)==-1);
    CHECK(ssa_script_spawn_free(&f->spawn)==-1);
    combat_guards+=6;
}
static int combat_effect(void *context,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
                         SsaCombatDamageEvent kind,uint32_t a,uint32_t b) {
    CombatCase *t=context;
    CHECK(r==&t->combat && v->entity==&t->fixture->players[0].entity);
    CHECK(kind==SSA_COMBAT_DAMAGE_SOUND && (a==15 || a==16) && !b);
    CHECK(t->fixture->players[0].entity_health==t->fixture->players[0].inventory.health);
    CHECK(t->damage.hurt_flash==9);combat_busy(t);++combat_callbacks;
    return t->fail_sound ? -1 : 0;
}
static int presentation(void *context,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,SsaEntity *other,
                         int32_t a,int32_t b,int32_t d) {
    CombatCase *t=context;
    CHECK(kind!=SSA_ENEMY_CD_CONTACT_DAMAGE && kind!=SSA_ENEMY_CD_EXPLOSION);
    combat_busy(t);
    return cd_event(&t->enemies[t->current],cd,kind,other,a,b,d);
}
static int ctick(CombatCase *t,unsigned index,unsigned ticks) {
    CdCase *c=&t->enemies[index];int result;
    t->current=index;c->frame.ticks=ticks;++c->frame.frame_counter;
    c->game.player_x=t->fixture->players[0].motion.x;c->game.player_y=t->fixture->players[0].motion.y;
    t->damage.frame_counter=c->frame.frame_counter;
    result=ssa_combat_cd_step(&t->combat,&c->runtime,&c->frame,&t->queries.ops,&t->cd_ops);
    CHECK(!t->combat.updating && !t->damage.updating && !t->fixture->spawn.updating && !t->fixture->player.updating);
    ++combat_frames;return result;
}
static void init_case(CombatCase *t,Fixture *f,const SsaMathTables *math,int32_t speed) {
    SsaEntity *reserved[]={&f->players[0].entity};SsaPlayerRuntime *players[]={&f->player};
    unsigned i;
    memset(t,0,sizeof(*t));t->fixture=f;
    t->damage.player_count=1;t->damage.difficulty=1;t->damage.credit_player=1;
    t->damage.shot_hit_count=&t->hits;
    t->effects=(SsaCombatRuntimeOps){combat_effect,t};t->cd_ops=(SsaCombatCdOps){&t->effects,presentation,t};
    CHECK(ssa_enemy_queries_init(&t->queries,&f->spawn,math,reserved,1)==0);
    for(i=0;i<2;++i) {
        CdCase *c=&t->enemies[i];unsigned slot=f->spawn.pool.active_slots[i+1];
        c->f=f;c->game.activation=&f->spawn.game;c->game.player=&f->players[0].entity;
        c->game.player_x=f->players[0].motion.x;c->game.player_y=f->players[0].motion.y;
        c->ops=(SsaEnemyCdRuntimeOps){&t->queries.ops,cd_event,c};
        c->frame=(SsaEnemyCdFrame){1,speed,0,0};c->check_busy=1;
        CHECK(ssa_enemy_cd_runtime_init(&c->runtime,&f->spawn,slot,math,&c->game,&c->walks)==0);
        activate(c);
    }
    CHECK(ssa_combat_runtime_init(&t->combat,&f->spawn,reserved,1,players,1,&t->damage,&t->score)==0);
    CHECK(ssa_combat_runtime_init(&t->combat,&f->spawn,reserved,1,players,1,&t->damage,&t->score)==-1);++combat_guards;
}
static int life_event_test(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
                             int32_t a,int32_t b,int32_t c,int32_t d) {
    Fixture *f=context;(void)a;(void)b;(void)c;(void)d;
    CHECK(g->runtime==&f->player);
    CHECK(kind==SSA_LIFECYCLE_NOTICE || kind==SSA_LIFECYCLE_MODE);return 0;
}
static int life_line_test(void *context,SsaPlayerGameplay *g,unsigned kind,uint16_t line_id) {
    Fixture *f=context;(void)kind;CHECK(g->runtime==&f->player && line_id<f->map.line_count);return 0;
}
static int life_animation_test(void *context,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    Fixture *f=context;CHECK(r==&f->player);CHECK(id==0x68 && ticks==8);return 0;
}
static int life_sound_test(void *context,SsaPlayerRuntime *r,unsigned id) {
    Fixture *f=context;CHECK(r==&f->player && id==17);return 0;
}
#include "test_combat_runtime_extra.inc"
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    int32_t speeds[12];unsigned level,i,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    for(i=0;i<64;++i) if(defs.thing[i].type==0xcd && !defs.thing[i].variant) {thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;CombatCase t;SsaPlayerSpawn *p;SsaScriptActor *a,*b;uint16_t movement_flags;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);script(&f,rom,thing,1,2);step(&f,1);step(&f,1);
        init_case(&t,&f,&math,speeds[0]);p=&f.players[0];
        a=&f.spawn.actors[t.enemies[0].runtime.slot];b=&f.spawn.actors[t.enemies[1].runtime.slot];
        p->inventory.stat_c6=p->inventory.stat_c4=100;movement_flags=p->motion.movement.flags;
        t.damage.source=&p->entity;t.damage.source_owner=&b->entity;t.damage.local_flags=0x1234;
        CHECK(ssa_combat_runtime_damage(&t.combat,&p->entity,&a->entity,NULL,30,&t.effects)==0);++damage_hits;
        CHECK(p->inventory.health==95 && p->entity_health==95 && p->inventory.stat_c6==60 && p->inventory.stat_c4==80);
        CHECK(f.player.frame.timer==30 && (p->entity.body.flags&1) && p->motion.movement.flags==movement_flags);
        CHECK(t.damage.source==&p->entity && t.damage.source_owner==&b->entity && t.damage.local_flags==0x1234);
        CHECK(ssa_combat_runtime_damage(&t.combat,&p->entity,&a->entity,NULL,30,&t.effects)==0);
        CHECK(p->inventory.health==95 && t.hits==2 && t.damage.enemy_hits[0]==1);
        for(i=0;i<128;++i) player_step(&f,i);
        CHECK(!(p->entity.body.flags&1));
        /* Move player back through world API for a real close-range chain.
         * Enemies have not moved since their scripted insertion. */
        CHECK(ssa_world_remove(&f.world,&p->entity)==1);
        p->entity.body.x=p->motion.x=a->entity.body.x;p->entity.body.y=p->motion.y=a->entity.body.y;
        CHECK(ssa_world_insert(&f.world,&p->entity)==1);
        CHECK(ssa_combat_runtime_damage(&t.combat,&a->entity,&p->entity,NULL,30,&t.effects)==0);++damage_hits;
        CHECK(!a->activation.health && (a->entity.body.flags&1) && t.score==2000);
        CHECK(!a->group_released && f.level.script.alive[0]==2);
        CHECK(ctick(&t,0,1)==0);
        CHECK(a->activation.state==2 && a->group_released && !b->activation.health && (b->entity.body.flags&1));
        CHECK(t.score==4000 && t.damage.enemy_kills[0]==2 && f.level.script.alive[0]==1);rewards+=2;
        CHECK(ctick(&t,1,1)==0 && b->activation.state==2 && b->group_released && !f.level.script.alive[0]);chain_deaths+=2;
        CHECK(t.score==4000);
        /* Fatal combat damage hands off to the recovered player death stage. */
        for(i=0;i<32;++i) player_step(&f,i);
        p->inventory.stat_c4=p->inventory.stat_c6=0;t.damage.hurt_flash=0;
        CHECK(ssa_combat_runtime_damage(&t.combat,&p->entity,&a->entity,NULL,1000,&t.effects)==0);++damage_hits;
        CHECK(!p->inventory.health && !p->entity_health && f.player.frame.timer==3000);
        {
            SsaPlayerGameplay gp={0};SsaPlayerGameplayOps ops={life_event_test,life_line_test,sector,life_animation_test,life_sound_test,&f};
            unsigned view=f.player.view.third_person;int32_t launch=f.player.jump.launch_velocity;
            CHECK(ssa_player_gameplay_init(&gp,&f.player,&f.plan)==0);
            f.input.keys=0;f.input.jump_down=1;
            for(i=0;i<32;++i) {
                CHECK(ssa_player_gameplay_step(&gp,&f.input,&ops)==0);
                CHECK(!p->inventory.health && !p->entity_health && (p->motion.movement.flags&0x10));
                CHECK(f.player.view.third_person==view && f.player.jump.launch_velocity==launch);++player_death_frames;
            }
            CHECK(p->animation==0x68 && gp.lifecycle.death_ticks==32);++player_deaths;
            CHECK(ssa_player_gameplay_free(&gp)==0);
        }
        for(i=0;i<2;++i) {
            t.enemies[i].runtime.controller.animation_done=1;
            f.spawn.actors[t.enemies[i].runtime.slot].animation_flags=1;
            CHECK(ctick(&t,i,240)==0);CHECK(ctick(&t,i,120)==0);CHECK(ctick(&t,i,1)==1);
        }
        CHECK(f.world.entity_count==1 && f.spawn.pool.active_count==1 && !f.level.script.alive[0]);
        CHECK(ssa_combat_runtime_free(&t.combat)==0);CHECK(ssa_enemy_queries_free(&t.queries)==0);cleanup(&f);
    }
    /* Sound failure retains armor/timer/flash but precedes health subtraction;
     * locks and borrowed source fields must still be restored. */
    {
        Fixture f;CombatCase t;SsaPlayerSpawn *p;SsaEntity *source;
        setup(&f,rom,(size_t)size,0,&math,&defs,100);script(&f,rom,thing,1,2);step(&f,1);step(&f,1);
        init_case(&t,&f,&math,speeds[0]);p=&f.players[0];source=t.enemies[0].runtime.navigation.entity;
        p->inventory.stat_c6=100;t.fail_sound=1;
        CHECK(ssa_combat_runtime_damage(&t.combat,&p->entity,source,NULL,30,&t.effects)==-1);++combat_failures;
        CHECK(p->inventory.health==100 && p->entity_health==100 && p->inventory.stat_c6==60);
        CHECK(f.player.frame.timer==30 && t.damage.hurt_flash==9 && !(p->entity.body.flags&1));
        CHECK(!t.combat.updating && !t.damage.updating && !f.player.updating && !f.spawn.updating && !t.damage.source);
        f.world.query_active=1;CHECK(ssa_combat_runtime_damage(&t.combat,source,NULL,NULL,1,&t.effects)==-1);f.world.query_active=0;++combat_guards;
        t.damage.player_count=2;CHECK(ssa_combat_runtime_damage(&t.combat,source,NULL,NULL,1,&t.effects)==-1);t.damage.player_count=1;++combat_guards;
        CHECK(ssa_combat_runtime_free(&t.combat)==0);CHECK(ssa_enemy_queries_free(&t.queries)==0);cleanup(&f);
    }
    /* Actual contact gate, then same-frame radial damage suppression by flag1. */
    {
        Fixture f;CombatCase t;SsaPlayerSpawn *p;SsaEnemyNavigation *nav;
        setup(&f,rom,(size_t)size,0,&math,&defs,100);script(&f,rom,thing,1,2);step(&f,1);step(&f,1);
        init_case(&t,&f,&math,speeds[0]);p=&f.players[0];nav=&t.enemies[0].runtime.navigation;
        /* Put the player first in this cell's collision list. Pool order stays
         * unchanged, so the radial query still visits player then actor. */
        CHECK(ssa_world_remove(&f.world,&p->entity)==1);
        CHECK(ssa_world_insert(&f.world,&p->entity)==1);
        nav->phase=2;nav->waypoint_x=(uint16_t)(p->entity.body.x/65536);
        nav->waypoint_y=(uint16_t)(p->entity.body.y/65536);
        CHECK(ctick(&t,0,1)==0);
        CHECK(t.enemies[0].runtime.controller.activation->state==2);
        CHECK(p->inventory.health==88 && p->entity_health==88 && t.damage.enemy_hits[0]==1);
        CHECK(t.hits==3 && t.score==2000);++damage_hits;
        CHECK(ssa_combat_runtime_free(&t.combat)==0);CHECK(ssa_enemy_queries_free(&t.queries)==0);cleanup(&f);
    }
    /* A failed explosion effect preserves death/group release already done,
     * but stops before damaging the later actor. */
    {
        Fixture f;CombatCase t;SsaScriptActor *a,*b;
        setup(&f,rom,(size_t)size,0,&math,&defs,100);script(&f,rom,thing,1,2);step(&f,1);step(&f,1);
        init_case(&t,&f,&math,speeds[0]);a=&f.spawn.actors[t.enemies[0].runtime.slot];
        b=&f.spawn.actors[t.enemies[1].runtime.slot];t.fail_sound=1;
        CHECK(ctick(&t,0,1)==-1);++combat_failures;
        CHECK(a->activation.state==2 && a->group_released && f.level.script.alive[0]==1);
        CHECK(b->activation.health==30 && !b->group_released && !t.score);
        CHECK(f.players[0].inventory.health==100 && f.players[0].entity_health==100);
        CHECK(ssa_combat_runtime_free(&t.combat)==0);CHECK(ssa_enemy_queries_free(&t.queries)==0);cleanup(&f);
    }
    extra_combat_cases(rom,(size_t)size,&math,&defs,thing,speeds[0]);
    CHECK(airborne && third_frames && combat_callbacks && chain_deaths==32);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"combat_controller_frames\":%u,\"direct_damage_hits\":%u,\"chain_deaths\":%u,\"rewards\":%u,\"player_deaths\":%u,\"death_frames\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"callbacks\":%u,\"guards\":%u,\"failures\":%u}\n",checks,combat_frames,damage_hits,chain_deaths,rewards,player_deaths,player_death_frames,player_frames,airborne,third_frames,combat_callbacks,combat_guards,combat_failures);
    return 0;
}
