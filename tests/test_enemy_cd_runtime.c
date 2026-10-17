/* Reuse established real-map/player/script fixtures without running their main. */
#define main script_spawn_fixture_main
#include "test_script_spawn.c"
#undef main
#include "ssa/enemy_cd_runtime.h"

static unsigned runtime_frames, chase_frames, chase_moves, detonations, released;
static unsigned visibility_calls, path_calls, ray_calls, contact_events, explosion_events;
static unsigned starts, stops, runtime_events, runtime_guards, runtime_failures, runtime_reentry;
static unsigned total_walks, slot_reuses;
typedef struct CdCase {
    Fixture *f;
    SsaEnemyCdRuntime runtime;
    SsaEnemyCdGame game;
    SsaEnemyCdFrame frame;
    SsaEnemyNavigationOps queries;
    SsaEnemyCdRuntimeOps ops;
    SsaEnemyPath path;
    uint32_t walks;
    unsigned fail_query, blocked, fail_event, check_busy;
} CdCase;
static void busy(CdCase *c) {
    CHECK(c->runtime.updating && c->f->spawn.updating);
    CHECK(ssa_enemy_cd_runtime_step(&c->runtime,&c->frame,&c->ops)==-1);
    CHECK(ssa_script_spawn_release(&c->f->spawn,c->runtime.slot)==-1);
    CHECK(ssa_script_spawn_defeat(&c->f->spawn,c->runtime.slot)==-1);
    CHECK(ssa_script_spawn_step(&c->f->spawn,1,0,&c->f->ops,&c->f->movers)==-1);
    ++runtime_reentry;
}
static int visible(void *context,int player,int enemy,int *blocked) {
    CdCase *c=context;CHECK(player>=0 && enemy>=0);++visibility_calls;
    if(c->check_busy) busy(c);
    *blocked=(int)c->blocked;return c->fail_query?-1:0;
}
static int nearest(void *context,SsaEntity *source,SsaEntity **out) {
    CdCase *c=context;CHECK(source);*out=NULL;
    if(c->check_busy) busy(c);
    return 0;
}
static int ray(void *context,const SsaEnemyPath *path,int32_t x0,int32_t y0,
                 int32_t x1,int32_t y1,int *clear) {
    CdCase *c=context;CHECK(path==&c->path && path->updating);
    (void)x0;(void)y0;(void)x1;(void)y1;*clear=1;++ray_calls;return 0;
}
static int path(void *context,const SsaEnemyNavigation *nav,int32_t x0,int32_t y0,
                  int32_t x1,int32_t y1,int32_t radius,unsigned special,unsigned mode,
                  int *mask,int32_t *dx,int32_t *dy) {
    CdCase *c=context;CHECK(nav==&c->runtime.navigation);++path_calls;
    if(c->check_busy) busy(c);
    CHECK(ssa_enemy_path_query(&c->path,c->runtime.math,x0,y0,x1,y1,radius,special,mode,ray,c,mask)==1);
    *dx=c->path.offset_x;*dy=c->path.offset_y;return 0;
}
static int cd_event(void *context,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,
                      SsaEntity *other,int32_t a,int32_t b,int32_t d) {
    CdCase *c=context;SsaScriptActor *actor=&c->f->spawn.actors[c->runtime.slot];
    CHECK(cd==&c->runtime.controller && actor->owned);++runtime_events;
    CHECK(kind!=SSA_ENEMY_CD_GROUP_RELEASE && kind!=SSA_ENEMY_CD_DESTROY);
    if(c->check_busy) busy(c);
    if(kind==SSA_ENEMY_CD_ANIMATION) {
        CHECK(actor->animation==(unsigned)a && actor->animation_ticks==(unsigned)b);
        CHECK(!actor->animation_frame && !actor->animation_counter && !actor->animation_flags);
    } else if(kind==SSA_ENEMY_CD_SOUND) {
        if(a==0x3a) {CHECK(c->game.loop_source==&actor->entity);++starts;}
        else CHECK(a==0x27);
    } else if(kind==SSA_ENEMY_CD_STOP_SOUND) {CHECK(a==0x3a && !c->game.loop_source);++stops;}
    else if(kind==SSA_ENEMY_CD_CONTACT_DAMAGE) {CHECK(other==c->game.player && a==20);++contact_events;}
    else if(kind==SSA_ENEMY_CD_EXPLOSION) {
        CHECK(a==0x20000 && b==30 && d==1 && !actor->activation.health);
        CHECK(actor->group_released && c->f->level.script.alive[0]==0);++explosion_events;
    } else CHECK(0);
    return c->fail_event==(unsigned)kind+1?-1:0;
}
static void bind(CdCase *c,Fixture *f,const SsaMathTables *tables,int32_t speed) {
    memset(c,0,sizeof *c);c->f=f;c->game.activation=&f->spawn.game;
    c->game.player=&f->players[0].entity;c->game.player_x=f->players[0].motion.x;c->game.player_y=f->players[0].motion.y;
    c->queries=(SsaEnemyNavigationOps){visible,nearest,path,NULL,NULL,NULL,c};
    c->ops=(SsaEnemyCdRuntimeOps){&c->queries,cd_event,c};
    c->frame=(SsaEnemyCdFrame){1,speed,0,0};c->check_busy=1;
    CHECK(ssa_enemy_cd_runtime_init(&c->runtime,&f->spawn,first_owned(f),tables,&c->game,&c->walks)==0);
}
static int tick(CdCase *c,unsigned ticks) {
    int result;c->frame.ticks=ticks;++c->frame.frame_counter;
    c->game.player_x=c->f->players[0].motion.x;c->game.player_y=c->f->players[0].motion.y;
    result=ssa_enemy_cd_runtime_step(&c->runtime,&c->frame,&c->ops);
    CHECK(!c->runtime.updating && !c->f->spawn.updating && !c->runtime.controller.updating && !c->runtime.navigation.updating);
    ++runtime_frames;return result;
}
static void activate(CdCase *c) {
    SsaScriptActor *a=&c->f->spawn.actors[c->runtime.slot];
    uint32_t old_count=c->f->spawn.game.spawned[0];
    CHECK(tick(c,59)==0 && a->activation.timer==1 && a->activation.state==0);
    CHECK(tick(c,1)==0 && a->activation.timer==30 && !(a->entity.body.flags&0x80));
    CHECK(tick(c,29)==0 && a->activation.timer==1 && a->activation.state==0);
    CHECK(tick(c,1)==0 && a->activation.state==1 && a->activation.health==30);
    CHECK(a->entity.radius==32*65536 && a->entity.body.height==120*65536);
    CHECK(a->script.variant==1 && a->script.timer==0 && c->f->spawn.game.spawned[0]==old_count+1);
}
static void separate(CdCase *c) {
    static const int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    SsaEntity *e=c->runtime.navigation.entity;unsigned i,j;int moved=0;
    int32_t x=e->body.x,y=e->body.y;
    for(i=0;i<5 && !moved;++i) for(j=0;j<8 && !moved;++j) {
        int dist=256+(int)i*64;
        moved=ssa_world_try_move(&c->f->world,e,x+dirs[j][0]*dist*65536,y+dirs[j][1]*dist*65536);
        CHECK(moved>=0);
    }
    CHECK(moved==1);
}
static void expire(CdCase *c) {
    SsaScriptActor *a=&c->f->spawn.actors[c->runtime.slot];unsigned slot=c->runtime.slot;
    CHECK(a->activation.state==2 && a->activation.timer==360 && a->group_released);
    CHECK(!c->f->level.script.alive[0]);++detonations;
    a->animation_flags=1;
    CHECK(tick(c,240)==0 && a->activation.timer==120 && !(a->entity.body.flags&0x40));
    CHECK(tick(c,1)==0 && a->activation.timer==119 && (a->entity.body.flags&0x40));
    CHECK(tick(c,119)==0 && a->activation.timer==0 && a->owned);
    CHECK(tick(c,1)==1 && !c->runtime.active && !a->owned);
    CHECK(!c->f->spawn.pool.occupied[slot] && c->f->world.entity_count==1 && c->f->spawn.pool.active_count==1);
    CHECK(!c->game.loop_source && !a->entity.owner && !a->entity.body.x && !a->activation.entity);++released;
    CHECK(tick(c,1)==-1);++runtime_guards;
}
#ifndef SSA_CD_TEST_MAIN
#define SSA_CD_TEST_MAIN main
#endif
int SSA_CD_TEST_MAIN(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables tables;SsaSpawnDefinitions defs;
    int32_t speeds[12];unsigned level,thing=100,i;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    for(i=0;i<64;++i) if(defs.thing[i].type==0xcd && !defs.thing[i].variant) {thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;CdCase c;SsaScriptActor *a;unsigned prior_moves=chase_moves;
        setup(&f,rom,(size_t)size,level,&tables,&defs,100);script(&f,rom,thing,1,1);step(&f,1);
        bind(&c,&f,&tables,speeds[0]);a=&f.spawn.actors[c.runtime.slot];activate(&c);separate(&c);
        CHECK(ssa_enemy_cd_runtime_init(&c.runtime,&f.spawn,c.runtime.slot,&tables,&c.game,&c.walks)==-1);++runtime_guards;
        f.world.query_active=1;CHECK(tick(&c,1)==-1);f.world.query_active=0;++runtime_guards;
        c.frame.ticks=256;CHECK(ssa_enemy_cd_runtime_step(&c.runtime,&c.frame,&c.ops)==-1);++runtime_guards;
        for(i=0;i<128;++i) {
            int32_t x=a->entity.body.x,y=a->entity.body.y;
            player_step(&f,i);
            if(a->activation.state==1) {
                CHECK(tick(&c,1)==0);++chase_frames;
                if(x!=a->entity.body.x || y!=a->entity.body.y) ++chase_moves;
            }
            CHECK(a->owned && a->entity.owner==&f.world);
        }
        CHECK(chase_moves>prior_moves);
        if(a->activation.state==1) {
            /* A hit/drain-controlled death exercises the native same-frame
             * detonation branch; no fake combat callback changes the actor. */
            a->parameter34=1;a->parameter36=0;a->activation.health=1;
            CHECK(tick(&c,1)==0);
        }
        expire(&c);
        /* Waiting script observes the real death notification and advances. */
        step(&f,1);step(&f,1);step(&f,1);CHECK(!f.level.script.count);
        total_walks+=c.walks;
        {
            unsigned old_slot=c.runtime.slot;
            script(&f,rom,thing,1,1);step(&f,1);
            CHECK(first_owned(&f)==old_slot);++slot_reuses;
            /* Inactive binding may be initialized for the recycled slot. */
            CHECK(ssa_enemy_cd_runtime_init(&c.runtime,&f.spawn,old_slot,&tables,&c.game,&c.walks)==0);
            CHECK(c.runtime.navigation.phase==0 && !c.runtime.navigation.target && !c.runtime.controller.destroy_requested);
            c.walks=0;activate(&c);CHECK(tick(&c,1)==0);expire(&c);total_walks+=c.walks;
        }
        cleanup(&f);
    }
    /* Near-target detonation, actual player blocker/contact, live moving-sector
     * hazard, callback failures, and destruction with a failed sound postlude. */
    for(i=0;i<6;++i) {
        Fixture f;CdCase c;SsaScriptActor *a;
        setup(&f,rom,(size_t)size,0,&tables,&defs,100);script(&f,rom,thing,1,1);step(&f,1);
        bind(&c,&f,&tables,speeds[0]);a=&f.spawn.actors[c.runtime.slot];activate(&c);
        if(i==0) {
            c.runtime.navigation.phase=2;c.runtime.navigation.waypoint_x=(uint16_t)(a->entity.body.x/65536);
            c.runtime.navigation.waypoint_y=(uint16_t)(a->entity.body.y/65536);
            a->activation.facing=64;CHECK(tick(&c,1)==0);expire(&c);
        } else if(i==1) {
            separate(&c);c.fail_query=1;CHECK(tick(&c,1)==-1);++runtime_failures;
            CHECK(a->owned && a->activation.state==1 && f.level.script.alive[0]==1);
            c.fail_query=0;c.blocked=1;f.level.movers.hazard_count=1;
            f.level.movers.hazard_sectors[0]=(uint16_t)a->entity.sector;c.frame.frame_counter=15;
            CHECK(tick(&c,1)==0 && a->activation.health==0);
            CHECK(a->activation.state==1);a->entity.body.flags|=1;CHECK(tick(&c,1)==0);expire(&c);
        } else if(i==2) {
            c.fail_event=SSA_ENEMY_CD_ANIMATION+1;CHECK(tick(&c,1)==-1);++runtime_failures;
            CHECK(a->activation.state==2 && !a->group_released && f.level.script.alive[0]==1);
            CHECK(a->animation==0x1c && a->animation_ticks==6);
        } else if(i==3) {
            CHECK(tick(&c,1)==0);a->activation.timer=0;c.game.loop_source=&a->entity;
            c.fail_event=SSA_ENEMY_CD_STOP_SOUND+1;CHECK(tick(&c,1)==-1);++runtime_failures;
            CHECK(!a->owned && !c.runtime.active && !c.game.loop_source && f.world.entity_count==1);++released;
        } else if(i==4) {
            c.fail_event=SSA_ENEMY_CD_EXPLOSION+1;CHECK(tick(&c,1)==-1);++runtime_failures;
            CHECK(a->group_released && !f.level.script.alive[0] && a->activation.timer==360);
            c.fail_event=0;expire(&c);
        } else {
            unsigned old_contacts=contact_events;
            /* Another world query updates the original global blocker. A sight
             * early exit must still use it, even without a navigation move. */
            c.blocked=1;c.runtime.navigation.phase=2;c.runtime.navigation.move_result=0;
            CHECK(!c.game.blocking_entity);
            CHECK(ssa_world_query(&f.world,&a->entity,a->entity.body.x,a->entity.body.y)==0);
            CHECK(f.world.blocking_entity==&f.players[0].entity);
            CHECK(tick(&c,1)==0 && c.walks==0);
            CHECK(contact_events==old_contacts+1 && c.game.blocking_entity==&f.players[0].entity);
            expire(&c);
        }
        total_walks+=c.walks;
        cleanup(&f);
    }
    CHECK(starts && stops && contact_events && airborne && third_frames && released>=19);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"runtime_frames\":%u,\"chase_frames\":%u,\"chase_moves\":%u,\"walk_calls\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"detonations\":%u,\"released\":%u,\"visibility_calls\":%u,\"path_calls\":%u,\"ray_calls\":%u,\"events\":%u,\"loop_starts\":%u,\"loop_stops\":%u,\"contacts\":%u,\"explosions\":%u,\"guards\":%u,\"failures\":%u,\"callback_reentry\":%u,\"slot_reuses\":%u}\n",
        checks,runtime_frames,chase_frames,chase_moves,total_walks,player_frames,airborne,third_frames,detonations,released,
        visibility_calls,path_calls,ray_calls,runtime_events,starts,stops,contact_events,explosion_events,runtime_guards,runtime_failures,runtime_reentry,slot_reuses);
    return 0;
}
