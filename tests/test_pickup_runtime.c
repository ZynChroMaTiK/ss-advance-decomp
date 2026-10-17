#include "ssa/pickup_runtime.h"
#include "ssa/weapon_select.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,contacts,events,reentries,original_pickups,synthetic_pickups,refusals,failures;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"check failed line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct Fixture {
    SsaMapAssets map;SsaLevelRuntime level;SsaSpawnPlan plan;SsaWorld world;SsaSlide slide;
    SsaScriptSpawn spawn;SsaPlayerSpawn players[4];SsaPlayerRuntime player;
    SsaCombatRuntime combat;SsaCombatDamage damage;SsaPickupRuntime pickup;SsaInventoryRules rules;
    SsaPickupRuntimeOps ops;uint32_t score;
} Fixture;
static void effect(void *context,const SsaPickupRuntime *r,unsigned player,enum SsaPickupEvent kind,unsigned a,unsigned b)
{
    Fixture *f=context;SsaEntity *e=&f->spawn.actors[r->slot].entity;(void)a;(void)b;
    CHECK(r==&f->pickup && player==0 && (kind==SSA_PICKUP_SOUND || kind==SSA_PICKUP_MESSAGE));
    CHECK(r->updating&&f->spawn.updating&&f->combat.updating&&f->player.updating&&f->level.updating);
    CHECK(f->world.executing_special==e && e->owner==&f->world);
    CHECK(f->players[0].entity_health==f->players[0].inventory.health);
    CHECK(ssa_pickup_runtime_free(&f->pickup)==-1);
    CHECK(ssa_pickup_runtime_execute(&f->pickup,&f->players[0].entity,e)==-1);
    CHECK(ssa_script_spawn_release(&f->spawn,r->slot)==-1);
    CHECK(ssa_combat_runtime_free(&f->combat)==-1);
    CHECK(ssa_player_runtime_free(&f->player)==-1);reentries+=5;++events;
}
static void setup(Fixture *f,const uint8_t *rom,const SsaMathTables *math,const SsaSpawnDefinitions *defs,unsigned level)
{
    SsaEntity *reserved[100];SsaPlayerRuntime *players[1];size_t i;
    memset(f,0,sizeof(*f));CHECK(ssa_map_assets_load(&f->map,rom,8388608,level)==1);
    CHECK(ssa_level_runtime_init(&f->level,&f->map,rom,8388608,level)==0);
    CHECK(ssa_spawn_plan_load(&f->plan,&f->map,defs,level,1,99)==1);
    CHECK(ssa_world_init(&f->world,&f->map,100)==1);CHECK(ssa_spawn_plan_insert(&f->plan,&f->world)==1);
    CHECK(ssa_players_spawn(f->players,&f->world,&f->plan,1,0)==1);
    CHECK(ssa_slide_init(&f->slide,&f->map)==1);
    CHECK(ssa_player_runtime_init(&f->player,&f->players[0],&f->world,&f->slide,math)==0);
    CHECK(ssa_script_spawn_init(&f->spawn,&f->level,&f->world)==0);
    CHECK(ssa_script_spawn_adopt_plan(&f->spawn,&f->plan)==0);
    for(i=0;i<f->plan.object_count;++i)reserved[i]=&f->spawn.actors[i].entity;
    reserved[i]=&f->players[0].entity;players[0]=&f->player;f->damage.player_count=1;
    CHECK(ssa_combat_runtime_init(&f->combat,&f->spawn,reserved,i+1,players,1,&f->damage,&f->score)==0);
    CHECK(ssa_inventory_rules_load(&f->rules,rom,8388608)==0);f->ops=(SsaPickupRuntimeOps){effect,f};
    CHECK(ssa_pickup_runtime_init(&f->pickup,&f->combat,&f->rules,&f->ops)==0);
    CHECK(ssa_pickup_runtime_init(&f->pickup,&f->combat,&f->rules,&f->ops)==-1);
}
static void warp(Fixture *f,const SsaEntity *e)
{
    SsaPlayerSpawn *p=&f->players[0];
    CHECK(ssa_world_remove(&f->world,&p->entity)==1);
    p->entity.body.x=p->motion.x=e->body.x;p->entity.body.y=p->motion.y=e->body.y;p->entity.body.z=e->body.z;
    CHECK(ssa_world_insert(&f->world,&p->entity)==1);
}
static int query(Fixture *f)
{
    SsaPlayerSpawn *p=&f->players[0];int result=ssa_world_query(&f->world,&p->entity,p->entity.body.x,p->entity.body.y);
    CHECK(!f->world.query_active&&!f->world.executing_special&&!f->pickup.updating&&!f->spawn.updating&&!f->combat.updating);
    CHECK(f->spawn.pool.active_count==f->world.entity_count);++contacts;return result;
}
static unsigned add(Fixture *f,unsigned animation,unsigned trigger)
{
    unsigned slot;SsaScriptActor *a;CHECK(ssa_entity_pool_allocate(&f->spawn.pool,&slot)==1);a=&f->spawn.actors[slot];
    CHECK(!a->owned);memset(a,0,sizeof(*a));a->owned=1;a->entity.type=0xe1;
    a->entity.body.x=f->players[0].entity.body.x;a->entity.body.y=f->players[0].entity.body.y;a->entity.body.z=f->players[0].entity.body.z;
    a->activation.entity=&a->entity;a->animation=(uint16_t)animation;a->script.trigger=(uint16_t)trigger;
    CHECK(ssa_world_insert(&f->world,&a->entity)==1);return slot;
}
static void cleanup(Fixture *f)
{
    CHECK(ssa_pickup_runtime_free(&f->pickup)==0);CHECK(!f->world.execute_special&&!f->world.execute_context);
    CHECK(ssa_pickup_runtime_free(&f->pickup)==-1);CHECK(ssa_combat_runtime_free(&f->combat)==0);
    CHECK(ssa_script_spawn_free(&f->spawn)==0);CHECK(ssa_player_runtime_free(&f->player)==0);
    ssa_world_free(&f->world);ssa_slide_free(&f->slide);CHECK(ssa_spawn_plan_free(&f->plan)==1);
    CHECK(ssa_level_runtime_free(&f->level)==0);ssa_map_assets_free(&f->map);
}
int main(int argc,char **argv)
{
    FILE *file;uint8_t *rom;SsaMathTables *math;SsaSpawnDefinitions defs;Fixture *f;unsigned level,slot;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);rom=malloc(8388608);math=malloc(sizeof(*math));f=malloc(sizeof(*f));CHECK(rom&&math&&f);
    CHECK(fread(rom,1,8388608,file)==8388608);CHECK(!fclose(file));CHECK(ssa_math_tables_load(math,rom,8388608)==1);
    CHECK(ssa_spawn_definitions_load(&defs,rom,8388608)==1);
    for(level=0;level<16;++level){
        SsaInventory *inv;unsigned before,count=0,health_slot,weapon_slot;SsaEntity copy;
        setup(f,rom,math,&defs,level);inv=&f->players[0].inventory;
        for(slot=0;slot<f->spawn.reserved;++slot){
            SsaScriptActor *a=&f->spawn.actors[slot];unsigned item;
            if(!a->owned || a->entity.type!=0xe1)continue;
            item=a->animation;inv->health=1;inv->stat_c4=inv->stat_c6=0;inv->pending_count=0;
            inv->weapons=(uint16_t)(item>=0x20 && item<=0x29?1:1023);memset(inv->ammo,0,sizeof(inv->ammo));
            /* Exercise each original pickup at its genuine map position. */
            CHECK(ssa_level_script_reset(&f->level.script)==1);warp(f,&a->entity);before=f->pickup.game.collected;
            {unsigned retry=0;
                do{
                    unsigned collected=f->pickup.game.collected;
                    inv->health=1;inv->stat_c4=inv->stat_c6=0;inv->pending_count=0;memset(inv->ammo,0,sizeof(inv->ammo));
                    CHECK(ssa_level_script_reset(&f->level.script)==1);
                    CHECK(query(f)>=0);CHECK(!f->pickup.failed);
                    CHECK(f->pickup.game.collected>collected);CHECK(++retry<=100);
                }while(a->owned);
            }
            CHECK(!a->owned);
            CHECK(f->pickup.game.collected>before);count+=(unsigned)(f->pickup.game.collected-before);
        }
        original_pickups+=count;
        /* Adopted-prefix releases remain reusable; two E1s in one cell must
         * release only the encountered head, then stop traversal this query. */
        inv->health=100;inv->weapons=1;inv->pending_count=0;
        health_slot=add(f,0x30,0);weapon_slot=add(f,0x20,0);before=f->pickup.game.collected;
        CHECK(query(f)>=0);CHECK(f->pickup.game.collected==before+1);
        CHECK(!f->spawn.actors[weapon_slot].owned&&f->spawn.actors[health_slot].owned);
        CHECK(inv->pending_count==1&&inv->pending_weapons[0]==2);synthetic_pickups++;
        {SsaWeaponControl control={1,0,1,0};CHECK(ssa_weapon_update(inv,&control,&f->rules,1)==0);CHECK(inv->weapons&2);}
        before=f->pickup.game.collected;CHECK(query(f)>=0);CHECK(f->pickup.game.collected==before);refusals++;
        inv->health=50;f->player.updating=1;f->level.updating=1;
        CHECK(query(f)>=0);CHECK(f->player.updating==1&&f->level.updating==1);
        f->player.updating=f->level.updating=0;
        CHECK(inv->health==75&&f->players[0].entity_health==75&&!f->spawn.actors[health_slot].owned);synthetic_pickups++;
        /* Native trigger queues before refusal, and its bit clears only once. */
        if(f->level.data.count[level]){
            CHECK(ssa_level_script_reset(&f->level.script)==1);slot=add(f,0x30,0x101);inv->health=100;
            CHECK(query(f)>=0);CHECK(f->level.script.count==1&&f->spawn.actors[slot].script.trigger==1);refusals++;
            CHECK(query(f)>=0);CHECK(f->level.script.count==1);refusals++;
            inv->health=1;CHECK(query(f)>=0);CHECK(inv->health==26);synthetic_pickups++;
        }
        /* Dispatch rejects fake/inactive ownership before inventory changes. */
        slot=add(f,0x30,0);copy=f->spawn.actors[slot].entity;
        CHECK(ssa_pickup_runtime_execute(&f->pickup,&f->players[0].entity,&f->spawn.actors[slot].entity)==-1);
        CHECK(memcmp(&copy,&f->spawn.actors[slot].entity,sizeof(copy))==0);
        f->pickup.updating=1;CHECK(ssa_pickup_runtime_free(&f->pickup)==-1);f->pickup.updating=0;
        f->spawn.pool.positions[slot]=100;CHECK(query(f)==-1);f->spawn.pool.positions[slot]=(uint16_t)(f->spawn.pool.active_count-1);failures++;
        inv->health=1;CHECK(query(f)>=0);synthetic_pickups++;
        /* Malformed script trigger latches failure; no actor is removed. */
        slot=add(f,0x30,0x1ff);inv->health=1;CHECK(query(f)==-1);
        CHECK(f->pickup.failed&&f->spawn.actors[slot].owned&&f->spawn.actors[slot].entity.owner==&f->world);failures++;
        CHECK(!f->combat.updating&&!f->spawn.updating&&!f->player.updating&&!f->level.updating);
        cleanup(f);
    }
    free(f);free(math);free(rom);
    printf("{\"asan_ubsan_assertions\":%u,\"maps\":16,\"world_queries\":%u,\"original_map_pickups\":%u,\"synthetic_pickups\":%u,\"refusals\":%u,\"presentation_events\":%u,\"callback_reentry_guards\":%u,\"failure_cases\":%u}\n",checks,contacts,original_pickups,synthetic_pickups,refusals,events,reentries,failures);return 0;
}
