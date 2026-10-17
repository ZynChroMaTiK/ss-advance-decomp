#include "ssa/native_game.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define U 65536
static unsigned checks,cases,frames,separations,real_frames,native_frames,native_moves,player_frames;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"enemy crowd line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static int64_t absolute(int64_t n){return n<0?-n:n;}
static void put16(uint8_t *p,int n){p[0]=(uint8_t)n;p[1]=(uint8_t)((unsigned)n>>8);}
typedef struct Room {uint8_t data[4096];SsaMapAssets map;SsaWorld world;} Room;
static void room(Room *r)
{
    static const int xy[4][2]={{-512,-512},{512,-512},{512,512},{-512,512}};unsigned i;
    memset(r,0,sizeof(*r));r->map.data=r->data;r->map.size=sizeof(r->data);
    r->map.node_count=r->map.leaf_count=r->map.sector_count=1;r->map.vertex_count=r->map.line_count=4;
    r->map.nodes=80;r->map.leaves=112;r->map.sectors=120;r->map.vertices=140;r->map.lines=160;r->map.blockmap=400;
    put16(r->data+0x1a,266);
    put16(r->data+80+24,0x8000);put16(r->data+80+26,0x8000);put16(r->data+122,512);
    for(i=0;i<4;++i){unsigned j=(i+1)%4;uint8_t *l=r->data+160+i*40;
        put16(r->data+140+i*4,xy[i][0]);put16(r->data+142+i*4,xy[i][1]);
        put16(l,(int)i);put16(l+2,(int)j);put16(l+4,xy[j][0]-xy[i][0]);put16(l+6,xy[j][1]-xy[i][1]);
        put16(l+16,xy[i][1]>xy[j][1]?xy[i][1]:xy[j][1]);put16(l+18,xy[i][1]<xy[j][1]?xy[i][1]:xy[j][1]);
        put16(l+20,xy[i][0]<xy[j][0]?xy[i][0]:xy[j][0]);put16(l+22,xy[i][0]>xy[j][0]?xy[i][0]:xy[j][0]);
        put16(l+24,xy[i][0]==xy[j][0]);
    }
    put16(r->data+400,-1024);put16(r->data+402,-1024);put16(r->data+404,16);put16(r->data+406,16);
    for(i=0;i<256;++i)put16(r->data+408+i*2,260);
    for(i=0;i<4;++i)put16(r->data+400+522+i*2,(int)i);
    put16(r->data+400+530,0xffff);CHECK(ssa_world_init(&r->world,&r->map,100));
}
static void body(SsaWorld *w,SsaEntity *e,int x,int y,unsigned type)
{
    memset(e,0,sizeof(*e));e->body.x=x*U;e->body.y=y*U;e->body.height=120*U;
    e->body.flags=(uint16_t)(type==0x64?0x10:2);e->radius=32*U;e->type=(uint16_t)type;
    CHECK(ssa_world_insert(w,e)==1);
}
static int toward(SsaEnemyCrowd *s,SsaWorld *w,SsaEntity *e,int x,int y)
{
    int64_t dx=(int64_t)x*U-e->body.x,dy=(int64_t)y*U-e->body.y;
    int32_t ox=e->body.x,oy=e->body.y;int result;
    if(dx>2*U)dx=2*U;
    if(dx< -2*U)dx=-2*U;
    if(dy>2*U)dy=2*U;
    if(dy< -2*U)dy=-2*U;
    result=ssa_enemy_crowd_move(s,w,e,(int32_t)(ox+dx),(int32_t)(oy+dy));CHECK(result>=0);
    CHECK(absolute((int64_t)e->body.x-ox)<=2*U && absolute((int64_t)e->body.y-oy)<=2*U);
    CHECK(!s->updating && !w->query_active && e->owner==w);++frames;return result;
}
static void pair_clear(const SsaEntity *a,const SsaEntity *b)
{CHECK(ssa_entity_contact(a,b,a->body.x,a->body.y)==1);}
static void scenarios(const SsaMathTables *math)
{
    Room r;SsaEntity a,b,c;SsaEnemyCrowd sa={0},sb={0};unsigned i;
    room(&r);body(&r.world,&a,0,0,0xcd);body(&r.world,&b,0,64,0xd6);
    CHECK(ssa_world_try_move(&r.world,&a,0,2*U)==0);
    for(i=0;i<200;++i){toward(&sa,&r.world,&a,0,256);pair_clear(&a,&b);}
    CHECK(a.body.y>200*U && absolute(a.body.x)<80*U);++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,0,-160,0xcd);body(&r.world,&b,0,160,0xd2);
    for(i=0;i<260;++i){toward(&sa,&r.world,&a,0,160);toward(&sb,&r.world,&b,0,-160);pair_clear(&a,&b);}
    CHECK(a.body.y>100*U && b.body.y< -100*U);++cases;ssa_world_free(&r.world);
    /* Existing coincident spawns separate gradually; no already clear pair
     * may gain overlap, and no separating axis may shrink during overlap. */
    {SsaEntity es[3];SsaEnemyCrowd states[3]={{0}};unsigned j,k;
        room(&r);for(j=0;j<3;++j)body(&r.world,&es[j],0,0,0xcd+j);
        for(i=0;i<200;++i)for(j=0;j<3;++j){int64_t x[3],y[3];int old[3];
            for(k=0;k<3;++k){x[k]=absolute((int64_t)es[j].body.x-es[k].body.x);
                y[k]=absolute((int64_t)es[j].body.y-es[k].body.y);old[k]=ssa_entity_contact(&es[j],&es[k],es[j].body.x,es[j].body.y);}
            toward(&states[j],&r.world,&es[j],0,384);
            for(k=0;k<3;++k)if(k!=j && !ssa_entity_contact(&es[j],&es[k],es[j].body.x,es[j].body.y)){
                CHECK(!old[k]);CHECK(absolute((int64_t)es[j].body.x-es[k].body.x)>=x[k]);
                CHECK(absolute((int64_t)es[j].body.y-es[k].body.y)>=y[k]);++separations;
            }
        }
        for(j=0;j<3;++j){CHECK(es[j].body.y>96*U);for(k=j+1;k<3;++k)pair_clear(&es[j],&es[k]);}
        ++cases;ssa_world_free(&r.world);
    }
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,-480,0,0xcd);body(&r.world,&b,-480,64,0xce);
    CHECK(toward(&sa,&r.world,&a,-480,256)==1 && a.body.x==-478*U);
    for(i=0;i<180;++i){toward(&sa,&r.world,&a,-480,256);CHECK(a.body.x>=-480*U);pair_clear(&a,&b);}
    CHECK(a.body.y>128*U);++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,-480,-480,0xcd);
    body(&r.world,&b,-480,-416,0xce);body(&r.world,&c,-416,-480,0xcf);
    CHECK(toward(&sa,&r.world,&a,-480,256)==0);CHECK(a.body.x==-480*U && a.body.y==-480*U);
    CHECK(r.world.blocking_entity==&b);++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,0,0,0xcd);body(&r.world,&b,0,64,0x64);
    CHECK(toward(&sa,&r.world,&a,0,256)==0 && r.world.blocking_entity==&b);
    {uint32_t walks=0;SsaEnemyMotion m={&a,2*U,128,128,0};SsaBody expected=a.body;
        a.body.z=expected.z=64*U;CHECK(ssa_vertical_step(&expected,NULL,1)>=0);
        CHECK(ssa_enemy_walk_world_sliding(&m,math,1,&walks,&r.world,&sa)==0 && walks==1);
        CHECK(a.body.z==expected.z && a.body.velocity_z==expected.velocity_z && !a.body.x && !a.body.y);
    }
    ++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,-160,0,0xcd);body(&r.world,&b,0,0,0x64);
    CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,1000*U,0)==1);CHECK(a.body.x<=-64*U);pair_clear(&a,&b);
    ++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,0,0,0xcd);
    CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,1000*U,0)==1 && a.body.x<=480*U);
    ++cases;ssa_world_free(&r.world);
    room(&r);memset(&sa,0,sizeof(sa));body(&r.world,&a,0,0,0xcd);body(&r.world,&b,-129,0,0xd6);b.radius=90*U;
    CHECK(ssa_world_query(&r.world,&a,-8*U,0)==1); /* Original32-unit broadphase. */
    CHECK(ssa_world_try_move_crowd(&r.world,&a,-8*U,0)==0 && r.world.blocking_entity==&b);
    CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,-8*U,0)==1);pair_clear(&a,&b);
    sa.updating=1;CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,0,0)==-1);sa.updating=0;
    r.world.query_active=1;CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,0,0)==-1);r.world.query_active=0;
    CHECK(ssa_enemy_crowd_move(&sa,&r.world,&a,a.body.x,a.body.y)==1);
    ++cases;ssa_world_free(&r.world);
}
static void real_maps(const uint8_t *rom,const SsaMathTables *math)
{
    unsigned level,i,j;uint32_t walks=0;
    for(level=0;level<16;++level){SsaMapAssets map={0};SsaWorld w={0};SsaPlayerStart start;
        SsaEntity es[3];SsaEnemyCrowd states[3]={{0}};
        CHECK(ssa_map_assets_load(&map,rom,0x800000,level));CHECK(ssa_map_player_start(&map,&start));CHECK(ssa_world_init(&w,&map,100));
        for(j=0;j<3;++j){body(&w,&es[j],start.x/U,start.y/U,0xcd+(level+j)%12);es[j].body.z=(int32_t)es[j].body.floor*U;}
        for(i=0;i<128;++i)for(j=0;j<3;++j){SsaEnemyMotion m={&es[j],2*U,(uint8_t)(32*(i/16+j)),0,0};
            uint32_t before=walks;int result=ssa_enemy_walk_world_sliding(&m,math,1,&walks,&w,&states[j]);
            CHECK(result>=0 && walks==before+1 && !m.updating && !states[j].updating && !w.query_active);
            CHECK(es[j].owner==&w && es[j].sector>=0 && (unsigned)es[j].sector<map.sector_count);++real_frames;
        }
        CHECK(w.entity_count==3);ssa_world_free(&w);ssa_map_assets_free(&map);
    }
}
static void player_clearance(const SsaMathTables *math)
{
    Room r;SsaEntity a,b,c;SsaSlide slide={0};unsigned i;
    room(&r);body(&r.world,&a,0,0,0x64);body(&r.world,&b,-129,0,0xd6);b.radius=90*U;
    CHECK(ssa_world_try_move(&r.world,&a,-8*U,0)==1); /* Reproduce the original missed large body. */
    CHECK(ssa_world_remove(&r.world,&a)==1);a.body.x=0;CHECK(ssa_world_insert(&r.world,&a)==1);
    CHECK(ssa_world_try_move_player(&r.world,&a,-8*U,0)==0 && r.world.blocking_entity==&b);
    CHECK(!a.body.x && !a.body.y && r.world.entity_count==2);
    CHECK(ssa_world_remove(&r.world,&b)==1);body(&r.world,&c,-129,0,0xe1);
    CHECK(ssa_world_try_move_player(&r.world,&a,-8*U,0)==1 && !r.world.special_count);
    ++cases;ssa_world_free(&r.world);
    room(&r);body(&r.world,&a,64,0,0x64);body(&r.world,&b,0,0,0xd6);b.radius=90*U;
    CHECK(ssa_world_try_move(&r.world,&a,72*U,0)==0);
    CHECK(ssa_world_try_move_player(&r.world,&a,60*U,0)==0);
    CHECK(ssa_world_try_move_player(&r.world,&a,-64*U,0)==0);
    CHECK(ssa_world_try_move_player(&r.world,&a,60*U,8*U)==0);
    CHECK(ssa_world_try_move_player(&r.world,&a,64*U,0)==0);
    body(&r.world,&c,130,70,0xcd);
    CHECK(ssa_world_try_move_player(&r.world,&a,72*U,8*U)==0 && r.world.blocking_entity==&c);
    CHECK(ssa_world_remove(&r.world,&c)==1);
    CHECK(ssa_slide_init(&slide,&r.map));
    for(i=0;i<8;++i){int32_t before=a.body.x;a.body.flags|=8;a.body.velocity_x=8*U;
        CHECK(ssa_world_horizontal_step_player(&r.world,&a,&slide,math,1,1,0)==0);
        CHECK(a.body.x==before+8*U && !a.body.y && a.owner==&r.world && r.world.entity_count==2);
    }
    pair_clear(&a,&b);CHECK(ssa_world_try_move_player(&r.world,&a,120*U,0)==0);
    CHECK(ssa_world_try_move_player(&r.world,&b,0,0)==-1);
    r.world.query_active=1;CHECK(ssa_world_try_move_player(&r.world,&a,136*U,0)==-1);r.world.query_active=0;
    CHECK(ssa_world_remove(&r.world,&a)==1);a.body.x=480*U;CHECK(ssa_world_insert(&r.world,&a)==1);
    CHECK(ssa_world_try_move_player(&r.world,&a,488*U,0)==0 && a.body.x==480*U);
    ssa_slide_free(&slide);++cases;ssa_world_free(&r.world);
}
static void native_input(const uint8_t *rom)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i,j,cd=0;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(g->enemies.crowd_slide==1 && g->game.enemy_crowd_slide==1);
    g->damage.god_mode=1;ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
    for(i=0;i<512;++i){int32_t x[100],y[100];
        for(j=0;j<100;++j){x[j]=g->spawn.actors[j].entity.body.x;y[j]=g->spawn.actors[j].entity.body.y;}
        if(i==128)ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
        if(i==192){ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);}
        CHECK(ssa_native_game_tick(g)==1 && !g->error_line);++native_frames;
        for(j=0;j<100;++j)if(g->spawn.actors[j].owned && g->spawn.actors[j].entity.type>=0xcd && g->spawn.actors[j].entity.type<=0xd8){
            if(g->spawn.actors[j].entity.body.x!=x[j] || g->spawn.actors[j].entity.body.y!=y[j])++native_moves;
            if(g->entities.enemies[j]){CHECK(g->entities.enemies[j]->crowd_slide==1);++cd;}
        }
    }
    CHECK(native_moves>0 && cd>0);ssa_native_game_free(g);free(g);
}
/* Presentation/hazard boundaries are controlled in this isolated movement
 * fixture; complete native-game scheduling remains in native_input above. */
