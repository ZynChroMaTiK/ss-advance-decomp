/* Reuse real map, script, player and controller fixtures. */
#define SSA_CD_TEST_MAIN enemy_cd_fixture_main
#include "test_enemy_cd_runtime.c"
#include "ssa/enemy_queries.h"
unsigned ssa_test_enemy_queries_order(SsaMapAssets *,const SsaMathTables *,int32_t,int32_t);

static unsigned query_checks, clear_paths, blocked_paths, query_guards, order_checks;
static void query_cases(Fixture *f,CdCase *c,SsaEnemyQueries *q) {
    unsigned i;
    SsaEntity *e=c->runtime.navigation.entity,*out;
    SsaEntity *old=f->world.blocking_entity;
    int mask,blocked; int32_t dx,dy;
    for(i=0;i<f->map.vertex_count;i+=3) {
        const uint8_t *v=f->map.data+f->map.vertices+i*4;
        int16_t vx=(int16_t)(v[0]|v[1]<<8),vy=(int16_t)(v[2]|v[3]<<8);
        CHECK(q->ops.path(q,&c->runtime.navigation,e->body.x,e->body.y,
            (int32_t)vx*65536,(int32_t)vy*65536,e->radius,i&1,i%2,&mask,&dx,&dy)==0);
        CHECK(mask>=0 && mask<=3 && dx==q->path.offset_x && dy==q->path.offset_y);
        CHECK(f->world.blocking_entity==old && !q->updating && !q->ray.updating);
        if(mask==3) ++clear_paths; else ++blocked_paths;
        ++query_checks;
    }
    CHECK(q->ops.nearest(q,&f->players[0].entity,&out)==0 && out==e);++query_checks;
    CHECK(q->ops.visibility(q,e->sector,e->sector,&blocked)==0 && !blocked);++query_checks;
    q->updating=1;
    CHECK(q->ops.nearest(q,e,&out)==-1);
    CHECK(q->ops.visibility(q,0,0,&blocked)==-1);
    CHECK(q->ops.path(q,&c->runtime.navigation,0,0,0,0,0,0,0,&mask,&dx,&dy)==-1);
    CHECK(ssa_enemy_queries_free(q)==-1);q->updating=0;query_guards+=4;
    f->world.query_active=1;
    CHECK(q->ops.nearest(q,e,&out)==-1);
    CHECK(q->ops.visibility(q,0,0,&blocked)==-1);
    CHECK(ssa_enemy_queries_free(q)==-1);f->world.query_active=0;query_guards+=3;
    CHECK(q->ops.visibility(q,-1,0,&blocked)==-1);
    CHECK(q->ops.visibility(q,0,f->map.sector_count,&blocked)==-1);
    CHECK(q->ops.path(q,NULL,0,0,0,0,0,0,0,&mask,&dx,&dy)==-1);query_guards+=3;
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables tables;SsaSpawnDefinitions defs;
    int32_t speeds[12];unsigned level,thing=100,i;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&tables,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    for(i=0;i<64;++i) if(defs.thing[i].type==0xcd && !defs.thing[i].variant) {thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;CdCase c;SsaEnemyQueries q={0};SsaEntity *reserved[1];SsaScriptActor *a;
        setup(&f,rom,(size_t)size,level,&tables,&defs,100);reserved[0]=&f.players[0].entity;
        checks+=ssa_test_enemy_queries_order(&f.map,&tables,reserved[0]->body.x,reserved[0]->body.y);
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,&tables,reserved,0)==-1);++query_guards;
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,&tables,NULL,1)==-1);++query_guards;
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,&tables,reserved,1)==0);
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,&tables,reserved,1)==-1);++query_guards;
        script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,&tables,speeds[0]);c.ops.queries=&q.ops;
        a=&f.spawn.actors[c.runtime.slot];activate(&c);separate(&c);query_cases(&f,&c,&q);
        for(i=0;i<256;++i) {
            int32_t x=a->entity.body.x,y=a->entity.body.y;
            player_step(&f,i);
            if(a->activation.state==1) {
                CHECK(tick(&c,1)==0);++chase_frames;
                if(x!=a->entity.body.x || y!=a->entity.body.y) ++chase_moves;
            }
            CHECK(a->owned && a->entity.owner==&f.world);
        }
        if(a->activation.state==1) {
            a->parameter34=1;a->parameter36=0;a->activation.health=1;
            CHECK(tick(&c,1)==0);
        }
        expire(&c);total_walks+=c.walks;
        {
            SsaEntity *out=(SsaEntity *)1;
            CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && !out);++query_checks;
        }
        CHECK(ssa_enemy_queries_free(&q)==0);cleanup(&f);
    }
    /* Equal distances follow the pool's active order, including swap-removal
     * and slot reuse. External reserved entities keep their supplied slots. */
    {
        Fixture f;SsaEnemyQueries q={0};SsaEntity *reserved[1],*out;unsigned a,b,c;
        setup(&f,rom,(size_t)size,0,&tables,&defs,100);reserved[0]=&f.players[0].entity;
        script(&f,rom,thing,1,3);step(&f,1);step(&f,1);step(&f,1);CHECK(owned(&f)==3);
        a=f.spawn.pool.active_slots[1];b=f.spawn.pool.active_slots[2];c=f.spawn.pool.active_slots[3];
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,&tables,reserved,1)==0);
        CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && out==&f.spawn.actors[a].entity);++order_checks;
        CHECK(ssa_script_spawn_release(&f.spawn,a)==0);
        CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && out==&f.spawn.actors[c].entity);++order_checks;
        f.spawn.actors[c].entity.body.flags|=0x20;
        CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && out==&f.spawn.actors[b].entity);++order_checks;
        script(&f,rom,thing,1,1);step(&f,1);CHECK(f.spawn.actors[a].owned);
        CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && out==&f.spawn.actors[b].entity);++order_checks;
        f.spawn.actors[b].entity.body.flags|=0x20;
        CHECK(q.ops.nearest(&q,reserved[0],&out)==0 && out==&f.spawn.actors[a].entity);++order_checks;
        ++f.spawn.pool.active_count;CHECK(q.ops.nearest(&q,reserved[0],&out)==-1);--f.spawn.pool.active_count;++query_guards;
        CHECK(ssa_enemy_queries_free(&q)==0);cleanup(&f);
    }
    CHECK(chase_moves && clear_paths && blocked_paths && airborne && third_frames);
    free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"controller_frames\":%u,\"chase_frames\":%u,\"enemy_moves\":%u,\"walk_calls\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"path_and_target_queries\":%u,\"clear_paths\":%u,\"blocked_paths\":%u,\"pool_order_checks\":%u,\"guards\":%u,\"releases\":%u,\"callback_reentry\":%u}\n",
        checks,runtime_frames,chase_frames,chase_moves,total_walks,player_frames,airborne,third_frames,
        query_checks,clear_paths,blocked_paths,order_checks,query_guards,released,runtime_reentry);
    return 0;
}
