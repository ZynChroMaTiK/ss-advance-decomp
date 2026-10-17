#include "ssa/native_game.h"
#include "ssa/sector_clip.h"
#include "../src/projectile_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,list_cases;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"sector runtime line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int16_t half(const uint8_t *p){unsigned n=p[0]|(unsigned)p[1]<<8;return (int16_t)(n<32768?(int)n:(int)n-65536);}
static int32_t bits(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
typedef struct Creation {SsaNativeGame *g;SsaProjectile *projectile;} Creation;
static int create(void *context,SsaCombatScope *scope){
    Creation *c=context;SsaProjectileCreateRequest request={0x64,&c->g->players[0].entity,0,0,64*65536,0,0};
    return ssa_projectile_runtime_enemy_create_scoped(&c->g->projectiles,scope,&request,NULL,0,&c->projectile);
}
static void lists(SsaNativeGame *g){
    SsaWorld world={0};SsaEntity a={0},b={0},replacement={0},before;unsigned leaf;int sector=-1;
    int old_sector=g->players[0].entity.sector;size_t cell;
    CHECK(ssa_world_init(&world,&g->map,4)==1);
    for(leaf=0;leaf<g->map.leaf_count;++leaf){sector=half(g->map.data+g->map.leaves+8*leaf+4);if(sector!=old_sector)break;}
    CHECK(leaf<g->map.leaf_count);
    a.body=g->players[0].entity.body;a.radius=16*65536;b.body=a.body;b.radius=a.radius;
    CHECK(ssa_world_insert(&world,&a)==1 && ssa_world_insert(&world,&b)==1);
    cell=b.cell;before=b;
    world.query_active=1;
    CHECK(ssa_world_sector_move(&world,&b,b.body.x,b.body.y,b.body.z,(int)leaf,sector,0)==-1 && !memcmp(&before,&b,sizeof(b)));
    world.query_active=0;
    CHECK(ssa_world_sector_move(&world,&b,b.body.x,b.body.y,b.body.z,(int)leaf,sector,0)==1);
    CHECK(world.sectors[old_sector]==&b && b.sector==sector && world.sectors[sector]==NULL && b.sector_next==&a);
    CHECK(b.cell==cell && world.cells[cell]==&b && b.block_next==&a);
    CHECK(b.body.floor==before.body.floor && b.body.ceiling==before.body.ceiling && b.body.step_adjust==before.body.step_adjust);
    CHECK(ssa_world_replace(&world,&b,&replacement)==1 && world.sectors[old_sector]==&replacement);
    CHECK(ssa_world_remove(&world,&replacement)==1 && world.sectors[old_sector]==&a && !a.sector_previous);
    before=a;
    CHECK(ssa_world_sector_move(&world,&a,a.body.x+65536,a.body.y,a.body.z,(int)leaf,sector,1)==1);
    CHECK(!world.sectors[old_sector] && world.sectors[sector]==&a && a.sector==sector);
    CHECK(a.cell==before.cell && a.body.floor==before.body.floor && world.cells[a.cell]==&a);
    CHECK(ssa_world_remove(&world,&a)==1 && !world.entity_count);
    ssa_world_free(&world);++list_cases;
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned level;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    for(level=0;level<16;++level){
        SsaNativeGame *g=calloc(1,sizeof(*g));Creation c={g,NULL};SsaProjectile *p;unsigned slot,frame;
        CHECK(g && ssa_native_game_init(g,rom,0x800000,level)==1);lists(g);
        CHECK(ssa_native_game_tick(g)==1);g->damage.god_mode=1;
        CHECK(ssa_combat_scope_run(&g->combat,&g->combat_ops,create,&c)==0 && c.projectile);
        p=c.projectile;slot=(unsigned)(p-g->projectiles.projectiles);CHECK(slot<100 && g->projectiles.active[slot]);
        p->entity->body.velocity_x=65536/4;p->entity->body.velocity_y=-65536/4;p->entity->body.velocity_z=65536;
        for(frame=0;frame<17;++frame){
            SsaEntity proposed=*p->entity;SsaSectorClip expected;size_t cell=proposed.cell;
            int32_t floor=proposed.body.floor,ceiling=proposed.body.ceiling;int result;
            if(frame){
                proposed.body.x=bits((uint32_t)proposed.body.x+(uint32_t)proposed.body.velocity_x);
                proposed.body.y=bits((uint32_t)proposed.body.y+(uint32_t)proposed.body.velocity_y);
                proposed.body.z=bits((uint32_t)proposed.body.z+(uint32_t)proposed.body.velocity_z);
            }
            result=ssa_sector_relocate(&g->map,&g->math,&proposed,&expected);CHECK(result>=0);
            if(!frame)CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,0,&g->projectile_ops)==0);
            else CHECK(ssa_native_game_tick(g)==1);
            CHECK(g->projectiles.active[slot] && p->state==1 && p->animation==0xb && p->animation_period==5);
            CHECK(p->entity->body.x==expected.end_x && p->entity->body.y==expected.end_y && p->entity->body.z==expected.end_z);
            CHECK(p->entity->leaf==expected.leaf && p->entity->sector==expected.sector && p->entity->cell==cell);
            CHECK(p->entity->body.floor==floor && p->entity->body.ceiling==ceiling);
            CHECK(!g->error_line);++frames;
        }
        CHECK(ssa_projectile_runtime_release(&g->projectiles,slot)==0 && !g->projectiles.active[slot]);
        ssa_native_game_free(g);
        free(g);
    }
    free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"maps\":16,\"projectile_frames\":%u,\"list_cases\":%u}\n",checks,frames,list_cases);return 0;
}
