#include "ssa/player_gameplay.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, frames, death_frames, respawns, hazard_cases, error_cases;
static unsigned reentries, line_events, third_frames, animations, sounds;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed at %s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } ++checks; } while (0)

typedef struct Fixture {
    SsaMapAssets map;
    SsaSpawnPlan plan;
    SsaWorld world;
    SsaSlide slide;
    SsaPlayerSpawn players[4];
    SsaPlayerRuntime runtime;
    SsaPlayerGameplay gameplay;
    SsaPlayerRuntimeInput input;
    SsaPlayerGameplayOps ops;
    SsaEntity peer;
    const SsaEntity *peers[2];
    SsaSpawnMarker marker;
    SsaSpawnMarker *saved_markers;
    size_t saved_count;
    unsigned notices, effects, weapon_animations, sectors;
    int fail;
} Fixture;

static void callback_guard(Fixture *f) {
    CHECK(f->gameplay.updating && f->runtime.updating);
    CHECK(ssa_player_gameplay_step(&f->gameplay,&f->input,&f->ops)==-1);
    CHECK(ssa_player_gameplay_free(&f->gameplay)==-1);
    CHECK(ssa_player_runtime_free(&f->runtime)==-1);
    ++reentries;
}
static int event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent kind,
                 int32_t a,int32_t b,int32_t c,int32_t d) {
    Fixture *f=context;
    CHECK(g==&f->gameplay); callback_guard(f);
    if(kind==SSA_LIFECYCLE_NOTICE) {
        CHECK((a==0xb5 || a==0xb4) && b==0 && (c==0x34 || c==0x40) && d==0);
        ++f->notices; if(f->fail==1) return -1;
    } else if(kind==SSA_LIFECYCLE_EFFECT) {
        CHECK(a==5 && b==f->runtime.player->motion.x && c==f->runtime.player->motion.y);
        CHECK(!f->runtime.player->entity.owner && f->world.entity_count==0);
        ++f->effects; if(f->fail==2) return -1;
    } else if(kind==SSA_LIFECYCLE_WEAPON_ANIMATION) {
        CHECK(a==0 && b==3 && c==0 && d==0);
        CHECK(f->runtime.player->entity.owner==&f->world && f->world.entity_count==1);
        ++f->weapon_animations; if(f->fail==3) return -1;
    } else CHECK(kind==SSA_LIFECYCLE_MODE && a==10);
    return 0;
}
static int line(void *context,SsaPlayerGameplay *g,unsigned kind,uint16_t id) {
    Fixture *f=context;
    CHECK(g==&f->gameplay && (kind==1 || kind==2) && id<f->map.line_count);
    callback_guard(f); ++line_events;
    return f->fail==7 ? -1 : 0;
}
static int sector(void *context,SsaPlayerRuntime *r,int16_t id) {
    Fixture *f=context;
    CHECK(r==&f->runtime && id==r->player->entity.sector);
    callback_guard(f); ++f->sectors;
    if(f->fail==4) { r->player->inventory.health=29; return -1; }
    return 0;
}
static int animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    Fixture *f=context;
    CHECK(r==&f->runtime && (id==0x65 || id==0x66 || id==0x68));
    CHECK(ticks==(id==0x68?8u:4u));
    CHECK(r->player->animation==id && r->player->animation_ticks==ticks);
    callback_guard(f); ++animations;
    return f->fail==5 ? -1 : 0;
}
static int sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    Fixture *f=context;
    CHECK(r==&f->runtime && (id==13 || id==14 || id==17));
    callback_guard(f); ++sounds;
    return f->fail==6 ? -1 : 0;
}
static void setup(Fixture *f,const uint8_t *rom,size_t size,unsigned level,
                  const SsaMathTables *tables,const SsaSpawnDefinitions *defs) {
    SsaPlayerSpawn *p;
    memset(f,0,sizeof(*f));
    CHECK(ssa_map_assets_load(&f->map,rom,size,level));
    CHECK(ssa_spawn_plan_load(&f->plan,&f->map,defs,level,1,100));
    CHECK(ssa_world_init(&f->world,&f->map,104));
    CHECK(ssa_slide_init(&f->slide,&f->map));
    CHECK(ssa_players_spawn(f->players,&f->world,&f->plan,1,0));
    p=&f->players[0]; CHECK(p->active);
    CHECK(ssa_player_runtime_init(&f->runtime,p,&f->world,&f->slide,tables)==0);
    CHECK(ssa_player_gameplay_init(&f->gameplay,&f->runtime,&f->plan)==0);
    CHECK(f->gameplay.interaction.visited!=f->world.collision.visited);
    CHECK(f->gameplay.interaction.visited!=f->runtime.view_collision.visited);
    CHECK(f->gameplay.interaction.special_lines!=f->world.collision.special_lines);
    f->marker.x=(int16_t)(p->motion.x/65536); f->marker.y=(int16_t)(p->motion.y/65536);
    f->marker.z=p->entity.body.floor; f->marker.heading=p->motion.facing;
    f->saved_markers=f->plan.player_starts; f->saved_count=f->plan.player_count;
    f->plan.player_starts=&f->marker; f->plan.player_count=1;
    f->peer.body.x=p->motion.x+1000*65536; f->peer.body.y=p->motion.y;
    f->peers[0]=&p->entity; f->peers[1]=&f->peer;
    f->input.motion.players=f->peers; f->input.motion.player_count=2;
    f->input.motion.ticks=1; f->input.controls_enabled=1;
    f->ops=(SsaPlayerGameplayOps){event,line,sector,animation,sound,f};
}
static void cleanup(Fixture *f) {
    CHECK(ssa_player_gameplay_free(&f->gameplay)==0);
    CHECK(!f->gameplay.runtime && !f->gameplay.interaction.map);
    CHECK(ssa_player_gameplay_free(&f->gameplay)==0);
    CHECK(ssa_player_runtime_free(&f->runtime)==0);
    f->plan.player_starts=f->saved_markers; f->plan.player_count=f->saved_count;
    ssa_world_free(&f->world); ssa_slide_free(&f->slide);
    CHECK(ssa_spawn_plan_free(&f->plan)); ssa_map_assets_free(&f->map);
}
static void run(Fixture *f) {
    SsaPlayerSpawn *p=f->runtime.player;
    CHECK(ssa_player_gameplay_step(&f->gameplay,&f->input,&f->ops)==0);
    CHECK(!f->gameplay.updating && !f->gameplay.lifecycle.updating && !f->runtime.updating);
    CHECK(p->entity.owner==&f->world && f->world.entity_count==1);
    CHECK(p->inventory.health==p->entity_health);
    CHECK(p->vertical.flags==p->motion.movement.flags);
    CHECK(p->entity.body.x==p->motion.x && p->entity.body.y==p->motion.y);
    CHECK(p->entity.body.z==p->motion.z);
    CHECK(f->world.cells[p->entity.cell]==&p->entity);
    CHECK(f->world.sectors[p->entity.sector]==&p->entity);
    CHECK(!p->entity.block_previous && !p->entity.block_next);
    CHECK(!p->entity.sector_previous && !p->entity.sector_next);
    if(p->motion.movement.flags&0x10) ++death_frames;
    if(f->runtime.view.third_person) ++third_frames;
    ++frames;
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) { uint16_t v=u16(p); return v<=32767?(int16_t)v:(int16_t)(-1-(int)(65535-v)); }

