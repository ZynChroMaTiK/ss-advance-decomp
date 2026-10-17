#include "ssa/enemy_ranged.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned assertions,events,failures;
#define CHECK(x) do{++assertions;if(!(x)){fprintf(stderr,"check failed line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct Context {SsaEnemyControl *c;SsaEnemyControlGame *g;SsaMathTables *math;SsaEnemyControlOps *ops;unsigned ticks;int fail;} Context;
static int event(void *context,const SsaEnemyControl *control,const SsaEnemyControlEvent *e)
{
    Context *p=context;(void)control;++events;
    CHECK(ssa_enemy_ranged_step(p->c,p->g,p->math,p->ticks,0x10000,p->ops)==-1);
    if(e->kind==SSA_ENEMY_CONTROL_ANIMATION)CHECK(!p->c->animation_done);
    if(e->kind==SSA_ENEMY_CONTROL_PROJECTILE)CHECK((e->args[0]==0x66 || e->args[0]==0x67) && e->args[1]==0x500000 && e->args[2]==0 && e->args[5]==1);
    if((int)e->kind==p->fail){++failures;return -1;}return 0;
}
static int aim(void *ctx,const SsaEnemyControl *c,int32_t z,uint16_t heading,int32_t range,SsaEntity **out)
{Context *p=ctx;(void)c;(void)z;(void)heading;CHECK(range==0xc000000);*out=p->g->player;return 0;}
static int move(void *ctx,SsaEnemyNavigation *n,int32_t *out)
{(void)ctx;CHECK(n->speed==0);*out=1;return 0;}
static int finish(void *ctx,SsaEnemyNavigation *n){(void)ctx;CHECK(n->target!=NULL);return 0;}
int main(int argc,char **argv)
{
    unsigned types[]={0xce,0xd3,0xd4,0xd5,0xd6},type,i;FILE *f;uint8_t *rom;SsaMathTables *math;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f);rom=malloc(8388608);math=malloc(sizeof(*math));CHECK(rom&&math);
    CHECK(fread(rom,1,8388608,f)==8388608);CHECK(!fclose(f));CHECK(ssa_math_tables_load(math,rom,8388608)==1);
    for(type=0;type<5;++type)for(i=0;i<4096;++i){
        SsaEntity e={0},player={0};SsaEnemyActivation a={0};SsaEnemyNavigation n={0};SsaEnemyControl c={0};
        SsaEnemyActivationGame activation={0};SsaEnemyControlGame g={0};SsaEnemyNavigationOps nav={0};SsaEnemyControlOps ops={0};
        Context ctx={&c,&g,math,&ops,i%256,i%31==0?(int)(i%11):-1};int result;
        e.type=(uint16_t)types[type];e.body.flags=(uint16_t)(i%8);e.body.height=0xb40000;e.body.z=(int32_t)(i%257)*65536;e.body.floor=0;
        player.body.x=0;player.body.y=100*65536;player.body.flags=0x10;
        a.entity=&e;a.state=(uint8_t)(i%9);a.health=(int16_t)((int)(i%151)-50);a.timer=(uint16_t)(i%361);a.facing=(uint8_t)i;
        n.entity=&e;n.target=&player;n.phase=(uint16_t)(i%4);n.distance=(uint16_t)(i%1025);n.navigation_flags=1;n.speed=0x30000;
        c.activation=&a;c.navigation=&n;c.animation_done=(uint8_t)(i%4?0xfa:0);c.drain_period=(uint16_t)(i%31);c.drain_countdown=(uint16_t)(i%37);
        g.activation=&activation;g.player=&player;g.blocking_entity=&player;g.player_y=player.body.y;
        CHECK(ssa_enemy_control_periods(&g,rom,8388608)==0);
        nav.move=move;nav.finish=finish;ops.navigation=&nav;ops.aim=aim;ops.event=event;ops.context=&ctx;
        result=ssa_enemy_ranged_step(&c,&g,math,ctx.ticks,0x10000,&ops);
        CHECK(result>=-1 && result<=1);CHECK(!c.updating&&!a.updating&&!n.updating);
        CHECK(result!=1 || c.destroy_requested);
        c.destroy_requested=0;c.updating=1;CHECK(ssa_enemy_ranged_step(&c,&g,math,1,0,&ops)==-1);c.updating=0;
        CHECK(ssa_enemy_ranged_step(&c,&g,math,256,0,&ops)==-1);
        e.type=0xcd;CHECK(ssa_enemy_ranged_step(&c,&g,math,1,0,&ops)==-1);
        CHECK(ssa_enemy_ranged_step(NULL,&g,math,1,0,&ops)==-1);
    }
    CHECK(events>0&&failures>0);free(math);free(rom);
    printf("{\"asan_ubsan_assertions\":%u,\"native_controller_steps\":20480,\"native_events\":%u,\"native_callback_failures\":%u}\n",assertions,events,failures);return 0;
}
