#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "../src/projectile_runtime_internal.h"

typedef struct GunShot { ProjectileCase *test;SsaEntity *source;unsigned heading;SsaProjectile *out; } GunShot;
static int gun_create(void *context,SsaCombatScope *scope) {
    GunShot *shot=context;
    SsaProjectileCreateRequest q={0x67,shot->source,80*65536,0,80*65536,shot->heading,0};
    return ssa_projectile_runtime_enemy_create_scoped(&shot->test->runtime,scope,&q,NULL,1,&shot->out);
}
static unsigned gun_fire(ProjectileCase *t,SsaScriptActor *a,unsigned heading) {
    GunShot shot={t,&a->entity,heading,NULL};unsigned slot;
    a->entity.body.flags&=UINT16_C(0xfffe);a->activation.health=150;
    a->entity.body.velocity_x=a->entity.body.velocity_y=a->entity.body.velocity_z=0;
    CHECK(ssa_combat_scope_run(&t->weapon.combat,&t->weapon.combat_ops,gun_create,&shot)==0);
    CHECK(shot.out && shot.out->source==&a->entity && shot.out->owner==0 && shot.out->speed==16*65536);
    for(slot=0;slot<100;++slot)if(shot.out==&t->runtime.projectiles[slot])break;
    CHECK(slot<100 && t->runtime.active[slot]);return slot;
}
static void place(Fixture *f,SsaEntity *e,int32_t x,int32_t y) {
    CHECK(ssa_world_remove(&f->world,e)==1);e->body.x=x;e->body.y=y;
    CHECK(ssa_world_insert(&f->world,e)==1);e->body.z=(int32_t)e->body.floor*65536;
}
int main(int argc,char **argv) {
    FILE *file;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations animations;
    Fixture *f=calloc(1,sizeof(*f));ProjectileCase *t=calloc(1,sizeof(*t));SsaScriptActor *gun,*other;
    unsigned thing,slot,i,dir,d,found=0,escapes=0,return_hits=0,wall_hits=0,player_hits=0,enemy_hits=0;
    int32_t cx=0,cy=0,px,py;unsigned old_particles;
    static const int dx[8]={0,1,1,1,0,-1,-1,-1},dy[8]={1,1,0,-1,-1,-1,0,1};
    CHECK(argc==2 && f && t);file=fopen(argv[1],"rb");CHECK(file);rom=malloc(0x800000);CHECK(rom);
    CHECK(fread(rom,1,0x800000,file)==0x800000);fclose(file);
    CHECK(ssa_math_tables_load(&math,rom,0x800000));CHECK(ssa_spawn_definitions_load(&defs,rom,0x800000));
    CHECK(ssa_inventory_rules_load(&inventory,rom,0x800000)==0);
    CHECK(ssa_weapon_rules_load(&rules,rom,0x800000)==1);CHECK(ssa_weapon_animations_load(&animations,rom,0x800000)==1);
    setup(f,rom,0x800000,0,&math,&defs,100);projectile_init(t,f,&inventory,&rules,&animations,&math);
    t->runtime.ballistic_player_cannon=1; /* PC fix must preserve enemy cannonballs. */
    t->runtime.rolling_cannonballs=1;
    for(thing=0;thing<64;++thing)if(defs.thing[thing].type==0xd6 && !defs.thing[thing].variant)break;
    CHECK(thing<64);script(f,rom,thing,1,1);step(f,1);gun=&f->spawn.actors[first_owned(f)];
    for(i=0;i<4 && !gun->activation.state;++i)CHECK(ssa_script_spawn_activate(&f->spawn,(unsigned)(gun-f->spawn.actors),100,&f->ops)>=0);
    CHECK(gun->activation.state==1 && gun->entity.radius==90*65536 && gun->entity.body.height==138*65536);
    px=f->players[0].entity.body.x;py=f->players[0].entity.body.y;
    /* Find a real, tall, clear courtyard away from the player. */
    for(d=256;d<=2048 && !found;d+=64)for(dir=0;dir<8 && !found;++dir){
        int32_t x=px+dx[dir]*(int32_t)d*65536,y=py+dy[dir]*(int32_t)d*65536;
        /* Keep the large shooter in the original32-unit broadphase margin
         * for every heading; its physical bounds exceed that old margin. */
        x=f->world.origin_x+((x-f->world.origin_x)/(128*65536))*128*65536+64*65536;
        y=f->world.origin_y+((y-f->world.origin_y)/(128*65536))*128*65536+64*65536;
        if(ssa_map_query(&f->world.collision,x,y,x,y,144*65536)!=1 ||
           f->world.collision.ceiling-f->world.collision.floor<138)continue;
        place(f,&gun->entity,x,y);found=1;
        /* Clearance must include actual floor/ceiling commits, not just a
         * wall query: the map's open courtyard also contains raised steps. */
        {unsigned heading,n;
            for(heading=0;heading<8 && found;++heading){SsaBody probe={0};int32_t sine,cosine;
                ssa_sincos(&math,(uint16_t)(heading*8192),&sine,&cosine);
                probe.x=x+80*sine;probe.y=y+80*cosine;probe.z=gun->entity.body.z+80*65536;
                probe.height=16*65536;probe.flags=0x104;
                for(n=0;n<8 && llabs((long long)probe.x-x)<106*65536 && llabs((long long)probe.y-y)<106*65536;++n)
                    if(ssa_map_try_move(&f->world.collision,&probe,16*65536,probe.x+16*sine,probe.y+16*cosine)!=1 ||
                       ssa_vertical_step(&probe,NULL,1)!=1){found=0;break;}
            }
        }
        if(found){cx=x;cy=y;}
    }
    CHECK(found);t->weapon.damage.god_mode=0;f->players[0].inventory.health=1000;
    for(dir=0;dir<8;++dir){
        SsaProjectile *p;uint16_t flags;int32_t x,y;
        slot=gun_fire(t,gun,dir*8192);p=&t->runtime.projectiles[slot];flags=gun->entity.body.flags;
        x=p->entity->body.x+p->entity->body.velocity_x;y=p->entity->body.y+p->entity->body.velocity_y;
        {int hit=ssa_world_query(&f->world,p->entity,x,y);
            if(hit!=0 || f->world.blocking_entity!=&gun->entity)
                fprintf(stderr,"Gunrilla reproduction heading%u: query%d sourceflags%x directcontact%d\n",dir,hit,
                    gun->entity.body.flags,ssa_entity_contact(p->entity,&gun->entity,x,y));
            CHECK(hit==0 && f->world.blocking_entity==&gun->entity);
        }
        old_particles=projectile_particles;
        for(i=0;i<16 && ssa_entity_contact(p->entity,&gun->entity,p->entity->body.x,p->entity->body.y)==0;++i){
            int result;int32_t z=p->entity->body.z;int16_t floor=p->entity->body.floor,ceiling=p->entity->body.ceiling;
            x=p->entity->body.x;y=p->entity->body.y;
            result=projectile_tick(t,slot,1);
            if(result!=0 || !t->runtime.active[slot])fprintf(stderr,
                "Gunrilla flight heading%u tick%u result%d at(%d,%d,%d) source(%d,%d) floor%d ceil%d blocker%u\n",
                dir,i,result,x/65536,y/65536,z/65536,gun->entity.body.x/65536,gun->entity.body.y/65536,
                floor,ceiling,f->world.blocking_entity?f->world.blocking_entity->type:0);
            CHECK(result==0 && t->runtime.active[slot]);
            CHECK(p->entity->body.x!=x || p->entity->body.y!=y);
            CHECK(p->entity->body.z==z && !p->entity->body.velocity_z);
            CHECK(gun->entity.body.flags==flags && gun->activation.health==150 && projectile_particles==old_particles);
        }
        CHECK(i>0 && i<16);++escapes;
        /* Immunity ends after escape: returning into the shooter still hits. */
        p->entity->body.velocity_x=-p->entity->body.velocity_x;p->entity->body.velocity_y=-p->entity->body.velocity_y;
        CHECK(projectile_tick(t,slot,1)==1 && !t->runtime.active[slot]);
        CHECK(projectile_particles==old_particles+1);++return_hits;
    }
    /* A second enemy at the muzzle remains solid while the shooter is ignored. */
    for(thing=0;thing<64;++thing)if(defs.thing[thing].type==0xcd && !defs.thing[thing].variant)break;
    CHECK(thing<64);script(f,rom,thing,1,1);step(f,1);
    for(i=1;i<100;++i)if(f->spawn.actors[i].owned && &f->spawn.actors[i]!=gun)break;
    CHECK(i<100);other=&f->spawn.actors[i];
    for(d=0;d<4 && !other->activation.state;++d)CHECK(ssa_script_spawn_activate(&f->spawn,i,100,&f->ops)>=0);
    CHECK(other->activation.health==30);
    slot=gun_fire(t,gun,0);
    place(f,&other->entity,t->runtime.projectiles[slot].entity->body.x,
        t->runtime.projectiles[slot].entity->body.y+16*65536);
    CHECK(projectile_tick(t,slot,1)==1 && !t->runtime.active[slot]);
    CHECK(other->activation.health<30);++enemy_hits;CHECK(ssa_script_spawn_release(&f->spawn,i)==0);
    /* A player at that same point also blocks and takes actual combat damage. */
    f->players[0].entity.body.flags&=UINT16_C(0xfffe);f->players[0].inventory.health=1000;
    f->players[0].inventory.stat_c4=f->players[0].inventory.stat_c6=0;
    slot=gun_fire(t,gun,0);
    place(f,&f->players[0].entity,t->runtime.projectiles[slot].entity->body.x,
        t->runtime.projectiles[slot].entity->body.y+16*65536);
    CHECK(projectile_tick(t,slot,1)==1 && !t->runtime.active[slot]);
    CHECK(f->players[0].inventory.health<1000);++player_hits;place(f,&f->players[0].entity,px,py);
    /* Find a real wall just beyond the muzzle, with the full shooter clear. */
    for(dir=0;dir<8 && !wall_hits;++dir){
        int32_t x=cx,y=cy;
        for(d=8;d<8192;d+=8){int32_t nx=cx+dx[dir]*(int32_t)d*65536,ny=cy+dy[dir]*(int32_t)d*65536;
            if(ssa_map_query(&f->world.collision,x,y,nx,ny,90*65536)!=1)break;
            x=nx;y=ny;
        }
        if(d==8192)continue;
        place(f,&gun->entity,x,y);
        slot=gun_fire(t,gun,dir*8192);
        {SsaEntity *e=t->runtime.projectiles[slot].entity;
            if(ssa_map_query(&f->world.collision,e->body.x,e->body.y,e->body.x+e->body.velocity_x,
                e->body.y+e->body.velocity_y,e->radius)!=0){CHECK(ssa_projectile_runtime_release(&t->runtime,slot)==0);continue;}
        }
        CHECK(projectile_tick(t,slot,1)==1 && !t->runtime.active[slot]);
        CHECK(f->world.blocking_entity==NULL);++wall_hits;
    }
    CHECK(wall_hits==1 && escapes==8 && return_hits==8 && player_hits==1 && enemy_hits==1);
    projectile_cleanup(t);cleanup(f);free(t);free(f);free(rom);
    printf("{\"result\":\"pass\",\"assertions\":%u,\"launch_headings\":%u,\"return_hits\":%u,\"wall_hits\":%u,\"player_hits\":%u,\"enemy_hits\":%u,\"projectile_frames\":%u}\n",
        checks,escapes,return_hits,wall_hits,player_hits,enemy_hits,projectile_frames);return 0;
}
