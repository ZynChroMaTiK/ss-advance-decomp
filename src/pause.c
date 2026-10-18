#include "ssa/pause.h"
#include "ssa/mode_control.h"
static int event(void *context,SsaModeControl *mode,SsaModeControlEvent kind,
    uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    SsaPause *s=context;(void)mode;(void)b;(void)c;(void)d;
    if(s->event_count>=8)return -1;
    s->events[s->event_count++]=a+(kind==SSA_MODE_CREATE_SPRITE?256:0);
    if(kind==SSA_MODE_CREATE_SPRITE){s->timer=120;s->flags&=(uint16_t)~0x200u;}
    return 1;
}
int ssa_pause_step(SsaPause *s,uint32_t input)
{
    uint8_t count=1;SsaModeControl mode={0};SsaModeControlOps ops={event,s};
    if(!s || s->selection>3 || (s->flags&0x900))return -1;
    s->event_count=0;
    mode.flags=&s->flags;mode.input_state=&s->input_state;mode.menu_timer=&s->timer;
    mode.player_count=&count;mode.selection=&s->selection;mode.inputs[0]=&input;
    return ssa_mode_control_step(&mode,&ops)<0?-1:1;
}
