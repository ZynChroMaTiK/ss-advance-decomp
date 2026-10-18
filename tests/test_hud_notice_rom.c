#include "ssa/hud_notice.h"
#include <string.h>
static SsaHudNotice notice;
static SsaHudSpriteState state;
static SsaWeaponAnimations animations;
static SsaWeaponObject object;
static uint8_t game[0x520];
static uint16_t half(const uint8_t *p){return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static void put(unsigned at,uint16_t value){game[at]=(uint8_t)value;game[at+1]=(uint8_t)(value>>8);}
int start(const void *rom,size_t size){
    memset(&state,0,sizeof(state));
    return ssa_hud_notice_init(&notice,rom,size,0)==1 && ssa_weapon_animations_load(&animations,rom,size)==1;
}
static void restore(const uint8_t *g,const uint8_t *x,const uint8_t *y,const void *images){
    unsigned timer;
    memcpy(game,g,sizeof(game));state.game_flags=half(g+4);timer=half(g+0x76);
    state.notice_timer=timer<=32767?(int16_t)timer:(int16_t)((int)timer-65536);
    notice.length=g[0x69];notice.count=g[0x6a];notice.shift=g[0x6b];notice.base_y=g[0x73];notice.progress=g[0x74];
    notice.id=half(g+0x78);notice.value=half(g+0x7a);memcpy(state.rectangle,g+0x6e,4);
    memcpy(notice.text,g+0x320,512);memcpy(notice.x,x,42);memcpy(notice.y,y,42);memcpy(notice.image,images,84);
}
static void save(void){
    put(4,state.game_flags);put(0x76,(uint16_t)state.notice_timer);put(0x78,notice.id);put(0x7a,notice.value);
    game[0x69]=notice.length;game[0x6a]=notice.count;game[0x6b]=notice.shift;
    game[0x73]=notice.base_y;game[0x74]=notice.progress;memcpy(game+0x6e,state.rectangle,4);
    memcpy(game+0x320,notice.text,512);
}
int set(const uint8_t *g,const uint8_t *x,const uint8_t *y,const void *images,unsigned lang,
        unsigned id,unsigned value,unsigned base,unsigned font){
    int result;restore(g,x,y,images);notice.language=lang;
    result=ssa_hud_notice_set(&notice,&state,id,value,base,font);save();return result;
}
int step(const uint8_t *g,const uint8_t *x,const uint8_t *y,const void *images,
         const void *o,unsigned ticks,unsigned local){
    int result;restore(g,x,y,images);memcpy(&object,o,sizeof(object));state.local_flags=(uint16_t)local;
    result=ssa_hud_notice_step(&object,&state,&notice,&animations,ticks);save();return result;
}
const void *output(unsigned kind){
    return kind==0?game:kind==1?notice.x:kind==2?notice.y:kind==3?(void *)notice.image:(void *)&object;
}
