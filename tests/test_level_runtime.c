#include "ssa/level_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, frames, airborne, third_frames, mover_cases, switch_cases;
static unsigned sector_cases, hazards, failures, reentries, spawn_cases, line_cases;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"failed %s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } ++checks; } while (0)
static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
static void put16(uint8_t *p,unsigned n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void put32(uint8_t *p,uint32_t n) { put16(p,n); put16(p+2,n>>16); }
enum { SCRIPT=0x7f0000, THINGS=0x7f1000 };
typedef struct Fixture {
    SsaMapAssets map;
    SsaLevelRuntime level;
    SsaSpawnPlan plan;
    SsaWorld world;
    SsaSlide slide;
    SsaPlayerSpawn players[4];
    SsaPlayerRuntime runtime;
    SsaPlayerGameplay gameplay;
    SsaPlayerRuntimeInput input;
    SsaLevelPlayerOps player_ops;
    SsaLevelScriptOps script_ops;
    SsaSectorMoverOps mover_ops;
    SsaLevelScriptEntity spawned;
    unsigned script_calls[4], notices, sounds;
    int fail, allocation_failure;
} Fixture;

static void guard(Fixture *f) {
    CHECK(f->level.updating);
    CHECK(ssa_level_runtime_free(&f->level)==-1);
    CHECK(ssa_level_runtime_sector(&f->level,0)==-1);
    CHECK(ssa_level_runtime_line(&f->level,1,0,&f->mover_ops)==-1);
    CHECK(ssa_level_runtime_step(&f->level,1,0,&f->script_ops,&f->mover_ops)==-1);
    CHECK(ssa_level_runtime_player_step(&f->level,&f->gameplay,&f->input,&f->player_ops)==-1);
    ++reentries;
}
static int event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent e,
                 int32_t a,int32_t b,int32_t c,int32_t d) {
    Fixture *f=context; (void)a;(void)b;(void)c;(void)d;
    CHECK(g==&f->gameplay); guard(f);
    if(e==SSA_LIFECYCLE_NOTICE) ++f->notices;
    return f->fail==1 ? -1 : 0;
}
static int animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    Fixture *f=context; CHECK(r==&f->runtime && id && ticks); guard(f);
    return f->fail==2 ? -1 : 0;
}
static int player_sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    Fixture *f=context; CHECK(r==&f->runtime && id); guard(f); ++f->sounds;
    return f->fail==3 ? -1 : 0;
}
static int mover_sound(void *context,unsigned id) {
    Fixture *f=context; CHECK(id==41 || id==103 || id==104); guard(f); ++f->sounds;
    return f->fail==4 ? -1 : 0;
}
static SsaLevelScriptEntity *allocate(void *context) {
    Fixture *f=context; guard(f); ++f->script_calls[0];
    memset(&f->spawned,0,sizeof f->spawned);
    return f->allocation_failure ? NULL : &f->spawned;
}
static int spawn_animation(void *context,SsaLevelScriptEntity *e,unsigned id,unsigned ticks) {
    Fixture *f=context; CHECK(e==&f->spawned && id<=65535 && ticks<=65535); guard(f); ++f->script_calls[1];
    return f->fail==5 ? -1 : 0;
}
static int insert(void *context,SsaLevelScriptEntity *e) {
    Fixture *f=context; CHECK(e==&f->spawned); guard(f); ++f->script_calls[2];
    e->floor=23;
    return f->fail==6 ? -1 : 0;
}
static int particle(void *context,unsigned id,int32_t x,int32_t y,int32_t z) {
    Fixture *f=context; CHECK(id==5); (void)x; (void)y; (void)z; guard(f); ++f->script_calls[3];
    return f->fail==7 ? -1 : 0;
}
static int forbidden_mover(void *context,unsigned op,uint32_t index) {
    (void)context; (void)op; (void)index; CHECK(0); return -1;
}
static void setup(Fixture *f,uint8_t *rom,size_t size,unsigned level,
                  const SsaMathTables *tables,const SsaSpawnDefinitions *defs) {
    memset(f,0,sizeof *f);
    CHECK(ssa_map_assets_load(&f->map,rom,size,level));
    CHECK(ssa_level_runtime_init(&f->level,&f->map,rom,size,level)==0);
    CHECK(f->level.map==&f->map && !f->level.script.count);
    CHECK(ssa_spawn_plan_load(&f->plan,&f->map,defs,level,1,100));
    CHECK(ssa_world_init(&f->world,&f->map,104)); CHECK(ssa_slide_init(&f->slide,&f->map));
    CHECK(ssa_players_spawn(f->players,&f->world,&f->plan,1,0)); CHECK(f->players[0].active);
    CHECK(ssa_player_runtime_init(&f->runtime,&f->players[0],&f->world,&f->slide,tables)==0);
    CHECK(ssa_player_gameplay_init(&f->gameplay,&f->runtime,&f->plan)==0);
    f->input.motion.player_count=1; f->input.motion.ticks=1; f->input.controls_enabled=1;
    f->player_ops=(SsaLevelPlayerOps){event,animation,player_sound,f};
    f->script_ops=(SsaLevelScriptOps){allocate,spawn_animation,insert,particle,forbidden_mover,f};
    f->mover_ops=(SsaSectorMoverOps){mover_sound,f};
}
static void cleanup(Fixture *f) {
    CHECK(ssa_player_gameplay_free(&f->gameplay)==0);
    CHECK(ssa_player_runtime_free(&f->runtime)==0);
    ssa_world_free(&f->world); ssa_slide_free(&f->slide); CHECK(ssa_spawn_plan_free(&f->plan));
    CHECK(ssa_level_runtime_free(&f->level)==0); CHECK(ssa_level_runtime_free(&f->level)==0);
    ssa_map_assets_free(&f->map);
}
static void player_frame(Fixture *f) {
    SsaPlayerSpawn *p=&f->players[0]; int32_t sentinel=-1;
    f->gameplay.hazard_game.hazard_sectors=&sentinel; f->gameplay.hazard_game.hazard_count=1;
    CHECK(ssa_level_runtime_player_step(&f->level,&f->gameplay,&f->input,&f->player_ops)==0);
    CHECK(f->gameplay.hazard_game.hazard_sectors==&sentinel && f->gameplay.hazard_game.hazard_count==1);
    f->gameplay.hazard_game.hazard_sectors=NULL; f->gameplay.hazard_game.hazard_count=0;
    CHECK(!f->level.updating && !f->gameplay.updating && !f->runtime.updating);
    CHECK(p->entity.owner==&f->world && f->world.entity_count==1);
    CHECK(p->inventory.health==p->entity_health && p->vertical.flags==p->motion.movement.flags);
    CHECK(p->entity.body.x==p->motion.x && p->entity.body.y==p->motion.y && p->entity.body.z==p->motion.z);
    CHECK(f->world.cells[p->entity.cell]==&p->entity && f->world.sectors[p->entity.sector]==&p->entity);
    if(p->motion.z>(int32_t)p->entity.body.floor*65536) ++airborne;
    if(f->runtime.view.third_person) ++third_frames;
    ++frames;
}
static void world_frame(Fixture *f) {
    CHECK(ssa_level_runtime_step(&f->level,1,123456,&f->script_ops,&f->mover_ops)==0);
    CHECK(!f->level.updating && !f->level.movers.updating && !f->level.script.updating);
}
static void instruction(Fixture *f,uint8_t *rom,unsigned opcode,unsigned arg) {
    CHECK(ssa_level_script_reset(&f->level.script));
    memset(rom+SCRIPT,0,80); put32(rom+SCRIPT,opcode); put32(rom+SCRIPT+8,arg);
    put32(rom+SCRIPT+20,10);
    f->level.script.pc[0]=0x08000000u+SCRIPT; f->level.script.count=1;
}
static unsigned door(Fixture *f) {
    unsigned id=(unsigned)f->players[0].entity.sector; uint8_t *r=f->level.movers.records[0];
    uint8_t *sec=f->map.data+f->map.sectors+20*id;
    f->level.movers.count=1; f->level.movers.hazard_count=0; memset(r,0,20);
    put16(r,id); put16(r+12,4); put16(r+14,0);
    put16(sec+2,u16(sec));
    return id;
}
static void query_height(Fixture *f,unsigned ceiling) {
    SsaMapCollision c={0}; SsaPlayerSpawn *p=&f->players[0];
    CHECK(ssa_map_collision_init(&c,&f->map));
    CHECK(ssa_map_query(&c,p->motion.x,p->motion.y,p->motion.x,p->motion.y,0)==1);
    CHECK((unsigned)(c.ceiling&65535)==ceiling);
    ssa_map_collision_free(&c);
}
int main(int argc,char **argv) {
    FILE *file; long length; uint8_t *rom; SsaMathTables tables; SsaSpawnDefinitions defs;
    unsigned level,i;
    CHECK(argc==2); file=fopen(argv[1],"rb"); CHECK(file);
    CHECK(fseek(file,0,SEEK_END)==0); length=ftell(file); CHECK(length>0); rewind(file);
    rom=malloc((size_t)length); CHECK(rom); CHECK(fread(rom,1,(size_t)length,file)==(size_t)length); fclose(file);
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)length)); CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)length));
    for(level=0;level<16;++level) {
        Fixture f; setup(&f,rom,(size_t)length,level,&tables,&defs);
        for(i=0;i<256;++i) {
            if(i==32 || i==192) { CHECK(ssa_player_runtime_key(&f.runtime,'H',1,0,1)); CHECK(!ssa_player_runtime_key(&f.runtime,'H',0,0,1)); }
            f.input.jump_down=(uint8_t)(i%48<4); f.input.yaw_delta=(i&1)?97:-97;
            player_frame(&f); world_frame(&f);
            CHECK(f.level.script.visited[f.players[0].entity.sector]);
        }
        /* A real switch routes to the actual queue once, then stays latched. */
        if(f.level.switches.count && f.level.data.count[level]) {
            SsaSwitchEntry *e=&f.level.switches.entries[0]; unsigned line=e->line,side;
            CHECK(ssa_level_script_reset(&f.level.script)); e->flags&=(uint16_t)~1u; e->trigger=1;
            CHECK(ssa_level_runtime_line(&f.level,2,line,&f.mover_ops)==0);
            CHECK(f.level.script.count==1 && (e->flags&1));
            side=u16(f.map.data+f.map.lines+40*line+8);
            CHECK(u16(f.map.data+f.map.sides+12*side+6)==e->on_texture);
            CHECK(ssa_level_runtime_line(&f.level,2,line,&f.mover_ops)==0 && f.level.script.count==1);
            /* Full queue retains the already-applied switch latch and texture. */
            CHECK(ssa_level_script_reset(&f.level.script)); f.level.script.count=5; e->flags&=(uint16_t)~1u;
            CHECK(ssa_level_runtime_line(&f.level,2,line,&f.mover_ops)==-1);
            CHECK((e->flags&1) && f.level.script.count==5 && !f.level.updating);
            CHECK(u16(f.map.data+f.map.sides+12*side+6)==e->on_texture); ++failures;
            /* Audio failure occurs after the successful queue. */
            CHECK(ssa_level_script_reset(&f.level.script)); e->flags&=(uint16_t)~1u; f.fail=4;
            CHECK(ssa_level_runtime_line(&f.level,2,line,&f.mover_ops)==-1);
            CHECK((e->flags&1) && f.level.script.count==1 && !f.level.updating); ++failures; f.fail=0;
            ++switch_cases;
        }
        /* Native visited bitmap suppresses a repeated sector queue. */
        if(f.level.data.count[level]) {
            unsigned id=(unsigned)f.players[0].entity.sector;
            CHECK(ssa_level_script_reset(&f.level.script)); put16(f.map.data+f.map.sectors+20*id+16,0x101);
            CHECK(ssa_level_runtime_sector(&f.level,id)==0 && f.level.script.count==1);
            CHECK(ssa_level_runtime_sector(&f.level,id)==0 && f.level.script.count==1);
            CHECK(ssa_level_script_reset(&f.level.script)); f.level.script.count=5;
            CHECK(ssa_level_runtime_sector(&f.level,id)==-1 && !f.level.script.visited[id]);
            CHECK(f.level.script.count==5 && !f.level.updating); ++failures;
            ++sector_cases;
        }
        cleanup(&f);
    }
    for(level=0;level<16;++level) {
        Fixture f; unsigned sector,floor; uint8_t *sec,*r;
        setup(&f,rom,(size_t)length,level,&tables,&defs); sector=door(&f);
        sec=f.map.data+f.map.sectors+20*sector; r=f.level.movers.records[0]; floor=u16(sec);
        for(i=0;i<f.map.line_count;++i) {
            const uint8_t *line=f.map.data+f.map.lines+40*i;
            if(u16(line+12)==sector || u16(line+14)==sector) {
                put16(r+14,4); CHECK(ssa_level_runtime_line(&f.level,1,i,&f.mover_ops)==0);
                CHECK(u16(r+14)==4);
                put16(r+14,0); CHECK(ssa_level_runtime_line(&f.level,1,i,&f.mover_ops)==0);
                CHECK(u16(r+14)==9); put16(r+14,0); ++line_cases; break;
            }
        }
        instruction(&f,rom,4,0); query_height(&f,floor);
        world_frame(&f); CHECK(u16(sec+2)==floor && (u16(r+14)&9)==9); query_height(&f,floor);
        world_frame(&f); CHECK(u16(sec+2)==((floor+4)&65535)); query_height(&f,(floor+4)&65535);
        instruction(&f,rom,7,0); world_frame(&f); CHECK(u16(r+14)&4);
        instruction(&f,rom,6,0); world_frame(&f); CHECK(!(u16(r+14)&4));
        put16(sec+2,floor+224); put16(r+14,1);
        instruction(&f,rom,5,0); world_frame(&f);
        CHECK(f.level.movers.hazard_count==1 && f.level.movers.hazard_sectors[0]==sector);
        CHECK(!(u16(r+14)&1) && u16(sec+2)==((floor+224)&65535));
        world_frame(&f); CHECK(u16(sec+2)==((floor+220)&65535)); query_height(&f,(floor+220)&65535);
        ++mover_cases;
        /* Closing mover hazard reaches actual player death on the next stage. */
        f.gameplay.hazard_game.frame_counter=0;
        CHECK(ssa_player_runtime_key(&f.runtime,'H',1,0,1));
        player_frame(&f); CHECK(f.players[0].inventory.health==0 && f.runtime.view.third_person);
        player_frame(&f); CHECK(f.players[0].motion.movement.flags&0x10); CHECK(f.notices==1); ++hazards;
        cleanup(&f);
    }
    /* Script-spawn callback chain and its failure boundaries use bounded data
       in a private ROM copy. It is not an implementation of host entity AI. */
    for(i=0;i<5;++i) {
        Fixture f; setup(&f,rom,(size_t)length,0,&tables,&defs);
        f.level.movers.count=0;
        instruction(&f,rom,1,3); put32(rom+SCRIPT+4,0x08000000u+THINGS);
        memset(rom+THINGS,0,16); put16(rom+THINGS,12); put16(rom+THINGS+2,34); put16(rom+THINGS+4,1);
        if(i==1) f.allocation_failure=1;
        if(i>=2) f.fail=(int)i+3;
        CHECK(ssa_level_runtime_step(&f.level,1,123456,&f.script_ops,&f.mover_ops)==(i>=2?-1:0));
        CHECK(!f.level.updating && !f.level.script.updating && !f.level.movers.updating);
        CHECK(f.script_calls[0]==1);
        if(i==0) {
            CHECK(f.script_calls[1]==1 && f.script_calls[2]==1 && f.script_calls[3]==1);
            CHECK(f.spawned.x==12*65536 && f.spawned.y==34*65536 && f.spawned.z==23*65536);
            CHECK(f.spawned.trigger==3 && f.level.script.alive[2]==1); ++spawn_cases;
        } else if(i==1) { CHECK(!f.script_calls[1] && !f.script_calls[2] && f.script_calls[3]==1); ++spawn_cases; }
        else { CHECK(f.level.script.pc[0]==0x08000000u+SCRIPT); ++failures; }
        cleanup(&f);
    }
    for(i=1;i<=4;++i) {
        Fixture f; int32_t sentinel=-1; setup(&f,rom,(size_t)length,0,&tables,&defs); f.fail=(int)i;
        if(i==4) {
            unsigned id=door(&f); uint8_t *sec=f.map.data+f.map.sectors+20*id;
            instruction(&f,rom,4,0);
            CHECK(ssa_level_runtime_step(&f.level,1,0,&f.script_ops,&f.mover_ops)==-1);
            CHECK((u16(f.level.movers.records[0]+14)&9)==9 && u16(sec+2)==u16(sec));
        } else {
            f.players[0].inventory.health=0;
            f.gameplay.hazard_game.hazard_sectors=&sentinel; f.gameplay.hazard_game.hazard_count=1;
            CHECK(ssa_level_runtime_player_step(&f.level,&f.gameplay,&f.input,&f.player_ops)==-1);
            CHECK(f.gameplay.hazard_game.hazard_sectors==&sentinel && f.gameplay.hazard_game.hazard_count==1);
        }
        CHECK(!f.level.updating && !f.level.movers.updating && !f.level.script.updating);
        ++failures; cleanup(&f);
    }
    {
        Fixture f; SsaLevelPlayerOps bad; SsaLevelScriptOps bad_script;
        setup(&f,rom,(size_t)length,0,&tables,&defs);
        CHECK(ssa_level_runtime_init(&f.level,&f.map,rom,(size_t)length,0)==-1);
        CHECK(ssa_level_runtime_sector(&f.level,f.map.sector_count)==-1);
        CHECK(ssa_level_runtime_line(&f.level,3,0,NULL)==-1);
        CHECK(ssa_level_runtime_line(&f.level,1,f.map.line_count,NULL)==-1);
        CHECK(ssa_level_runtime_step(&f.level,256,0,&f.script_ops,NULL)==-1);
        CHECK(ssa_level_runtime_step(&f.level,1,0,NULL,NULL)==-1);
        bad_script=f.script_ops; bad_script.allocate=NULL;
        CHECK(ssa_level_runtime_step(&f.level,1,0,&bad_script,NULL)==-1);
        bad=f.player_ops; bad.event=NULL;
        CHECK(ssa_level_runtime_player_step(&f.level,&f.gameplay,&f.input,&bad)==-1);
        CHECK(ssa_level_runtime_player_step(&f.level,&f.gameplay,NULL,&f.player_ops)==-1);
        CHECK(ssa_level_runtime_free(NULL)==-1);
        cleanup(&f);
    }
    CHECK(airborne>0 && third_frames>0 && hazards==16 && mover_cases==16 && line_cases==16);
    printf("{\"passed\":true,\"assertions\":%u,\"player_frames\":%u,\"airborne_frames\":%u,\"third_person_frames\":%u,\"real_maps\":16,\"mover_cases\":%u,\"line_activation_cases\":%u,\"switch_cases\":%u,\"sector_cases\":%u,\"closing_door_deaths\":%u,\"spawn_callback_cases\":%u,\"failure_cases\":%u,\"reentry_callbacks\":%u}\n",
       checks,frames,airborne,third_frames,mover_cases,line_cases,switch_cases,sector_cases,hazards,spawn_cases,failures,reentries);
    free(rom); return 0;
}
