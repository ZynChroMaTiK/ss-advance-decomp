#include "ssa/enemy_queries.h"
#include <assert.h>
#include <stdlib.h>

/* Uses an isolated real world; the level wrapper only needs its borrowed map
 * because this helper exercises read-only queries, never VM/movers. */
unsigned ssa_test_enemy_queries_order(SsaMapAssets *map,const SsaMathTables *math,
                                      int32_t x,int32_t y) {
    SsaWorld world={0};
    SsaLevelRuntime level={0};
    SsaScriptSpawn *spawn=calloc(1,sizeof(*spawn));
    SsaEnemyQueries q={0},bad={0};
    SsaEntity player={0},pickup={0},*out,*reserved[2],*duplicate[2];
    SsaEntity *a,*b,*c;
    unsigned slots[3],i,assertions=0;
    int blocked,mask;
    int32_t dx,dy;
    SsaEnemyNavigation nav={0};
#define CHECK(v) do { assert(v); ++assertions; } while (0)
    CHECK(spawn!=NULL);
    level.map=map;
    CHECK(ssa_world_init(&world,map,100)==1);
    player.body.x=pickup.body.x=x; player.body.y=pickup.body.y=y;
    pickup.type=0xe1;
    CHECK(ssa_world_insert(&world,&player)==1);
    CHECK(ssa_world_insert(&world,&pickup)==1);
    CHECK(ssa_script_spawn_init(spawn,&level,&world)==0);
    CHECK(spawn->reserved==2);
    reserved[0]=&player; reserved[1]=&pickup;
    duplicate[0]=duplicate[1]=&player;
    CHECK(ssa_enemy_queries_init(&bad,spawn,math,duplicate,2)==-1);
    CHECK(!bad.spawn && !bad.ray.walls.map && !bad.ray.traversal.intercepts);
    CHECK(ssa_enemy_queries_init(&bad,spawn,math,reserved,1)==-1);
    CHECK(ssa_enemy_queries_init(&bad,spawn,math,NULL,2)==-1);
    for (i=0;i<3;++i) {
        SsaScriptActor *actor;
        CHECK(ssa_entity_pool_allocate(&spawn->pool,&slots[i])==1);
        actor=&spawn->actors[slots[i]];
        actor->owned=1; actor->entity.type=(uint16_t)(0xcd+i);
        actor->entity.body.x=x; actor->entity.body.y=y;
        actor->activation.entity=&actor->entity;
        CHECK(ssa_world_insert(&world,&actor->entity)==1);
    }
    a=&spawn->actors[slots[0]].entity;
    b=&spawn->actors[slots[1]].entity;
    c=&spawn->actors[slots[2]].entity;
    CHECK(ssa_enemy_queries_init(&q,spawn,math,reserved,2)==0);
    CHECK(q.reserved[0]==&player && q.reserved[1]==&pickup);
    /* The init copies the array, not its mutable container. */
    reserved[0]=reserved[1]=NULL;
    out=NULL;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==a);
    /* Reserved entities participate in original order when their type matches. */
    pickup.type=0xcd;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==&pickup);
    pickup.type=0xe1;
    CHECK(ssa_script_spawn_release(spawn,slots[0])==0);
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==c);
    CHECK(ssa_entity_pool_allocate(&spawn->pool,&i)==1 && i==slots[0]);
    spawn->actors[i].owned=1;
    a->type=0xcd; a->body.x=x; a->body.y=y;
    spawn->actors[i].activation.entity=a;
    CHECK(ssa_world_insert(&world,a)==1);
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==c);
    c->body.flags|=0x20;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==b);
    c->body.flags&=(uint16_t)~0x20u;
    world.blocking_entity=&pickup;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==c);
    CHECK(world.blocking_entity==&pickup);
    /* Invalid projection fails without overwriting the output. */
    out=&pickup;
    q.reserved[1]=&player;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==-1 && out==&pickup);
    q.reserved[1]=&pickup;
    pickup.owner=NULL;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==-1 && out==&pickup);
    pickup.owner=&world;
    ++spawn->pool.positions[slots[1]];
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==-1 && out==&pickup);
    --spawn->pool.positions[slots[1]];
    --world.entity_count;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==-1 && out==&pickup);
    ++world.entity_count;
    /* Controller updates deliberately hold spawn->updating while making these
     * queries; free still rejects teardown during the update. */
    spawn->updating=1;
    CHECK(q.ops.nearest(q.ops.context,&player,&out)==0 && out==c);
    CHECK(ssa_enemy_queries_free(&q)==-1);
    spawn->updating=0;
    nav.entity=c;
    {
        unsigned *busy[]={&q.updating,&q.path.updating,&q.ray.updating,
            &q.ray.traversal.updating,&q.ray.walls.updating,&world.query_active,
            &level.updating,&level.script.updating,&level.movers.updating,
            &level.switches.updating};
        for (i=0;i<sizeof(busy)/sizeof(busy[0]);++i) {
            *busy[i]=1; out=&pickup; blocked=41; mask=42; dx=43; dy=44;
            CHECK(q.ops.nearest(q.ops.context,&player,&out)==-1 && out==&pickup);
            CHECK(q.ops.visibility(q.ops.context,0,0,&blocked)==-1 && blocked==41);
            CHECK(q.ops.path(q.ops.context,&nav,x,y,x,y,0,0,0,&mask,&dx,&dy)==-1 &&
                  mask==42 && dx==43 && dy==44);
            CHECK(ssa_enemy_queries_free(&q)==-1);
            *busy[i]=0;
        }
    }
    CHECK(ssa_enemy_queries_free(&q)==0);
    CHECK(ssa_script_spawn_free(spawn)==0);
    ssa_world_free(&world);
    CHECK(!player.owner && !pickup.owner);
    free(spawn);
#undef CHECK
    return assertions;
}
