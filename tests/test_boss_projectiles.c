#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "../src/projectile_runtime_internal.h"
#include "ssa/native_game.h"

typedef struct BossShot {ProjectileCase *t;SsaEntity *source;unsigned heading;SsaProjectile *out;} BossShot;
static int create_boss_shot(void *context,SsaCombatScope *scope)
{
    BossShot *s=context;SsaProjectileCreateRequest q={0x6a,s->source,64*65536,0,64*65536,s->heading,0};
    return ssa_projectile_runtime_enemy_create_scoped(&s->t->runtime,scope,&q,NULL,0,&s->out);
}
static unsigned fire_boss(ProjectileCase *t,SsaEntity *source)
{
    BossShot s={t,source,0,NULL};unsigned i;
    CHECK(ssa_combat_scope_run(&t->weapon.combat,&t->weapon.combat_ops,create_boss_shot,&s)==0 && s.out);
    for(i=0;i<100;++i)if(s.out==&t->runtime.projectiles[i])break;
    CHECK(i<100);return i;
}
static void position(Fixture *f,SsaEntity *e,int32_t x,int32_t y,int32_t z)
{CHECK(ssa_world_remove(&f->world,e)==1);e->body.x=x;e->body.y=y;e->body.z=z;CHECK(ssa_world_insert(&f->world,e)==1);}
static SsaScriptActor *spawn_enemy(Fixture *f,uint8_t *rom,const SsaSpawnDefinitions *defs,unsigned type)
{
    unsigned thing,i;
    for(thing=0;thing<64;++thing)if(defs->thing[thing].type==type && !defs->thing[thing].variant)break;
    CHECK(thing<64);script(f,rom,thing,1,1);step(f,1);
    for(i=0;i<100;++i)if(f->spawn.actors[i].owned && f->spawn.actors[i].entity.type==type && !f->spawn.actors[i].activation.state)break;
    CHECK(i<100);
    for(unsigned j=0;j<4 && !f->spawn.actors[i].activation.state;++j)CHECK(ssa_script_spawn_activate(&f->spawn,i,100,&f->ops)>=0);
    CHECK(f->spawn.actors[i].activation.state==1);return &f->spawn.actors[i];
}
static int boss_particle(void *ctx,const SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z)
{(void)ctx;(void)r;(void)x;(void)y;(void)z;CHECK(id==13 || id==14);return 0;}
static void reset_health(SsaScriptActor *a)
{a->activation.health=3000;a->entity.body.flags&=UINT16_C(0xfffe);}
int main(int argc,char **argv)
{
    FILE *file;uint8_t *rom=malloc(0x800000);SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations animations;unsigned type,blast_cases=0,children=0;
    CHECK(argc==2 && rom);file=fopen(argv[1],"rb");CHECK(file && fread(rom,1,0x800000,file)==0x800000);fclose(file);
    CHECK(ssa_math_tables_load(&math,rom,0x800000) && ssa_spawn_definitions_load(&defs,rom,0x800000));
    CHECK(ssa_inventory_rules_load(&inventory,rom,0x800000)==0 && ssa_weapon_rules_load(&rules,rom,0x800000)==1);
    CHECK(ssa_weapon_animations_load(&animations,rom,0x800000)==1);
    for(type=0xd7;type<=0xd8;++type){
        Fixture *f=calloc(1,sizeof(*f));ProjectileCase *t=calloc(1,sizeof(*t));SsaScriptActor *boss,*other;
        int32_t px,py,cx=0,cy=0,z;unsigned found=0,slot,child=100,grandchild=100;
        CHECK(f && t);setup(f,rom,0x800000,0,&math,&defs,100);projectile_init(t,f,&inventory,&rules,&animations,&math);
        t->ops.particle=boss_particle;t->weapon.damage.god_mode=0;
        boss=spawn_enemy(f,rom,&defs,type);other=spawn_enemy(f,rom,&defs,type==0xd7?0xd8:0xd7);
        px=f->players[0].entity.body.x;py=f->players[0].entity.body.y;
        for(int y=-2048;y<=2048 && !found;y+=128)for(int x=-2048;x<=2048 && !found;x+=128){
            int32_t tx=px+x*65536,ty=py+y*65536;
            tx=f->world.origin_x+((tx-f->world.origin_x)/(128*65536))*128*65536+64*65536;
            ty=f->world.origin_y+((ty-f->world.origin_y)/(128*65536))*128*65536+64*65536;
            if((abs(x)<512 && abs(y)<512) || ssa_map_query(&f->world.collision,tx,ty,tx,ty,192*65536)!=1 ||
               f->world.collision.ceiling-f->world.collision.floor<186)continue;
            position(f,&boss->entity,tx,ty,0);boss->entity.body.z=(int32_t)boss->entity.body.floor*65536;
            {SsaEntity probe={0};probe.body.x=tx;probe.body.y=ty+64*65536;probe.body.z=boss->entity.body.z+64*65536;
                probe.radius=probe.body.height=32*65536;probe.body.flags=0x104;
                if(ssa_world_query(&f->world,&probe,tx,ty+76*65536)!=0 || f->world.blocking_entity!=&boss->entity)continue;}
            cx=tx;cy=ty;found=1;
        }
        CHECK(found);position(f,&boss->entity,cx,cy,0);z=(int32_t)boss->entity.body.floor*65536;boss->entity.body.z=z;
        position(f,&other->entity,px,py,z);
        slot=fire_boss(t,&boss->entity);
        /* Reproduce a genuine source collision before the PC exclusion. */
        {SsaProjectile *p=&t->runtime.projectiles[slot];
            CHECK(ssa_world_query(&f->world,p->entity,p->entity->body.x,p->entity->body.y+12*65536)==0);
            CHECK(f->world.blocking_entity==&boss->entity);
            reset_health(boss);
            CHECK(ssa_combat_runtime_damage(&t->weapon.combat,&boss->entity,p->entity,p->source,30,&t->weapon.combat_ops)==0);
            CHECK(boss->activation.health<3000);reset_health(boss);
            t->weapon.combat.boss_projectile_immunity=1;
            CHECK(ssa_combat_runtime_damage(&t->weapon.combat,&boss->entity,p->entity,p->source,10000,&t->weapon.combat_ops)==0);
            CHECK(boss->activation.health==3000 && !(boss->entity.body.flags&1));
            CHECK(projectile_tick(t,slot,1)==0 && t->runtime.active[slot] && boss->activation.health==3000);
        }
        /* Children and grandchildren must keep the boss identity even when
         * their parent slots are released and reused for unrelated shots. */
        f->spawn.actors[slot].activation.timer=63;
        CHECK(projectile_tick(t,slot,1)==0);
        for(unsigned i=0;i<100;++i)if(t->runtime.active[i] && i!=slot){
            CHECK(t->runtime.projectiles[i].source==&boss->entity);child=i;++children;}
        CHECK(child<100);f->spawn.actors[child].activation.timer=63;
        CHECK(projectile_tick(t,child,1)==0);
        for(unsigned i=0;i<100;++i)if(t->runtime.active[i] && i!=slot && i!=child){
            CHECK(t->runtime.projectiles[i].source==&boss->entity);grandchild=i;}
        CHECK(grandchild<100);CHECK(ssa_projectile_runtime_release(&t->runtime,slot)==0);
        CHECK(ssa_projectile_runtime_release(&t->runtime,child)==0);
        {unsigned reused=fire_boss(t,&other->entity);CHECK(reused==slot || reused==child);
            CHECK(t->runtime.projectiles[grandchild].source==&boss->entity);}
        for(unsigned i=0;i<100;++i)if(t->runtime.active[i] && i!=grandchild)CHECK(ssa_projectile_runtime_release(&t->runtime,i)==0);
        /* Return into the firing boss, then force a real expiry explosion in
         * its splash radius with a second boss and the player also nearby. */
        {SsaProjectile *p=&t->runtime.projectiles[grandchild];
            position(f,p->entity,cx,cy+96*65536,z+64*65536);
            p->entity->body.velocity_x=0;p->entity->body.velocity_y=-12*65536;p->entity->body.velocity_z=0;
            CHECK(projectile_tick(t,grandchild,1)==0 && p->entity->body.y==cy+84*65536);
            position(f,&other->entity,cx+96*65536,cy+84*65536,z);reset_health(other);reset_health(boss);
            position(f,&f->players[0].entity,cx-96*65536,cy+84*65536,z);
            f->players[0].entity.body.flags&=UINT16_C(0xfffe);f->players[0].inventory.health=1000;
            f->players[0].inventory.stat_c4=f->players[0].inventory.stat_c6=0;
            f->spawn.actors[grandchild].activation.timer=481;
            CHECK(projectile_tick(t,grandchild,1)==1 && !t->runtime.active[grandchild]);
            CHECK(boss->activation.health==3000 && !(boss->entity.body.flags&1));
            CHECK(other->activation.health<3000 && f->players[0].inventory.health<1000);++blast_cases;
        }
        /* Player projectiles still hurt the boss, and player self-splash is
         * unaffected by the new enemy-only immunity. */
        slot=fire_boss(t,&f->players[0].entity);
        CHECK(ssa_combat_runtime_damage(&t->weapon.combat,&boss->entity,t->runtime.projectiles[slot].entity,
            &f->players[0].entity,30,&t->weapon.combat_ops)==0 && boss->activation.health<3000);
        f->players[0].entity.body.flags&=UINT16_C(0xfffe);f->players[0].inventory.health=1000;
        CHECK(ssa_combat_runtime_explode(&t->weapon.combat,t->runtime.projectiles[slot].entity,t->runtime.projectiles[slot].entity,
            &f->players[0].entity,0x20000,30,1,&t->weapon.combat_ops)==0 && f->players[0].inventory.health<1000);
        projectile_cleanup(t);cleanup(f);free(t);free(f);
    }
    {SsaNativeGame *g=calloc(1,sizeof(*g));CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
        CHECK(g->combat.boss_projectile_immunity);ssa_native_game_free(g);free(g);}
    free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"boss_types\":2,\"self_blast_cases\":%u,\"children\":%u}\n",checks,blast_cases,children);return 0;
}
