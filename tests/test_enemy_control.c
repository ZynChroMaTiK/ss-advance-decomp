#include "ssa/enemy_control.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned callbacks;
static SsaEntity player;
static int emit(void *p,const SsaEnemyControl *c,const SsaEnemyControlEvent *e) {
    (void)p;(void)c; assert(e->kind<=SSA_ENEMY_CONTROL_KNOCKBACK); ++callbacks; return 0;
}
static int aim(void *p,const SsaEnemyControl *c,int32_t z,uint16_t h,int32_t range,SsaEntity **out) {
    (void)p;(void)c;(void)z;(void)h;assert(range==0xc000000);*out=NULL;return 0;
}
int ssa_enemy_navigation_step(SsaEnemyNavigation *n,SsaEntity *p,const SsaMathTables *m,
                              unsigned t,int32_t speed,const SsaEnemyNavigationOps *o) {
    (void)n;(void)p;(void)m;(void)t;(void)speed;return o?1:-1;
}
int main(int argc,char **argv) {
    FILE *f; uint8_t *rom; size_t size; unsigned i,type,frames=0;
    SsaMathTables math; SsaEnemyActivationGame ag={0}; SsaEnemyControlGame g={0};
    SsaEnemyNavigationOps nav={0};SsaEnemyControlOps ops={&nav,aim,emit,NULL};
    assert(argc==2);f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));size=(size_t)ftell(f);
    rewind(f);rom=malloc(size);assert(rom);assert(fread(rom,1,size,f)==size);fclose(f);
    assert(ssa_math_tables_load(&math,rom,size));assert(!ssa_enemy_control_periods(&g,rom,size));free(rom);
    g.activation=&ag;g.player=&player;player.body.flags=0x10;
    for(type=0xcf;type<=0xd2;++type) for(i=0;i<256;++i) {
        SsaEntity e={0};SsaEnemyActivation a={0};SsaEnemyNavigation n={0};SsaEnemyControl c={0};
        unsigned j;e.type=(uint16_t)type;a.entity=&e;n.entity=&e;c.activation=&a;c.navigation=&n;
        assert(!ssa_enemy_basic_step(&c,&g,&math,1,0x4000,&ops));
        a.timer=0;n.navigation_flags=1;assert(!ssa_enemy_control_attack(&c,&g,1,&ops));
        e.body.flags|=0x400;a.timer=0;assert(ssa_enemy_control_attack(&c,&g,1,&ops)==1);
        for(j=0;j<32;++j) { assert(ssa_enemy_basic_step(&c,&g,&math,1,0x4000,&ops)>=0);c.animation_done=1;++frames; }
        a.state=1;a.health=0;e.body.flags|=1;assert(!ssa_enemy_basic_step(&c,&g,&math,1,0x4000,&ops));
        a.timer=0;assert(ssa_enemy_basic_step(&c,&g,&math,1,0x4000,&ops)==1);
        assert(ssa_enemy_basic_step(&c,&g,&math,1,0x4000,&ops)==-1);
    }
    printf("{\"native_frames\":%u,\"callbacks\":%u,\"cycles\":1024}\n",frames,callbacks);return 0;
}