int main(int argc,char **argv) {
    FILE *file; long size; uint8_t *rom; SsaMathTables tables; SsaSpawnDefinitions defs;
    unsigned level,view,i;
    CHECK(argc==2); file=fopen(argv[1],"rb"); CHECK(file);
    CHECK(fseek(file,0,SEEK_END)==0); size=ftell(file); CHECK(size>0); rewind(file);
    rom=malloc((size_t)size); CHECK(rom); CHECK(fread(rom,1,(size_t)size,file)==(size_t)size); fclose(file);
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)size));
    CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    for(level=0;level<16;++level) for(view=0;view<2;++view) {
        Fixture f; SsaPlayerSpawn *p; int32_t x,y; int16_t ammo[10]; uint32_t pending[10];
        setup(&f,rom,(size_t)size,level,&tables,&defs); p=f.runtime.player;
        if(view) CHECK(ssa_player_runtime_key(&f.runtime,'H',1,0,1));
        f.runtime.jump.launch_velocity=7*65536;
        p->inventory.weapons=0x3ff; p->inventory.pending_count=3;
        p->inventory.stat_c4=71; p->inventory.stat_c6=82;
        for(i=0;i<10;++i) { ammo[i]=(int16_t)(i*13+9); pending[i]=i+1; }
        memcpy(p->inventory.ammo,ammo,sizeof(ammo)); memcpy(p->inventory.pending_weapons,pending,sizeof(pending));
        f.gameplay.weapon.current=7; f.gameplay.weapon.requested=8;
        p->inventory.health=0; f.input.jump_down=1; f.input.keys=0x40;
        x=p->motion.x; y=p->motion.y;
        for(i=0;i<300;++i) {
            run(&f);
            CHECK(p->motion.movement.flags&0x10);
            CHECK(p->inventory.health==0 && p->animation==0x68);
            CHECK(p->motion.x==x && p->motion.y==y && p->motion.velocity_z<=0);
            CHECK(f.runtime.jump.was_down && f.runtime.view.third_person==view);
            CHECK(f.gameplay.lifecycle.death_ticks==i+1);
        }
        /* Recovered camera flag4 clamps collapse to24 units, even though the
         * death routine separately tests4 before setting body flag0x40. */
        CHECK(p->vertical.eye_offset==24*65536);
        CHECK(f.notices==1 && f.effects==0 && f.weapon_animations==0);
        run(&f); ++respawns;
        CHECK(!(p->motion.movement.flags&0x10) && p->inventory.health==100);
        CHECK(p->inventory.stat_c4==0 && p->inventory.stat_c6==0);
        CHECK(p->inventory.weapons==1 && p->inventory.pending_count==0 && p->inventory.ammo[0]==999);
        CHECK(memcmp(p->inventory.ammo+1,ammo+1,9*sizeof(ammo[0]))==0);
        CHECK(memcmp(p->inventory.pending_weapons,pending,sizeof(pending))==0);
        CHECK(f.gameplay.weapon.current==1 && f.gameplay.weapon.requested==1 && f.gameplay.weapon.flags==0);
        CHECK(f.gameplay.weapon_view.flags==2 && f.gameplay.weapon_view.kind==14 && f.gameplay.weapon_view.player_number==1);
        CHECK(f.gameplay.weapon_view.x==0x7800 && f.gameplay.weapon_view.y==0xa000);
        CHECK(f.effects==1 && f.weapon_animations==1 && f.gameplay.lifecycle.death_ticks==0);
        CHECK(f.runtime.jump.was_down && f.runtime.jump.launch_velocity==7*65536);
        CHECK(f.runtime.view.third_person==view && p->motion.velocity_z<=0);
        f.input.keys=0;
        for(i=0;i<8;++i) { run(&f); CHECK(p->motion.velocity_z<=0); }
        f.input.jump_down=0; run(&f);
        f.input.jump_down=1; run(&f);
        if(p->entity.body.ceiling-p->entity.body.floor>100) CHECK(p->motion.velocity_z>0);
        /* Native sector sampling kills in the hazard stage. Death is seen on
         * the following frame; its jump lock then takes effect. */
        {
            int32_t hazard=p->entity.sector;
            p->inventory.stat_c4=51; p->inventory.stat_c6=61;
            f.gameplay.hazard_game.hazard_sectors=&hazard;
            f.gameplay.hazard_game.hazard_count=1;
            f.gameplay.hazard_game.frame_counter=0;
            run(&f);
            CHECK(p->inventory.health==0 && p->entity_health==0);
            CHECK(p->inventory.stat_c4==0 && p->inventory.stat_c6==0);
            CHECK(!(p->motion.movement.flags&0x10));
            f.gameplay.hazard_game.hazard_count=0;
            run(&f); CHECK(p->motion.movement.flags&0x10); ++hazard_cases;
        }
        cleanup(&f);
    }
    /* Independent fixtures keep partially applied error states honest. */
    for(i=1;i<=6;++i) {
        Fixture f; SsaViewPose pose; SsaPlayerSpawn *p;
        setup(&f,rom,(size_t)size,0,&tables,&defs); p=f.runtime.player;
        f.fail=(int)i; p->inventory.health=0;
        if(i==2 || i==3) { p->motion.movement.flags|=0x10; f.gameplay.lifecycle.death_ticks=300; }
        if(i==4) p->inventory.health=100;
        pose=f.runtime.pose;
        CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&f.ops)==-1);
        CHECK(!f.gameplay.updating && !f.gameplay.lifecycle.updating && !f.runtime.updating);
        CHECK(memcmp(&pose,&f.runtime.pose,sizeof(pose))==0);
        CHECK(p->inventory.health==p->entity_health);
        CHECK((p->entity.owner==NULL)==(i==2));
        if(i==4) CHECK(p->inventory.health==29);
        ++error_cases; cleanup(&f);
    }
    {
        Fixture f; SsaPlayerGameplayOps broken; SsaPlayerSpawn *p;
        setup(&f,rom,(size_t)size,0,&tables,&defs); p=f.runtime.player;
        CHECK(ssa_player_gameplay_init(&f.gameplay,&f.runtime,&f.plan)==-1); ++error_cases;
        broken=f.ops; broken.line=NULL;
        CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&broken)==-1); ++error_cases;
        CHECK(p->inventory.health==100 && f.sectors==0);
        f.input.motion.player_count=5;
        CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&f.ops)==-1); ++error_cases;
        f.input.motion.player_count=2;
        f.gameplay.hazard_game.hazard_count=256;
        CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&f.ops)==-1); ++error_cases;
        CHECK(!f.gameplay.updating && !f.runtime.updating); f.gameplay.hazard_game.hazard_count=0;
        p->inventory.health=0; p->motion.movement.flags|=0x10;
        f.gameplay.lifecycle.death_ticks=300; f.plan.player_count=0;
        CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&f.ops)==-1); ++error_cases;
        CHECK(!p->entity.owner && f.world.entity_count==0);
        CHECK(!f.gameplay.updating && !f.gameplay.lifecycle.updating && !f.runtime.updating);
        cleanup(&f);
    }
    /* Find a real line endpoint that participates in an interaction sweep.
     * Only its script kind is edited, and restoration precedes map cleanup. */
    {
        Fixture f; SsaPlayerSpawn *p; unsigned n; int found=0;
        setup(&f,rom,(size_t)size,0,&tables,&defs); p=f.runtime.player;
        for(n=0;n<f.map.line_count && !found;++n) {
            uint8_t *record=f.map.data+f.map.lines+n*40;
            const uint8_t *vertex=f.map.data+f.map.vertices+u16(record)*4;
            uint8_t lo=record[36],hi=record[37]; SsaEntity probe=p->entity;
            probe.body.x=(int32_t)s16(vertex)*65536; probe.body.y=(int32_t)s16(vertex+2)*65536;
            record[36]=1; record[37]=0;
            if(ssa_player_interaction_query(&f.gameplay.interaction,&probe,0,&tables)==0 && f.gameplay.interaction.special_count) {
                CHECK(ssa_world_remove(&f.world,&p->entity));
                p->entity.body.x=probe.body.x; p->entity.body.y=probe.body.y;
                CHECK(ssa_world_insert(&f.world,&p->entity));
                p->motion.x=p->entity.body.x; p->motion.y=p->entity.body.y;
                p->motion.z=p->entity.body.z; p->motion.movement.heading=0; p->motion.facing=0;
                f.fail=7;
                CHECK(ssa_player_gameplay_step(&f.gameplay,&f.input,&f.ops)==-1);
                CHECK(!f.gameplay.updating && !f.runtime.updating && line_events>0);
                ++error_cases; found=1;
            }
            record[36]=lo; record[37]=hi;
        }
        CHECK(found); cleanup(&f);
    }
    CHECK(ssa_player_gameplay_init(NULL,NULL,NULL)==-1);
    CHECK(ssa_player_gameplay_step(NULL,NULL,NULL)==-1);
    CHECK(ssa_player_gameplay_free(NULL)==-1);
    CHECK(respawns==32 && hazard_cases==32 && line_events>0 && sounds>0 && animations>0);
    free(rom);
    printf("{\"assertions\":%u,\"real_map_frames\":%u,\"maps\":16,\"death_frames\":%u,"
           "\"respawns\":%u,\"hazard_death_cases\":%u,\"error_cases\":%u,\"reentry_cases\":%u,"
           "\"third_person_frames\":%u,\"line_events\":%u,\"sound_events\":%u,\"animation_events\":%u}\n",
           checks,frames,death_frames,respawns,hazard_cases,error_cases,reentries,third_frames,line_events,sounds,animations);
    return 0;
}