static int fixture_gameplay(void *p,SsaPlayerRuntime *r,enum SsaPlayerFrameStage s){(void)p;(void)r;(void)s;return 0;}
static int fixture_sector(void *p,SsaPlayerRuntime *r,int16_t s){(void)p;(void)r;(void)s;return 0;}
static int fixture_animation(void *p,SsaPlayerRuntime *r,unsigned a,unsigned n){(void)p;(void)r;(void)a;(void)n;return 0;}
static int fixture_sound(void *p,SsaPlayerRuntime *r,unsigned s){(void)p;(void)r;(void)s;return 0;}
static void native_player_clearance(const uint8_t *rom)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaEntity enemy={0};int32_t x,y;unsigned i;
    SsaPlayerRuntimeInput input={0};SsaPlayerRuntimeOps ops={fixture_gameplay,fixture_sector,fixture_animation,fixture_sound,NULL};
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
    x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
    /* Controlled overlapping spawn, followed only by ordinary PC input. No
     * player relocation, health grant, collision flag change or enemy push. */
    enemy.type=0xd6;enemy.radius=90*U;enemy.body.height=186*U;enemy.body.flags=2;
    enemy.body.x=x-64*U;enemy.body.y=y;enemy.body.z=g->players[0].entity.body.z;
    CHECK(ssa_world_insert(&g->world,&enemy)==1);
    CHECK(ssa_entity_contact(&g->players[0].entity,&enemy,x,y)==0);
    CHECK(ssa_world_try_move(&g->world,&g->players[0].entity,x+8*U,y)==0);
    input.motion.ticks=1;input.motion.player_count=1;input.controls_enabled=1;input.keys=0x40;
    input.yaw_delta=(int16_t)(16384-g->players[0].motion.movement.heading);
    for(i=0;i<16;++i){int32_t before=g->players[0].entity.body.x;
        input.jump_down=(uint8_t)(i==4);input.camera_counter=(uint16_t)i;
        if(i==8)CHECK(ssa_player_runtime_key(&g->player,'H',1,0,1));
        CHECK(ssa_player_runtime_step(&g->player,&input,&ops)==0);++player_frames;input.yaw_delta=0;
        CHECK(g->players[0].entity.body.x>before && g->players[0].entity.body.y==y);
        CHECK(enemy.body.x==x-64*U && enemy.body.y==y && enemy.body.flags==2);
    }
    CHECK(g->players[0].entity.body.x>x+58*U && g->players[0].inventory.health>0 && g->player.view.third_person);
    pair_clear(&g->players[0].entity,&enemy);CHECK(ssa_world_remove(&g->world,&enemy)==1);
    ssa_native_game_free(g);free(g);++cases;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);FILE *f;SsaMathTables math;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    CHECK(ssa_math_tables_load(&math,rom,0x800000));scenarios(&math);player_clearance(&math);real_maps(rom,&math);native_player_clearance(rom);native_input(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"scenarios\":%u,\"crowd_frames\":%u,\"overlap_separation_checks\":%u,\"maps\":16,\"real_map_walks\":%u,\"native_frames\":%u,\"native_enemy_moves\":%u,\"player_escape_frames\":%u}\n",checks,cases,frames,separations,real_frames,native_frames,native_moves,player_frames);return 0;
}
