#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "ssa/actor_animation_runtime.h"
#include "../src/projectile_runtime_internal.h"
static unsigned particle_sounds,particle_cycles,particle_guards,particle_frames,particle_failures;
static int fail_sound;
static unsigned extended_spreads,extended_splits,extended_falls,extended_children;
static int native_sound(void *context,const SsaProjectileRuntime *r,const SsaProjectile *p,unsigned sound) {
    ProjectileCase *t=context;
    CHECK(r==&t->runtime && p->entity->type==0x96 && p->entity->owner==&t->weapon.f->world);
    CHECK(sound>=0x22 && sound<=0x64 && !(p->entity->body.flags&0x20));
    CHECK(ssa_projectile_runtime_particle_held(&t->runtime,0,0,0,0)==-1);
    CHECK(ssa_projectile_runtime_particle(&t->runtime,0,0,0,0,&t->weapon.combat_ops)==-1);
    CHECK(ssa_projectile_runtime_free(&t->runtime)==-1);particle_guards+=3;++particle_sounds;
    return fail_sound?-1:0;
}
static int held(void *context,SsaCombatScope *scope) {
    ProjectileCase *t=context;SsaEntity *source=&t->weapon.f->players[0].entity;
    CHECK(ssa_projectile_runtime_particle_scoped(&t->runtime,scope,1,source->body.x,source->body.y,source->body.z)==0);
    CHECK(ssa_projectile_runtime_particle_held(&t->runtime,2,source->body.x,source->body.y,source->body.z)==0);
    return 0;
}
typedef struct CreateExtended { ProjectileCase *t;unsigned type,heading; } CreateExtended;
static int extended_create(void *context,SsaCombatScope *scope) {
    CreateExtended *c=context;SsaEntity *source=&c->t->weapon.f->players[0].entity;
    SsaProjectileCreateRequest request={c->type,source,0x500000,0,0x400000,c->heading,0};
    return ssa_projectile_runtime_enemy_scoped(&c->t->runtime,scope,&request,NULL,0);
}
static void clear_particles(ProjectileCase *t) {
    unsigned i;for(i=0;i<100;++i)if(t->runtime.active[i])CHECK(ssa_projectile_runtime_release(&t->runtime,i)==0);
}
static void extended_cases(ProjectileCase *t,SsaActorAnimationRuntime *ar) {
    CreateExtended creation={t,0x68,0};unsigned parent,i,frame,direction;
    CHECK(ssa_combat_scope_run(&t->weapon.combat,&t->weapon.combat_ops,extended_create,&creation)==0);
    parent=projectile_first(t);CHECK(parent<100);
    CHECK(projectile_tick(t,parent,255)==0);
    CHECK(t->runtime.projectiles[parent].state==2 && projectile_count(t)==16);++extended_spreads;
    CHECK(ssa_projectile_runtime_release(&t->runtime,parent)==0);
    /* Original source pointers retain stable cleared pool addresses. */
    for(i=0;i<100;++i)if(t->runtime.active[i]) {
        CHECK(t->runtime.projectiles[i].source==&t->weapon.f->spawn.actors[parent].entity);
        CHECK(projectile_tick(t,i,255)==0);++extended_children;
    }
    for(frame=0;frame<128 && projectile_count(t);++frame) {
        unsigned slots[100],count=0;
        for(i=0;i<100;++i)if(t->runtime.active[i])slots[count++]=i;
        for(i=0;i<count;++i)if(t->runtime.active[slots[i]])CHECK(projectile_tick(t,slots[i],8)>=0);
        CHECK(ssa_actor_animation_runtime_step(ar,8)==0);
    }
    CHECK(!projectile_count(t));
    creation.type=0x6a;
    for(direction=0;direction<16;++direction) {
        unsigned before;
        creation.heading=direction*4096;
        CHECK(ssa_combat_scope_run(&t->weapon.combat,&t->weapon.combat_ops,extended_create,&creation)==0);
        parent=projectile_first(t);CHECK(parent<100);
        CHECK(projectile_tick(t,parent,1)>=0);
        if(t->runtime.active[parent] && t->runtime.projectiles[parent].entity->type==0x6a) {
            t->weapon.f->spawn.actors[parent].activation.timer=63;before=projectile_count(t);
            CHECK(projectile_tick(t,parent,1)>=0);
            if(projectile_count(t)>=before+2) {++extended_splits;clear_particles(t);break;}
        }
        clear_particles(t);
    }
    creation.type=0xe3;creation.heading=0;
    CHECK(ssa_combat_scope_run(&t->weapon.combat,&t->weapon.combat_ops,extended_create,&creation)==0);
    parent=projectile_first(t);CHECK(parent<100);
    for(frame=0;frame<128 && t->runtime.active[parent];++frame)CHECK(projectile_tick(t,parent,1)>=0);
    CHECK(!t->runtime.active[parent]);++extended_falls;clear_particles(t);
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inv;SsaWeaponRules rules;SsaWeaponAnimations animations;
    SsaActorAnimations actor_animations={0};SsaParticleRules particles={0};unsigned level,id,frame,slot;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);
    CHECK(size>0 && !fseek(file,0,SEEK_SET));rom=malloc((size_t)size);CHECK(rom);
    CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_inventory_rules_load(&inv,rom,(size_t)size)==0);CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);
    CHECK(ssa_weapon_animations_load(&animations,rom,(size_t)size)==1);
    CHECK(ssa_actor_animations_load(&actor_animations,rom,(size_t)size)==1);
    CHECK(ssa_particle_rules_load(&particles,rom,(size_t)size)==1);
    for(level=0;level<16;++level) {
        Fixture f;ProjectileCase t;SsaActorAnimationRuntime ar={0};SsaActorAnimation player_anim={0};
        SsaEntity *reserved[1];SsaActorAnimation *states[1]={&player_anim};SsaEntity *source;
        setup(&f,rom,(size_t)size,level,&math,&defs,100);projectile_init(&t,&f,&inv,&rules,&animations,&math);
        source=&f.players[0].entity;reserved[0]=source;
        CHECK(ssa_actor_animation_runtime_init(&ar,&f.spawn,&actor_animations,reserved,states,1)==0);
        CHECK(ssa_projectile_runtime_bind_particles(&t.runtime,&particles,native_sound,&t)==0);
        t.ops.particle=NULL;
        CHECK(ssa_projectile_runtime_particle_held(&t.runtime,0,0,0,0)==-1);++particle_guards;
        for(id=0;id<16;++id) {
            CHECK(ssa_projectile_runtime_particle(&t.runtime,id,source->body.x,source->body.y,source->body.z,&t.weapon.combat_ops)==0);
            slot=projectile_first(&t);CHECK(slot<100 && projectile_count(&t)==1);
            CHECK(t.runtime.projectiles[slot].source==NULL && t.runtime.projectiles[slot].entity->type==0x96);
            CHECK(f.spawn.actors[slot].animation==particles.animation[id]);
            CHECK((f.spawn.actors[slot].entity.body.flags&0x2020)==(id>=8 && id<=12?0x2020:0x20));
            for(frame=0;frame<1024 && t.runtime.active[slot];++frame) {
                CHECK(projectile_tick(&t,slot,1)>=0);++particle_frames;
                CHECK(ssa_actor_animation_runtime_step(&ar,1)==0);
            }
            CHECK(!t.runtime.active[slot] && frame>1 && f.world.entity_count==1);++particle_cycles;
        }
        CHECK(ssa_combat_scope_run(&t.weapon.combat,&t.weapon.combat_ops,held,&t)==0);
        CHECK(projectile_count(&t)==2);
        for(slot=0;slot<100;++slot) if(t.runtime.active[slot]) CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
        /* Script VM callbacks have their own valid held-lock phase. */
        f.spawn.updating=f.level.updating=f.level.script.updating=1;
        CHECK(ssa_projectile_runtime_particle_held(&t.runtime,5,source->body.x,source->body.y,source->body.z)==0);
        f.spawn.updating=f.level.updating=f.level.script.updating=0;
        slot=projectile_first(&t);CHECK(slot<100);CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
        fail_sound=1;
        CHECK(ssa_projectile_runtime_particle(&t.runtime,0,source->body.x,source->body.y,source->body.z,&t.weapon.combat_ops)==-1);
        slot=projectile_first(&t);CHECK(slot<100 && t.runtime.projectiles[slot].entity->owner==&f.world);
        CHECK(!(t.runtime.projectiles[slot].entity->body.flags&0x20));
        CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);fail_sound=0;++particle_failures;
        CHECK(ssa_projectile_runtime_particle(&t.runtime,16,0,0,0,&t.weapon.combat_ops)==-1);++particle_guards;
        CHECK(!projectile_count(&t));
        extended_cases(&t,&ar);
        CHECK(ssa_actor_animation_runtime_free(&ar)==0);projectile_cleanup(&t);cleanup(&f);
    }
    free(rom);
    CHECK(extended_splits>0);
    printf("{\"assertions\":%u,\"maps\":16,\"particle_cycles\":%u,\"particle_frames\":%u,\"sounds\":%u,\"guards\":%u,\"failures\":%u,\"spreads\":%u,\"splits\":%u,\"falling_explosions\":%u,\"children_after_parent_release\":%u}\n",
        checks,particle_cycles,particle_frames,particle_sounds,particle_guards,particle_failures,extended_spreads,extended_splits,extended_falls,extended_children);
    return 0;
}
