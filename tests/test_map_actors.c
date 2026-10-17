#include "ssa/map_actors.h"
#include "ssa/player_spawn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,frames,retained;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;} } while(0)
static int only_player(const SsaWorld *w,const SsaEntity *p) {
    size_t i;unsigned blocks=0,sectors=0;
    if(w->entity_count!=1||p->owner!=w)return 0;
    for(i=0;i<(size_t)w->width*w->height;i++) {
        const SsaEntity *e=w->cells[i];
        if(e) { if(e!=p||e->block_next||e->block_previous)return 0;blocks++; }
    }
    for(i=0;i<w->collision.map->sector_count;i++) {
        const SsaEntity *e=w->sectors[i];
        if(e) { if(e!=p||e->sector_next||e->sector_previous)return 0;sectors++; }
    }
    return blocks==1&&sectors==1;
}
int main(int argc,char **argv) {
    FILE *file;long length;uint8_t *rom;SsaSpawnDefinitions definitions;
    SsaActorAnimations data;unsigned level;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file!=NULL);
    CHECK(fseek(file,0,SEEK_END)==0);length=ftell(file);CHECK(length>0);rewind(file);
    rom=(uint8_t *)malloc((size_t)length);CHECK(rom!=NULL);
    CHECK(fread(rom,1,(size_t)length,file)==(size_t)length);CHECK(fclose(file)==0);
    memset(&definitions,0,sizeof(definitions));memset(&data,0,sizeof(data));
    CHECK(ssa_spawn_definitions_load(&definitions,rom,(size_t)length)==1);
    CHECK(ssa_actor_animations_load(&data,rom,(size_t)length)==1);
    for(level=0;level<16;level++) {
        SsaMapAssets map={0};SsaWorld world={0};SsaMapActors actors={0},before;
        SsaPlayerSpawn players[4]={0};SsaActorAnimation saved[100];
        size_t n,i;unsigned frame;
        CHECK(ssa_map_assets_load(&map,rom,(size_t)length,level)==1);
        CHECK(ssa_world_init(&world,&map,100)==1);
        before=actors;
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,16)==-1);
        CHECK(memcmp(&before,&actors,sizeof(actors))==0&&world.entity_count==0);
        world.query_active=1;
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,level)==-1);
        CHECK(memcmp(&before,&actors,sizeof(actors))==0&&world.entity_count==0);
        world.query_active=0;data.loaded=0;
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,level)==-1);
        CHECK(memcmp(&before,&actors,sizeof(actors))==0&&world.entity_count==0);
        data.loaded=1;
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,level)==1);
        n=actors.plan.object_count;CHECK(n<100&&world.entity_count==n);
        CHECK(ssa_players_spawn(players,&world,&actors.plan,1,0)==1);
        CHECK(players[0].active&&players[0].entity.owner==&world&&world.entity_count==n+1);
        before=actors;
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,level)==-1);
        CHECK(memcmp(&before,&actors,sizeof(actors))==0&&world.entity_count==n+1);
        for(i=0;i<n;i++) {
            CHECK(ssa_map_actor_object(&actors,(unsigned)i)==&actors.plan.objects[i]);
            CHECK(ssa_map_actor_animation(&actors,(unsigned)i)==&actors.animations[i]);
        }
        CHECK(ssa_map_actor_object(&actors,(unsigned)n)==NULL);
        CHECK(ssa_map_actor_animation(&actors,(unsigned)n)==NULL);
        for(frame=0;frame<128;frame++) {
            for(i=0;i<n;i++)actors.plan.objects[i].entity.body.flags=(uint16_t)((frame+i)%7?0:0x800);
            CHECK(ssa_map_actors_step(&actors,frame%6==0?255:1)==1);frames++;
            CHECK(world.entity_count==n+1&&players[0].entity.owner==&world);
        }
        before=actors;
        CHECK(ssa_map_actors_step(&actors,256)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
        actors.updating=1;before=actors;
        CHECK(ssa_map_actors_step(&actors,1)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
        CHECK(ssa_map_actors_free(&actors)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
        actors.updating=0;world.query_active=1;before=actors;
        CHECK(ssa_map_actors_step(&actors,1)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
        CHECK(ssa_map_actors_free(&actors)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
        world.query_active=0;
        if(n) {
            uint16_t old;
            actors.plan.objects[n-1].entity.body.flags=0;
            old=actors.animations[n-1].animation;actors.animations[n-1].animation=65535;
            memcpy(saved,actors.animations,sizeof(saved));
            CHECK(ssa_map_actors_step(&actors,1)==-1);
            CHECK(memcmp(saved,actors.animations,sizeof(saved))==0);
            actors.animations[n-1].animation=old;
            actors.plan.objects[n-1].entity.owner=NULL;before=actors;
            CHECK(ssa_map_actors_step(&actors,1)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
            CHECK(ssa_map_actors_free(&actors)==-1&&memcmp(&before,&actors,sizeof(actors))==0);
            CHECK(ssa_map_actor_object(&actors,(unsigned)n-1)==NULL);
            actors.plan.objects[n-1].entity.owner=&world;
        }
        CHECK(ssa_map_actors_free(&actors)==1);
        CHECK(only_player(&world,&players[0].entity));retained++;
        CHECK(!actors.loaded&&!actors.plan.objects&&!actors.world&&!actors.data);
        CHECK(ssa_map_actor_object(&actors,0)==NULL&&ssa_map_actor_animation(&actors,0)==NULL);
        CHECK(ssa_map_actors_step(&actors,1)==-1&&ssa_map_actors_free(&actors)==-1);
        CHECK(ssa_world_remove(&world,&players[0].entity)==1&&world.entity_count==0);
        CHECK(ssa_map_actors_init(&actors,&world,&definitions,&data,level)==1);
        CHECK(ssa_map_actors_free(&actors)==1&&world.entity_count==0);
        ssa_world_free(&world);ssa_map_assets_free(&map);
    }
    free(rom);
    printf("{\"asan_ubsan_assertions\":%u,\"ownership_frames\":%u,\"retained_players\":%u}\n",checks,frames,retained);
    return 0;
}
