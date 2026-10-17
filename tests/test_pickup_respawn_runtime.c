#define SSA_WEAPON_TEST_MAIN weapon_fixture_main
#include "test_weapon_runtime.c"
#include "ssa/pickup_respawn_runtime.h"
#include "ssa/entity_timer.h"
typedef struct PickupCase {
    Fixture *f;Multiplayer mp;SsaPickupRespawnRuntime runtime;SsaPickupRespawnRuntimeOps ops;
    unsigned particles,fail_particle;int32_t last_z;
} PickupCase;
static unsigned pickup_frames,pickup_spawns,pickup_particles,pickup_guards,pickup_failures;
static unsigned pickup_occupied,pickup_full,pickup_reuses,pickup_timer_steps;
static int pickup_particle(void *context,const SsaPickupRespawnRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    PickupCase *t=context;SsaScriptSpawn *s=&t->f->spawn;unsigned i;
    CHECK(r==&t->runtime && id==5 && r->updating && r->state.updating);
    CHECK(s->updating && t->mp.combat.updating && t->f->player.updating && t->mp.second.updating);
    CHECK(ssa_pickup_respawn_runtime_step(&t->runtime,1,0,&t->ops)==-1);
    CHECK(ssa_pickup_respawn_runtime_free(&t->runtime)==-1);
    CHECK(ssa_script_spawn_free(s)==-1);CHECK(ssa_combat_runtime_free(&t->mp.combat)==-1);
    pickup_guards+=4;
    if(r->pending_slot<100) {
        const SsaScriptActor *a=&s->actors[r->pending_slot];CHECK(a->owned && a->entity.owner==&t->f->world);
        CHECK(a->script.z==z && a->entity.body.z==z && a->entity.body.x==x && a->entity.body.y==y);
        CHECK(z==(int32_t)a->entity.body.floor*65536);
    }
    for(i=0;i<t->f->plan.teleport_count;++i)
        if((int32_t)t->f->plan.teleport_starts[i].x*65536==x && (int32_t)t->f->plan.teleport_starts[i].y*65536==y)break;
    CHECK(i<t->f->plan.teleport_count);++t->particles;++pickup_particles;t->last_z=z;
    return t->fail_particle?-1:0;
}
static void pickup_setup(PickupCase *t,Fixture *f,const SsaPickupRespawnData *data,const SsaMathTables *math) {
    Multiplayer *m;SsaEntity *reserved[2];SsaPlayerRuntime *players[2];
    memset(t,0,sizeof(*t));t->f=f;m=&t->mp;m->f=f;
    CHECK(ssa_script_spawn_free(&f->spawn)==0);
    f->players[1]=f->players[0];memset(&f->players[1].entity,0,sizeof(f->players[1].entity));
    f->players[1].entity.body=f->players[0].entity.body;f->players[1].entity.radius=f->players[0].entity.radius;
    f->players[1].entity.type=0xca;f->players[1].entity.player_index=1;
    CHECK(ssa_world_insert(&f->world,&f->players[1].entity)==1);CHECK(ssa_slide_init(&m->slide,&f->map));
    CHECK(ssa_player_runtime_init(&m->second,&f->players[1],&f->world,&m->slide,math)==0);
    CHECK(ssa_script_spawn_init(&f->spawn,&f->level,&f->world)==0);
    reserved[0]=&f->players[0].entity;reserved[1]=&f->players[1].entity;players[0]=&f->player;players[1]=&m->second;
    m->damage.player_count=2;m->damage.difficulty=1;
    CHECK(ssa_combat_runtime_init(&m->combat,&f->spawn,reserved,2,players,2,&m->damage,&m->score)==0);
    CHECK(ssa_pickup_respawn_runtime_init(&t->runtime,&m->combat,&f->plan,data)==0);
    t->ops=(SsaPickupRespawnRuntimeOps){pickup_particle,t};
    {
        static const unsigned marker_counts[4]={22,17,15,17};
        CHECK(f->plan.teleport_count==marker_counts[f->level.level-12]);
    }
}
static int pickup_step(PickupCase *t,unsigned ticks,int32_t fallback) {
    int result=ssa_pickup_respawn_runtime_step(&t->runtime,ticks,fallback,&t->ops);++pickup_frames;
    CHECK(!t->runtime.updating && !t->runtime.state.updating && !t->mp.combat.updating);
    CHECK(!t->f->spawn.updating && !t->f->player.updating && !t->mp.second.updating);return result;
}
static void pickup_cleanup(PickupCase *t) {
    Fixture *f=t->f;Multiplayer *m=&t->mp;
    CHECK(ssa_pickup_respawn_runtime_free(&t->runtime)==0);
    CHECK(ssa_combat_runtime_free(&m->combat)==0);CHECK(ssa_script_spawn_free(&f->spawn)==0);
    CHECK(ssa_player_runtime_free(&m->second)==0);ssa_slide_free(&m->slide);
    CHECK(ssa_world_remove(&f->world,&f->players[1].entity)==1);
    CHECK(ssa_script_spawn_init(&f->spawn,&f->level,&f->world)==0);cleanup(f);
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;SsaPickupRespawnData data;
    unsigned level,i,j;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_pickup_respawn_data_load(&data,rom,(size_t)size)==1);
    for(level=12;level<16;++level) {
        Fixture f;PickupCase t;unsigned last_slot=100;SsaPickupRespawn before;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);pickup_setup(&t,&f,&data,&math);
        CHECK(ssa_pickup_respawn_runtime_init(&t.runtime,&t.mp.combat,&f.plan,&data)==-1);++pickup_guards;
        for(i=0;i<16 || i<f.plan.teleport_count;++i) {
            unsigned marker=t.runtime.state.marker,sequence=t.runtime.state.sequence,slot;
            SsaScriptActor *a;unsigned particles=t.particles;
            t.runtime.state.elapsed=300;CHECK(pickup_step(&t,0,0)==0 && t.particles==particles);
            CHECK(pickup_step(&t,1,0)==0 && t.particles==particles+1);slot=t.runtime.pending_slot;
            CHECK(slot>=2 && slot<100);a=&f.spawn.actors[slot];++pickup_spawns;
            CHECK(a->entity.type==0xe1 && a->owned && a->entity.owner==&f.world);
            CHECK(a->entity.body.x==(int32_t)f.plan.teleport_starts[marker].x*65536);
            CHECK(a->entity.body.y==(int32_t)f.plan.teleport_starts[marker].y*65536);
            CHECK(a->animation==data.schedule[level-12][sequence+1] && a->animation_ticks==6);
            CHECK(!a->script.trigger && !a->activation.state && a->activation.timer==60);
            CHECK(a->entity.body.flags==0x80 && !a->entity.radius && !a->entity.body.height && !a->activation.health);
            CHECK(a->activation.facing==(uint8_t)f.plan.teleport_starts[marker].heading);
            CHECK(!t.runtime.state.elapsed && t.runtime.state.sequence==(2*(i+1))%32);
            CHECK(t.runtime.state.marker==(marker+1)%f.plan.teleport_count);
            CHECK(ssa_entity_timer_step(&a->entity.body.flags,&a->activation.timer,59)==0 && a->activation.timer==1);
            CHECK(ssa_entity_timer_step(&a->entity.body.flags,&a->activation.timer,1)==0 && a->activation.timer==30);
            CHECK(!(a->entity.body.flags&0x80));pickup_timer_steps+=2;
            a->script.flags=a->entity.body.flags;a->script.timer=a->activation.timer;
            CHECK(ssa_script_spawn_release(&f.spawn,slot)==0 && f.world.entity_count==2 && f.spawn.pool.active_count==2);
            if(slot==last_slot)++pickup_reuses;
            last_slot=slot;
        }
        /* Occupancy is any entity at wholeXY, regardless of type, flags orZ. */
        t.runtime.state.elapsed=300;CHECK(pickup_step(&t,1,0)==0);++pickup_spawns;
        {
            unsigned slot=t.runtime.pending_slot,particles=t.particles;
            SsaScriptActor *a=&f.spawn.actors[slot];a->entity.type=0x6b;a->entity.body.z+=123*65536;
            t.runtime.state.marker=(uint16_t)((t.runtime.state.marker+f.plan.teleport_count-1)%f.plan.teleport_count);
            t.runtime.state.elapsed=300;CHECK(pickup_step(&t,1,0)==0 && t.particles==particles);
            CHECK(!t.runtime.state.elapsed && f.spawn.pool.active_count==3);++pickup_occupied;
            CHECK(ssa_script_spawn_release(&f.spawn,slot)==0);
        }
        /* Failed presentation retains inserted pickup and advanced indices. */
        t.fail_particle=1;t.runtime.state.elapsed=300;before=t.runtime.state;
        CHECK(pickup_step(&t,1,0)==-1 && t.runtime.state.elapsed==301);
        CHECK(t.runtime.state.sequence!=before.sequence && f.spawn.pool.active_count==3);++pickup_failures;
        CHECK(ssa_script_spawn_release(&f.spawn,t.runtime.pending_slot)==0);t.fail_particle=0;
        f.world.query_active=1;CHECK(ssa_pickup_respawn_runtime_step(&t.runtime,1,0,&t.ops)==-1);f.world.query_active=0;++pickup_guards;
        t.mp.second.updating=1;CHECK(ssa_pickup_respawn_runtime_step(&t.runtime,1,0,&t.ops)==-1);t.mp.second.updating=0;++pickup_guards;
        CHECK(pickup_step(&t,256,0)==-1);++pickup_guards;
        if(level==12) {
            /* Fill actual sharedpool usingrespawner; move fixture pickups away
             * from all markers with native unlink/insert between requests. */
            for(i=0;i<98;++i) {
                SsaScriptActor *a;unsigned slot;t.runtime.state.elapsed=300;
                CHECK(pickup_step(&t,1,0)==0);slot=t.runtime.pending_slot;CHECK(slot<100);
                a=&f.spawn.actors[slot];CHECK(ssa_world_remove(&f.world,&a->entity)==1);
                a->entity.body.x=f.players[0].entity.body.x;a->entity.body.y=f.players[0].entity.body.y;
                for(j=0;j<f.plan.teleport_count;++j)CHECK(a->entity.body.x!=(int32_t)f.plan.teleport_starts[j].x*65536 || a->entity.body.y!=(int32_t)f.plan.teleport_starts[j].y*65536);
                CHECK(ssa_world_insert(&f.world,&a->entity)==1);++pickup_spawns;
            }
            CHECK(f.spawn.pool.active_count==100 && f.world.entity_count==100);
            t.runtime.state.elapsed=300;CHECK(pickup_step(&t,1,-1234567)==0);
            CHECK(t.runtime.pending_slot==100 && t.last_z==-1234567 && !t.runtime.state.elapsed);++pickup_full;
            for(i=2;i<100;++i)CHECK(ssa_script_spawn_release(&f.spawn,i)==0);
            t.runtime.state.elapsed=300;CHECK(pickup_step(&t,1,0)==0 && t.runtime.pending_slot<100);++pickup_reuses;++pickup_spawns;
            CHECK(ssa_script_spawn_release(&f.spawn,t.runtime.pending_slot)==0);
        }
        /* Single-player and other levels preserve all counters. */
        before=t.runtime.state;t.mp.combat.player_count=1;t.mp.damage.player_count=1;
        CHECK(pickup_step(&t,255,0)==0 && !memcmp(&before,&t.runtime.state,sizeof(before)));
        t.mp.combat.player_count=2;t.mp.damage.player_count=2;f.level.level=0;
        CHECK(pickup_step(&t,255,0)==0 && !memcmp(&before,&t.runtime.state,sizeof(before)));f.level.level=level;
        pickup_cleanup(&t);
    }
    free(rom);
    printf("{\"assertions\":%u,\"maps\":4,\"marker_counts\":[22,17,15,17],\"frames\":%u,\"spawned_pickups\":%u,\"particles\":%u,\"occupied_attempts\":%u,\"full_pool_attempts\":%u,\"slot_reuses\":%u,\"pickup_timer_steps\":%u,\"guards\":%u,\"failures\":%u}\n",
        checks,pickup_frames,pickup_spawns,pickup_particles,pickup_occupied,pickup_full,pickup_reuses,pickup_timer_steps,pickup_guards,pickup_failures);
    return 0;
}
