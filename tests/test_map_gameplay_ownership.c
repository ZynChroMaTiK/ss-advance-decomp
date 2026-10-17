#include "ssa/combat_runtime.h"
#include "ssa/actor_animation_runtime.h"
#include "ssa/enemy_queries.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"ownership line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static unsigned checks;
static int event(void *p,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
 SsaCombatDamageEvent k,uint32_t a,uint32_t b){(void)p;(void)r;(void)v;(void)k;(void)a;(void)b;return 0;}
int main(int argc,char **argv){
 uint8_t *rom;FILE *f;SsaMathTables math;SsaSpawnDefinitions defs;SsaActorAnimations data;unsigned level;
 CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f);rom=malloc(0x800000);CHECK(rom);
 CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
 CHECK(ssa_math_tables_load(&math,rom,0x800000));CHECK(ssa_spawn_definitions_load(&defs,rom,0x800000));
 CHECK(ssa_actor_animations_load(&data,rom,0x800000)==1);
 for(level=0;level<16;++level){
  SsaMapAssets map={0};SsaLevelRuntime l={0};SsaSpawnPlan plan={0};SsaWorld w={0};SsaSlide slide={0};
  SsaPlayerSpawn players[4]={0};SsaPlayerRuntime player={0};SsaScriptSpawn s={0};SsaCombatRuntime combat={0};
  SsaCombatDamage damage={0};SsaActorAnimationRuntime anim={0};SsaActorAnimation pa={0};
  SsaEnemyQueries queries={0};SsaEntity *bindings[100]={0};SsaActorAnimation *states[100]={0};
  SsaPlayerRuntime *pr[]={&player};SsaCombatRuntimeOps ops={event,NULL};uint32_t score=0;unsigned i,n,slot;
  SsaEntity *before[100]={0};SsaEntity payload[100];unsigned prior[100]={0},next[100]={0};
  CHECK(ssa_map_assets_load(&map,rom,0x800000,level));CHECK(ssa_level_runtime_init(&l,&map,rom,0x800000,level)==0);
  CHECK(ssa_spawn_plan_load(&plan,&map,&defs,level,1,99));CHECK(ssa_world_init(&w,&map,100));
  CHECK(ssa_slide_init(&slide,&map));CHECK(ssa_spawn_plan_insert(&plan,&w));
  CHECK(ssa_players_spawn(players,&w,&plan,1,0));CHECK(players[0].active);
  CHECK(ssa_player_runtime_init(&player,&players[0],&w,&slide,&math)==0);
  n=(unsigned)plan.object_count;
  for(i=0;i<n;++i)before[i]=&plan.objects[i].entity;
  before[n]=&players[0].entity;
  for(i=0;i<=n;++i){unsigned j;payload[i]=*before[i];prior[i]=next[i]=100;
   for(j=0;j<=n;++j){if(before[i]->sector_previous==before[j])prior[i]=j;if(before[i]->sector_next==before[j])next[i]=j;}}
  CHECK(ssa_script_spawn_init(&s,&l,&w)==0);CHECK(ssa_script_spawn_adopt_plan(&s,&plan)==0);
  if(n)CHECK(ssa_script_spawn_adopt_plan(&s,&plan)==-1);
  for(i=0;i<n;++i){bindings[i]=&s.actors[i].entity;CHECK(s.actors[i].owned);CHECK(!plan.objects[i].entity.owner);
   CHECK(!memcmp(&bindings[i]->body,&payload[i].body,sizeof(SsaBody)));
   CHECK(s.actors[i].activation.entity==bindings[i]);CHECK(s.actors[i].script.trigger==plan.objects[i].trigger);
   CHECK(s.actors[i].animation==plan.objects[i].animation);}
  bindings[n]=&players[0].entity;states[n]=&pa;
  for(i=0;i<=n;++i){CHECK(bindings[i]->sector_previous==(prior[i]==100?NULL:bindings[prior[i]]));
   CHECK(bindings[i]->sector_next==(next[i]==100?NULL:bindings[next[i]]));CHECK(s.pool.active_slots[i]==i);
   CHECK(ssa_script_spawn_entity(&s,bindings,i)==bindings[i]);}
  damage.player_count=1;damage.difficulty=1;
  CHECK(ssa_combat_runtime_init(&combat,&s,bindings,n+1,pr,1,&damage,&score)==0);
  CHECK(ssa_enemy_queries_init(&queries,&s,&math,bindings,n+1)==0);
  CHECK(ssa_actor_animation_set(&pa,players[0].animation,players[0].animation_ticks)==1);
  CHECK(ssa_actor_animation_runtime_init(&anim,&s,&data,bindings,states,n+1)==0);
  CHECK(ssa_actor_animation_runtime_step(&anim,1)==0);
  for(i=0;i<n;++i)if(bindings[i]->type>=0xcd && bindings[i]->type<=0xd8){
   s.actors[i].activation.health=100;bindings[i]->body.flags=8;
   CHECK(ssa_combat_runtime_damage(&combat,bindings[i],&players[0].entity,NULL,1,&ops)==0);
   CHECK(s.actors[i].activation.health<100);break;}
  for(i=0;i<n;++i)CHECK(ssa_script_spawn_release(&s,i)==0);
  CHECK(s.pool.active_count==1 && w.entity_count==1);CHECK(ssa_actor_animation_runtime_step(&anim,1)==0);
  CHECK(ssa_entity_pool_allocate(&s.pool,&slot)==1);CHECK(!n || slot<n);
  s.actors[slot].owned=1;s.actors[slot].activation.entity=&s.actors[slot].entity;
  s.actors[slot].entity.type=0xe1;s.actors[slot].entity.body=players[0].entity.body;
  s.actors[slot].animation=0;
  CHECK(ssa_world_insert(&w,&s.actors[slot].entity));CHECK(ssa_actor_animation_runtime_step(&anim,1)==0);
  CHECK(ssa_script_spawn_entity(&s,bindings,slot)==&s.actors[slot].entity);
  CHECK(ssa_enemy_queries_free(&queries)==0);CHECK(ssa_actor_animation_runtime_free(&anim)==0);
  CHECK(ssa_combat_runtime_free(&combat)==0);CHECK(ssa_script_spawn_free(&s)==0);
  CHECK(ssa_player_runtime_free(&player)==0);ssa_world_free(&w);ssa_slide_free(&slide);
  CHECK(ssa_spawn_plan_free(&plan));CHECK(ssa_level_runtime_free(&l)==0);ssa_map_assets_free(&map);
 }
 free(rom);printf("{\"checks\":%u,\"maps\":16}\n",checks);return 0;
}
