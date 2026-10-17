#include "ssa/script_spawn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, scripts, script_frames, player_frames, airborne, third_frames;
static unsigned particles, initialized, defeats, guards, failures, reentries, exhausted, reused;
static unsigned moved_frames, moved_with_actor_frames, blocked_queries, relocated_actors;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"failed %s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } ++checks; } while (0)
enum { SCRIPT=0x7f0000, THINGS=0x7f1000 };
static void put16(uint8_t *p,unsigned x) { p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8); }
static void put32(uint8_t *p,uint32_t x) { put16(p,x);put16(p+2,x>>16); }
typedef struct Fixture {
    SsaMapAssets map;
    SsaLevelRuntime level;
    SsaSpawnPlan plan;
    SsaWorld world;
    SsaSlide slide;
    SsaPlayerSpawn players[4];
    SsaPlayerRuntime player;
    SsaPlayerRuntimeInput input;
    SsaPlayerRuntimeOps player_ops;
    SsaScriptSpawn spawn;
    SsaScriptSpawnOps ops;
    SsaSectorMoverOps movers;
    unsigned fail_particle, fail_event, expect_fallback, check_reentry;
    int32_t fallback;
} Fixture;
static unsigned first_owned(const Fixture *f) {
    unsigned i; for(i=0;i<100;++i) if(f->spawn.actors[i].owned) return i;
    return 100;
}
static unsigned owned(const Fixture *f) {
    unsigned i,n=0;for(i=0;i<100;++i) n+=!!f->spawn.actors[i].owned;
    return n;
}
static void busy_guards(Fixture *f) {
    unsigned slot=first_owned(f);
    CHECK(f->spawn.updating);
    CHECK(ssa_script_spawn_free(&f->spawn)==-1);
    CHECK(ssa_script_spawn_init(&f->spawn,&f->level,&f->world)==-1);
    CHECK(ssa_script_spawn_step(&f->spawn,1,0,&f->ops,&f->movers)==-1);
    CHECK(ssa_script_spawn_activate(&f->spawn,slot,1,&f->ops)==-1);
    CHECK(ssa_script_spawn_defeat(&f->spawn,slot)==-1);
    CHECK(ssa_script_spawn_release(&f->spawn,slot)==-1);
    ++reentries;
}
static int particle(void *ctx,const SsaScriptSpawn *s,unsigned id,int32_t x,int32_t y,int32_t z) {
    Fixture *f=ctx;unsigned i,found=0;
    CHECK(s==&f->spawn && id==5); ++particles;
    if(f->expect_fallback) { CHECK(z==f->fallback); ++exhausted; }
    else {
        for(i=0;i<100;++i) {
            const SsaScriptActor *a=&s->actors[i];
            if(!a->owned || a->script.x!=x || a->script.y!=y) continue;
            CHECK(a->entity.owner==&f->world);
            CHECK(a->script.z==a->entity.body.z);
            CHECK(a->script.z==(int32_t)(int16_t)a->script.floor*65536);
            CHECK(a->entity.body.floor==(int16_t)a->script.floor);
            if(a->script.z==z) found=1;
        }
        CHECK(found);
    }
    if(f->check_reentry) busy_guards(f);
    return f->fail_particle ? -1 : 0;
}
static int event(void *ctx,const SsaScriptSpawn *s,unsigned slot,SsaEnemyActivationEvent e,
                 int32_t a,int32_t b,int32_t c,int32_t d) {
    Fixture *f=ctx; CHECK(s==&f->spawn && slot<100 && s->actors[slot].owned);
    CHECK(e>=SSA_ENEMY_ACTIVATION_SOUND && e<=SSA_ENEMY_ACTIVATION_NOTICE);
    if(e==SSA_ENEMY_ACTIVATION_ANIMATION) {
        CHECK(s->actors[slot].animation==(unsigned)a);
        CHECK(s->actors[slot].animation_ticks==(unsigned)b);
    }
    (void)c;(void)d;
    if(f->check_reentry) busy_guards(f);
    return f->fail_event ? -1 : 0;
}
static int mover(void *ctx,unsigned id) {
    Fixture *f=ctx; CHECK(id==41 || id==103 || id==104);
    if(f->check_reentry) busy_guards(f);
    return 0;
}
static int gameplay(void *ctx,SsaPlayerRuntime *r,enum SsaPlayerFrameStage stage) {
    Fixture *f=ctx;CHECK(r==&f->player);
    CHECK(stage==SSA_PLAYER_FRAME_DEATH || stage==SSA_PLAYER_FRAME_HAZARDS);return 0;
}
static int sector(void *ctx,SsaPlayerRuntime *r,int16_t id) {
    Fixture *f=ctx;CHECK(r==&f->player && id==r->player->entity.sector);return 0;
}
static int animation(void *ctx,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    Fixture *f=ctx;CHECK(r==&f->player && (id==0x65 || id==0x66) && ticks==4);return 0;
}
static int sound(void *ctx,SsaPlayerRuntime *r,unsigned id) {
    Fixture *f=ctx;CHECK(r==&f->player && (id==13 || id==14));return 0;
}
static void setup(Fixture *f,uint8_t *rom,size_t size,unsigned level,
                  const SsaMathTables *tables,const SsaSpawnDefinitions *defs,unsigned capacity) {
    memset(f,0,sizeof *f);
    CHECK(ssa_map_assets_load(&f->map,rom,size,level));
    CHECK(ssa_level_runtime_init(&f->level,&f->map,rom,size,level)==0);
    CHECK(ssa_spawn_plan_load(&f->plan,&f->map,defs,level,1,100));
    CHECK(ssa_world_init(&f->world,&f->map,capacity));CHECK(ssa_slide_init(&f->slide,&f->map));
    CHECK(ssa_players_spawn(f->players,&f->world,&f->plan,1,0));CHECK(f->players[0].active);
    CHECK(ssa_player_runtime_init(&f->player,&f->players[0],&f->world,&f->slide,tables)==0);
    CHECK(ssa_script_spawn_init(&f->spawn,&f->level,&f->world)==0);
    CHECK(f->spawn.reserved==1 && f->spawn.pool.active_count==1);
    f->ops=(SsaScriptSpawnOps){particle,event,f};f->movers=(SsaSectorMoverOps){mover,f};
    f->player_ops=(SsaPlayerRuntimeOps){gameplay,sector,animation,sound,f};
    f->input.motion.ticks=1;f->input.motion.player_count=1;f->input.controls_enabled=1;
    f->check_reentry=1;
}
static void release_all(Fixture *f,int notify) {
    unsigned i;
    for(i=0;i<100;++i) if(f->spawn.actors[i].owned) {
        if(notify && !f->spawn.actors[i].group_released) {
            CHECK(ssa_script_spawn_defeat(&f->spawn,i)==0);++defeats;
        }
        CHECK(ssa_script_spawn_release(&f->spawn,i)==0);
    }
    CHECK(f->world.entity_count==1 && f->spawn.pool.active_count==1);
}
static void cleanup(Fixture *f) {
    CHECK(ssa_script_spawn_free(&f->spawn)==0);CHECK(f->world.entity_count==1);
    CHECK(ssa_player_runtime_free(&f->player)==0);ssa_world_free(&f->world);ssa_slide_free(&f->slide);
    CHECK(ssa_spawn_plan_free(&f->plan));CHECK(ssa_level_runtime_free(&f->level)==0);ssa_map_assets_free(&f->map);
}
static void script(Fixture *f,uint8_t *rom,unsigned thing,unsigned group,unsigned count) {
    unsigned i; CHECK(ssa_level_script_reset(&f->level.script));
    memset(rom+SCRIPT,0,80);memset(rom+THINGS,0,8*(count+1));
    put32(rom+SCRIPT,1);put32(rom+SCRIPT+4,0x08000000u+THINGS);put32(rom+SCRIPT+8,group);
    put32(rom+SCRIPT+20,2);put32(rom+SCRIPT+28,group?group:1);put32(rom+SCRIPT+40,10);
    for(i=0;i<count;++i) {
        put16(rom+THINGS+8*i,(uint32_t)f->players[0].motion.x>>16);
        put16(rom+THINGS+8*i+2,(uint32_t)f->players[0].motion.y>>16);
        put16(rom+THINGS+8*i+4,thing);put16(rom+THINGS+8*i+6,0x1200+i);
    }
    f->level.script.pc[0]=0x08000000u+SCRIPT;f->level.script.count=1;
}
static void step(Fixture *f,unsigned ticks) {
    CHECK(ssa_script_spawn_step(&f->spawn,ticks,f->fallback,&f->ops,&f->movers)==0);
    CHECK(!f->spawn.updating && !f->level.updating && !f->level.script.updating);++script_frames;
}
static void player_step(Fixture *f,unsigned frame) {
    int32_t old_x=f->players[0].motion.x,old_y=f->players[0].motion.y;
    if(frame==16 || frame==112) {
        CHECK(ssa_player_runtime_key(&f->player,'H',1,0,1));
        CHECK(!ssa_player_runtime_key(&f->player,'H',0,0,1));
    }
    f->input.jump_down=(uint8_t)(frame%48<3);f->input.yaw_delta=(frame&1)?97:-97;
    f->input.keys=frame%64<16?0x40:frame%64<32?0x200:frame%64<48?0x80:0x100;
    CHECK(ssa_player_runtime_step(&f->player,&f->input,&f->player_ops)==0);
    CHECK(f->players[0].entity.owner==&f->world);
    CHECK(f->players[0].motion.z==f->players[0].entity.body.z);
    if(f->players[0].motion.z>(int32_t)f->players[0].entity.body.floor*65536) ++airborne;
    if(f->player.view.third_person) ++third_frames;
    if(f->players[0].motion.x!=old_x || f->players[0].motion.y!=old_y) {
        ++moved_frames;
        if(owned(f)) ++moved_with_actor_frames;
    }
    ++player_frames;
}
static void relocate_actor(Fixture *f,SsaScriptActor *a) {
    static const int direction[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    static const int distance[4]={96,128,192,256};
    unsigned i,j;int moved=0;int32_t x=a->entity.body.x,y=a->entity.body.y;
    for(i=0;i<4 && !moved;++i) for(j=0;j<8 && !moved;++j) {
        int result=ssa_world_try_move(&f->world,&a->entity,
            x+direction[j][0]*distance[i]*65536,y+direction[j][1]*distance[i]*65536);
        CHECK(result>=0);moved=result==1;
    }
    CHECK(moved && a->entity.owner==&f->world);
    CHECK(ssa_entity_contact(&f->players[0].entity,&a->entity,
                             f->players[0].motion.x,f->players[0].motion.y)==1);
    ++relocated_actors;
}
int main(int argc,char **argv) {
    FILE *file;long length;uint8_t *rom;SsaMathTables tables;SsaSpawnDefinitions defs;
    unsigned level,i,j,thing[12]={0};
    static const int height[12]={120,140,152,90,140,170,210,180,66,138,186,186};
    static const int radius[12]={32,64,48,25,50,70,48,56,32,90,70,70};
    static const int health[12]={30,100,50,40,100,80,100,130,60,150,3000,3000};
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(fseek(file,0,SEEK_END)==0);
    length=ftell(file);CHECK(length>0);rewind(file);rom=malloc((size_t)length);CHECK(rom);
    CHECK(fread(rom,1,(size_t)length,file)==(size_t)length);CHECK(fclose(file)==0);
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)length));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)length));
    for(i=0;i<64;++i) if(defs.thing[i].type>=0xcd && defs.thing[i].type<=0xd8) thing[defs.thing[i].type-0xcd]=i;
    for(i=0;i<12;++i) CHECK(defs.thing[thing[i]].type==0xcd+i);
    for(level=0;level<16;++level) {
        Fixture f;setup(&f,rom,(size_t)length,level,&tables,&defs,100);
        for(j=0;j<12;++j) {
            unsigned slot;SsaScriptActor *a;
            script(&f,rom,thing[j],1,1);step(&f,1);slot=first_owned(&f);CHECK(slot<100);a=&f.spawn.actors[slot];
            CHECK(owned(&f)==1 && f.world.entity_count==2 && f.level.script.alive[0]==1);
            CHECK(a->entity.type==0xcd+j && a->script.type==a->entity.type);
            CHECK(a->entity.body.height==0 && a->entity.radius==0 && a->activation.health==0);
            CHECK(a->activation.timer==60 && (a->entity.body.flags&0x80));
            CHECK(a->animation==defs.thing[thing[j]].animation);
            CHECK(a->animation_ticks==(uint8_t)defs.thing[thing[j]].animation_ticks);
            CHECK(!a->animation_frame && !a->animation_counter && !a->animation_flags);
            if(j==0) {
                CHECK(ssa_entity_contact(&f.players[0].entity,&a->entity,a->entity.body.x+48*65536,a->entity.body.y)==1);
                CHECK(ssa_world_query(&f.world,&f.players[0].entity,a->entity.body.x,a->entity.body.y)==0);
                CHECK(f.world.blocking_entity==&a->entity);++blocked_queries;
            }
            CHECK(ssa_script_spawn_activate(&f.spawn,slot,59,&f.ops)==0);CHECK(a->activation.timer==1);
            CHECK(ssa_script_spawn_activate(&f.spawn,slot,1,&f.ops)==0);
            CHECK(a->activation.timer==30 && !(a->entity.body.flags&0x80) && a->activation.state==0);
            CHECK(ssa_script_spawn_activate(&f.spawn,slot,29,&f.ops)==0);CHECK(a->activation.timer==1);
            CHECK(ssa_script_spawn_activate(&f.spawn,slot,1,&f.ops)==1);
            CHECK(a->activation.state==1 && a->activation.health==health[j]);
            CHECK(a->entity.radius==radius[j]*65536 && a->entity.body.height==height[j]*65536);
            CHECK((a->entity.body.flags&2) && f.spawn.game.spawned[j]==1);++initialized;
            CHECK(!a->animation_frame && !a->animation_counter && !a->animation_flags);
            if(j==0) {
                CHECK(ssa_entity_contact(&f.players[0].entity,&a->entity,a->entity.body.x+48*65536,a->entity.body.y)==0);
                CHECK(ssa_world_query(&f.world,&f.players[0].entity,a->entity.body.x,a->entity.body.y)==0);
                CHECK(f.world.blocking_entity==&a->entity);++blocked_queries;
            }
            CHECK(ssa_script_spawn_activate(&f.spawn,slot,1,&f.ops)==-1);++guards;
            step(&f,1);CHECK(f.level.script.pc[0]==0x08000000u+SCRIPT+20);
            step(&f,1);CHECK(f.level.script.pc[0]==0x08000000u+SCRIPT+20);
            if(j==0) {
                unsigned moved_before=moved_with_actor_frames;
                relocate_actor(&f,a);
                for(i=0;i<128;++i) player_step(&f,i);
                CHECK(moved_with_actor_frames>moved_before);
                CHECK(a->entity.owner==&f.world && owned(&f)==1);
            }
            CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==0);++defeats;
            CHECK(!f.level.script.alive[0] && a->entity.owner==&f.world && a->group_released);
            CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==-1);++guards;
            step(&f,1);CHECK(f.level.script.pc[0]==0x08000000u+SCRIPT+40);step(&f,1);CHECK(!f.level.script.count);
            CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);CHECK(!owned(&f) && f.world.entity_count==1);
            CHECK(!f.spawn.game.boss);
            if(j==0) for(i=128;i<192;++i) player_step(&f,i);
            CHECK(ssa_script_spawn_release(&f.spawn,slot)==-1);++guards;
        }
        /* Every original entry runs on its actual map with real native insertions.
         * Release is explicitly test-driven; enemy combat/AI is not simulated. */
        for(j=1;j<=f.level.data.count[level];++j) {
            CHECK(ssa_level_script_reset(&f.level.script));f.level.script.game_state=0;
            CHECK(ssa_level_script_queue(&f.level.script,&f.level.data,level,j));
            for(i=0;i<4096;++i) {
                step(&f,255);release_all(&f,1);
                if(!f.level.script.count || f.level.script.game_state==3) break;
            }
            CHECK(i<4096);++scripts;
        }
        /* Reentry and live-query guards retain ownership and byte payload. */
        f.world.query_active=1;CHECK(ssa_script_spawn_free(&f.spawn)==-1);
        CHECK(ssa_script_spawn_step(&f.spawn,1,0,&f.ops,&f.movers)==-1);f.world.query_active=0;guards+=2;
        f.level.updating=1;CHECK(ssa_script_spawn_free(&f.spawn)==-1);f.level.updating=0;++guards;
        CHECK(ssa_script_spawn_step(&f.spawn,256,0,&f.ops,&f.movers)==-1);
        CHECK(ssa_script_spawn_step(&f.spawn,1,0,NULL,&f.movers)==-1);
        CHECK(ssa_script_spawn_release(&f.spawn,0)==-1);guards+=3;
        /* Particle failure keeps the inserted and counted entity until release/free. */
        script(&f,rom,thing[0],1,1);f.fail_particle=1;
        CHECK(ssa_script_spawn_step(&f.spawn,1,0,&f.ops,&f.movers)==-1);
        CHECK(owned(&f)==1 && f.world.entity_count==2 && f.level.script.alive[0]==1);
        CHECK(!f.spawn.updating && !f.level.updating && !f.level.script.updating);++failures;
        cleanup(&f);
    }
    /* All 99 available slots, normal exhaustion, free-list reuse and group accounting. */
    {
        Fixture f;unsigned slot;setup(&f,rom,(size_t)length,0,&tables,&defs,100);
        script(&f,rom,thing[0],1,100);f.check_reentry=0;
        for(i=0;i<99;++i) step(&f,1);
        CHECK(owned(&f)==99 && f.spawn.pool.active_count==100 && f.world.entity_count==100);
        f.expect_fallback=1;f.fallback=1234567;step(&f,1);
        CHECK(f.level.script.alive[0]==99 && owned(&f)==99);
        slot=f.spawn.pool.active_slots[49];CHECK(f.spawn.actors[slot].owned);
        f.spawn.actors[slot].animation_counter=11;f.spawn.actors[slot].animation_frame=9;
        f.spawn.actors[slot].animation_flags=7;f.spawn.actors[slot].activation.health=41;
        CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==0);CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);
        { SsaScriptActor zero={0};CHECK(memcmp(&zero,&f.spawn.actors[slot],sizeof zero)==0); }
        CHECK(f.level.script.alive[0]==98);f.expect_fallback=0;
        f.level.script.state[0][1]=0x08000000u+THINGS;step(&f,1);
        CHECK(f.spawn.actors[slot].owned && f.level.script.alive[0]==99);++reused;
        CHECK(!f.spawn.actors[slot].activation.health && !f.spawn.actors[slot].group_released);
        CHECK(!f.spawn.actors[slot].animation_counter && !f.spawn.actors[slot].animation_frame && !f.spawn.actors[slot].animation_flags);
        release_all(&f,1);CHECK(!f.level.script.alive[0]);cleanup(&f);
    }
    /* World capacity exhaustion is also normal and preserves explicit stale Z. */
    {
        Fixture f;setup(&f,rom,(size_t)length,0,&tables,&defs,1);
        script(&f,rom,thing[0],1,1);f.expect_fallback=1;f.fallback=-987654;step(&f,1);
        CHECK(!owned(&f) && !f.level.script.alive[0]);cleanup(&f);
    }
    /* Activation effects persist after a presentation error; destruction itself
     * must not manufacture a script defeat notification. */
    for(j=1;j<12;j+=(j==1?9:1)) {
        Fixture f;unsigned slot;SsaScriptActor *a;
        setup(&f,rom,(size_t)length,0,&tables,&defs,100);
        script(&f,rom,thing[j],1,1);step(&f,1);slot=first_owned(&f);a=&f.spawn.actors[slot];
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,60,NULL)==-1);
        CHECK(a->activation.timer==60 && a->activation.state==0);++guards;
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,60,&f.ops)==0);f.fail_event=1;
        CHECK(ssa_script_spawn_activate(&f.spawn,slot,30,&f.ops)==-1);
        CHECK(a->activation.state==1 && a->activation.health==health[j]);
        CHECK(a->script.variant==1 && a->script.timer==a->activation.timer);
        CHECK(!f.spawn.updating && !a->activation.updating && f.spawn.game.spawned[j]==1);++failures;
        CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);CHECK(f.level.script.alive[0]==1);
        CHECK(!f.spawn.game.boss);cleanup(&f);
    }
    /* Original group zero changes its explicit preceding byte, but release
     * ignores tag zero; bit100 groups never count. */
    for(j=0;j<2;++j) {
        Fixture f;unsigned slot;
        setup(&f,rom,(size_t)length,0,&tables,&defs,100);
        script(&f,rom,thing[0],j?0x101:0,1);step(&f,1);slot=first_owned(&f);
        CHECK(!f.level.script.alive[0] && f.level.script.alive_before==(j?0:1));
        CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==0);
        CHECK(!f.level.script.alive[0] && f.level.script.alive_before==(j?0:1));
        cleanup(&f);
    }
    /* Fault-inject an invalid leaf sector only for world insertion, then
     * restore the map immediately. The allocated unlinked actor remains owned
     * and can be explicitly released; no group count was incremented. */
    {
        Fixture f;unsigned slot;uint8_t *leaf;uint8_t saved[2];
        setup(&f,rom,(size_t)length,0,&tables,&defs,100);script(&f,rom,thing[0],1,1);
        leaf=f.map.data+f.map.leaves+8*(unsigned)f.players[0].entity.leaf+4;
        memcpy(saved,leaf,2);put16(leaf,0xffff);
        CHECK(ssa_script_spawn_step(&f.spawn,1,0,&f.ops,&f.movers)==-1);
        memcpy(leaf,saved,2);slot=first_owned(&f);CHECK(slot<100);
        CHECK(!f.spawn.actors[slot].entity.owner && f.world.entity_count==1);
        CHECK(f.spawn.pool.active_count==2 && !f.level.script.alive[0]);
        CHECK(!f.spawn.updating && !f.level.updating && !f.level.script.updating);++failures;
        CHECK(ssa_script_spawn_defeat(&f.spawn,slot)==-1);++guards;
        CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);CHECK(f.spawn.pool.active_count==1);cleanup(&f);
    }
    free(rom);
    CHECK(scripts==116 && initialized==192 && player_frames==3072 && airborne && third_frames && moved_with_actor_frames);
    printf("{\"passed\":true,\"assertions\":%u,\"maps\":16,\"real_scripts\":%u,"
           "\"script_frames\":%u,\"player_frames\":%u,\"airborne_frames\":%u,\"third_person_frames\":%u,"
           "\"particles\":%u,\"enemy_initializations\":%u,\"defeats\":%u,\"guards\":%u,"
           "\"failure_cases\":%u,\"reentry_checks\":%u,\"exhaustion_cases\":%u,\"reused_slots\":%u,"
           "\"moved_frames\":%u,\"moved_with_actor_frames\":%u,\"blocked_queries\":%u,\"relocated_actors\":%u}\n",
           checks,scripts,script_frames,player_frames,airborne,third_frames,particles,initialized,defeats,guards,
           failures,reentries,exhausted,reused,moved_frames,moved_with_actor_frames,blocked_queries,relocated_actors);
    return 0;
}
