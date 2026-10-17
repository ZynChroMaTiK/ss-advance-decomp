#define main script_spawn_fixture_main
#include "test_script_spawn.c"
#undef main
#include "ssa/enemy_control_runtime.h"
#include "ssa/actor_animation.h"
#include "ssa/native_game.h"

typedef struct BossCase {
    SsaEnemyControlRuntime runtime;
    SsaEnemyControlRuntimeOps ops;
    SsaEnemyCdFrame frame;
    unsigned slot,events,reentry,music,notices,fail;
} BossCase;
static int boss_event(void *ctx,const SsaEnemyControl *v,const SsaEnemyControlEvent *ev) {
    BossCase *c=ctx;
    CHECK(v==&c->runtime.controllers[c->slot]);
    CHECK(c->runtime.updating && c->runtime.combat->updating && c->runtime.combat->spawn->updating);
    CHECK(ssa_enemy_control_runtime_step(&c->runtime,c->slot,&c->frame,&c->ops)==-1);
    CHECK(ssa_enemy_control_runtime_free(&c->runtime)==-1);++c->events;++c->reentry;
    if(ev->kind==SSA_ENEMY_CONTROL_MUSIC)++c->music;
    else if(ev->kind==SSA_ENEMY_CONTROL_NOTICE)++c->notices;
    else CHECK(ev->kind==SSA_ENEMY_CONTROL_ANIMATION || ev->kind==SSA_ENEMY_CONTROL_SOUND);
    return c->fail?-1:0;
}
static int boss_combat(void *ctx,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
    SsaCombatDamageEvent ev,uint32_t x,uint32_t y) {
    BossCase *c=ctx;(void)v;(void)ev;(void)x;(void)y;CHECK(r==c->runtime.combat && r->updating);return 0;
}
static void animate(SsaScriptActor *a,const SsaActorAnimations *animations) {
    SsaActorAnimation v={a->animation,a->animation_display_frame,a->animation_frame,
        a->animation_counter,a->animation_ticks,a->animation_flags};
    CHECK(ssa_actor_animation_step(&v,animations,a->entity.body.flags,1)==1);
    a->animation_display_frame=v.frame;a->animation_frame=v.index;a->animation_counter=v.elapsed;
    a->animation_ticks=v.period;a->animation_flags=v.done;
}
int main(int argc,char **argv) {
    FILE *file;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;SsaActorAnimations animations;
    unsigned level,type,thing,i,j,frames=0,volleys=0,queues=0,death_cycles=0,events=0,reentry=0,effect_failures=0;
    uint32_t walks=0;unsigned native_frames=0,native_boss_frames=0;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);rom=malloc(0x800000);CHECK(rom);
    CHECK(fread(rom,1,0x800000,file)==0x800000);fclose(file);
    CHECK(ssa_math_tables_load(&math,rom,0x800000)==1);CHECK(ssa_spawn_definitions_load(&defs,rom,0x800000));
    CHECK(ssa_actor_animations_load(&animations,rom,0x800000)==1);
    for(level=0;level<16;++level)for(type=0xd7;type<=0xd8;++type){
        Fixture f;BossCase c={0};SsaCombatRuntime combat={0};SsaCombatDamage damage={0};
        SsaProjectileRuntime projectiles={0};SsaEnemyQueries queries={0};uint32_t score=0;
        SsaCombatRuntimeOps cops={boss_combat,&c};SsaEntity *reserved[1];SsaPlayerRuntime *players[1];SsaScriptActor *a;
        unsigned shots=0,script_id=0;
        setup(&f,rom,0x800000,level,&math,&defs,100);
        /* Borrow the actual boss script bank in this cross-map fixture.
         * Multiplayer/ordinary maps need not contain those script IDs. */
        f.level.data.table[level]=f.level.data.table[type==0xd7?5:11];
        f.level.data.count[level]=f.level.data.count[type==0xd7?5:11];
        reserved[0]=&f.players[0].entity;players[0]=&f.player;damage.player_count=1;damage.difficulty=1;
        CHECK(ssa_combat_runtime_init(&combat,&f.spawn,reserved,1,players,1,&damage,&score)==0);
        CHECK(ssa_projectile_runtime_init(&projectiles,&combat,&math)==0);
        CHECK(ssa_enemy_queries_init(&queries,&f.spawn,&math,reserved,1)==0);
        CHECK(ssa_enemy_control_runtime_init(&c.runtime,&combat,&projectiles,&queries,&math,&walks,rom,0x800000)==0);
        c.ops=(SsaEnemyControlRuntimeOps){&cops,boss_event,&c};c.frame=(SsaEnemyCdFrame){1,0x4000,0,0};
        for(thing=0;thing<64;++thing)if(defs.thing[thing].type==type && !defs.thing[thing].variant)break;
        CHECK(thing<64);script(&f,rom,thing,1,1);step(&f,1);c.slot=first_owned(&f);a=&f.spawn.actors[c.slot];
        for(i=0;i<100 && !a->activation.state;++i)CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
        CHECK(a->activation.state==1 && a->activation.health==3000 && f.spawn.game.boss==&a->entity);
        CHECK(c.music==1 && c.notices==1);
        /* Native script fixtures place a boss at each real-map player start.
         * Keep the group occupied until its genuine summon animation queues
         * a script. Flight/completion of the boss maps is a separate proof. */
        memset(f.level.script.alive,1,sizeof(f.level.script.alive));f.level.script.count=0;
        for(i=0;i<3000 && a->activation.state!=5;++i){
            CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);++frames;
            for(j=0;j<100;++j)if(projectiles.active[j]){
                CHECK(projectiles.projectiles[j].entity->type==0x6a);
                CHECK(projectiles.projectiles[j].source==&a->entity);
                CHECK(projectiles.projectiles[j].state==(type==0xd8));
                CHECK(f.spawn.actors[j].activation.state==projectiles.projectiles[j].state);
                CHECK(ssa_projectile_runtime_release(&projectiles,j)==0);++shots;++volleys;
            }
            animate(a,&animations);
        }
        CHECK(i<3000 && shots==5 && f.level.script.count==1);++queues;
        script_id=c.runtime.boss_rules.scripts[type-0xd7][0];
        if(script_id<=f.level.data.count[level]){
            const uint8_t *table=rom+f.level.data.table[level]-0x08000000;
            uint32_t expected=(uint32_t)table[(script_id-1)*4]|(uint32_t)table[(script_id-1)*4+1]<<8|
                (uint32_t)table[(script_id-1)*4+2]<<16|(uint32_t)table[(script_id-1)*4+3]<<24;
            CHECK(f.level.script.pc[0]==expected);
        }
        for(i=0;i<10;++i){CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);animate(a,&animations);}
        CHECK(a->activation.state==5 && f.spawn.game.boss_phase==0);
        f.level.script.alive[c.runtime.boss_rules.groups[type-0xd7][0]]=0;
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
        CHECK(a->activation.state==1 && a->activation.boss_ticks==5 && f.spawn.game.boss_phase==1);
        /* Real combat damage; hurt is accepted in the waiting state, and AI
         * performs ordered group release and selects post-boss music. */
        a->activation.state=5;
        CHECK(ssa_combat_runtime_damage(&combat,&a->entity,&f.players[0].entity,&f.players[0].entity,1,&cops)==0);
        c.fail=1;CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==-1);c.fail=0;
        CHECK(a->activation.state==6 && a->script.variant==6 && !c.runtime.updating && !combat.updating && !f.spawn.updating);
        ++effect_failures;
        for(i=0;i<100 && a->activation.state!=5;++i){animate(a,&animations);CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);}
        CHECK(i<100 && a->activation.state==5 && !(a->entity.body.flags&1));
        CHECK(ssa_combat_runtime_damage(&combat,&a->entity,&f.players[0].entity,&f.players[0].entity,10000,&cops)==0);
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0);
        CHECK(a->activation.state==7 && !a->activation.health && a->group_released && c.music==2);
        CHECK(ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops)==0 && !f.spawn.game.boss);
        for(i=0;i<361;++i){
            int result=ssa_enemy_control_runtime_step(&c.runtime,c.slot,&c.frame,&c.ops);
            CHECK(result>=0);if(result==1)break;animate(a,&animations);
        }
        CHECK(i<361 && !a->owned && !c.runtime.active[c.slot]);++death_cycles;
        events+=c.events;reentry+=c.reentry;
        CHECK(ssa_enemy_control_runtime_free(&c.runtime)==0);CHECK(ssa_projectile_runtime_free(&projectiles)==0);
        CHECK(ssa_enemy_queries_free(&queries)==0);CHECK(ssa_combat_runtime_free(&combat)==0);cleanup(&f);
    }
    /* Unmodified ROM map objects, actual outer dispatcher and full game frame.
     * Explicitly queue the encounter's original script as a test fixture;
     * idle input is a frame/render proof, not a boss-map completion route. */
    file=fopen(argv[1],"rb");CHECK(file);CHECK(fread(rom,1,0x800000,file)==0x800000);fclose(file);
    for(level=5;level<=11;level+=6){
        SsaNativeGame *g=calloc(1,sizeof(*g));unsigned seen=0;
        CHECK(g && ssa_native_game_init(g,rom,0x800000,level)==1);
        CHECK(ssa_level_script_queue(&g->level_runtime.script,&g->level_runtime.data,level,level==5?10:6)==1);
        for(i=0;i<1200;++i){
            if(ssa_native_game_tick(g)!=1){fprintf(stderr,"boss map%u frame%u error line%u\n",level,i,g->error_line);CHECK(0);}
            ++native_frames;
            if(g->spawn.game.boss){
                CHECK(g->hud.boss_present && g->hud.boss_health>0);++seen;++native_boss_frames;
            }
            if(i%120==0)CHECK(ssa_native_game_draw(g)==1);
        }
        CHECK(seen>0);ssa_native_game_free(g);free(g);
    }
    free(rom);printf("{\"result\":\"pass\",\"assertions\":%u,\"maps\":16,\"boss_types\":2,\"controller_frames\":%u,\"projectiles\":%u,\"script_queues\":%u,\"death_cycles\":%u,\"events\":%u,\"reentry_guards\":%u,\"effect_failures\":%u,\"native_map_frames\":%u,\"native_boss_hud_frames\":%u}\n",
        checks,frames,volleys,queues,death_cycles,events,reentry,effect_failures,native_frames,native_boss_frames);return 0;
}
