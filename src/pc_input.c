#include "ssa/pc_input.h"
#include <limits.h>
#include <string.h>

static int ready(const SsaPcInput *p) {return p && p->yaw_per_count>0;}
static int32_t signed_bits(uint32_t v) {return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static void pending_clear(SsaPcInput *p) {
    p->pressed=p->pending_yaw=0;
    p->pending_pitch=0;
    p->fire_pressed=p->jump_pressed=p->use_pressed=p->view_toggle=0;
}
int ssa_pc_input_init(SsaPcInput *p,int32_t sensitivity) {
    if(!p || sensitivity<=0)return -1;
    memset(p,0,sizeof(*p));p->yaw_per_count=sensitivity;p->focused=p->enabled=1;return 1;
}
int ssa_pc_input_sensitivity(SsaPcInput *p,int32_t sensitivity) {
    if(!ready(p) || sensitivity<=0)return -1;
    p->yaw_per_count=sensitivity;return 1;
}
void ssa_pc_input_enable(SsaPcInput *p,int enabled) {
    if(!ready(p))return;
    p->enabled=(uint8_t)!!enabled;
    if(!enabled) {
        p->blocked|=p->down&(SSA_PC_SPACE|SSA_PC_H|SSA_PC_E);
        if(p->left_down)p->left_blocked=1;
        pending_clear(p);
    }
}
void ssa_pc_input_focus(SsaPcInput *p,int focused) {
    if(!ready(p))return;
    p->focused=(uint8_t)!!focused;
    if(!focused) {
        p->down=p->blocked=0;p->left_down=p->left_blocked=0;pending_clear(p);
    }
}
static uint32_t key_bit(int key) {
    if(key>='a' && key<='z')key-=32;
    switch(key) {
    case 'W':return SSA_PC_W;case 'S':return SSA_PC_S;
    case 'A':return SSA_PC_A;case 'D':return SSA_PC_D;
    case ' ':return SSA_PC_SPACE;case 'H':return SSA_PC_H;case 'E':return SSA_PC_E;
    default:return 0;
    }
}
int ssa_pc_input_key(SsaPcInput *p,int key,int down,int repeat) {
    uint32_t bit;int edge,was_down;
    if(!ready(p))return -1;
    bit=key_bit(key);if(!bit)return 0;
    if(!p->focused)return 1;
    was_down=!!(p->down&bit);edge=down && !was_down && !repeat;
    if(down)p->down|=bit;
    else {p->down&=~bit;p->blocked&=~bit;return 1;}
    if((!p->enabled || (repeat && !was_down)) && (bit&(SSA_PC_SPACE|SSA_PC_H|SSA_PC_E)))p->blocked|=bit;
    if(!edge || !p->enabled || (p->blocked&bit))return 1;
    p->pressed|=bit;
    if(bit==SSA_PC_SPACE)p->jump_pressed=1;
    else if(bit==SSA_PC_H)p->view_toggle^=1;
    else if(bit==SSA_PC_E)p->use_pressed=1;
    return 1;
}
int ssa_pc_input_button(SsaPcInput *p,unsigned button,int down) {
    int edge;
    if(!ready(p))return -1;
    if(button!=SSA_PC_MOUSE_LEFT)return 0;
    if(!p->focused)return 1;
    edge=down && !p->left_down;p->left_down=(uint8_t)!!down;
    if(!down)p->left_blocked=0;
    else if(!p->enabled)p->left_blocked=1;
    else if(edge && !p->left_blocked)p->fire_pressed=1;
    return 1;
}
int ssa_pc_input_relative(SsaPcInput *p,int32_t dx,int32_t dy) {
    if(!ready(p))return -1;
    if(p->focused && p->enabled){
        int64_t pitch=(int64_t)p->pending_pitch-(int64_t)dy*p->yaw_per_count;
        p->pending_yaw+=(uint32_t)dx*(uint32_t)p->yaw_per_count;
        p->pending_pitch=pitch>INT32_MAX?INT32_MAX:pitch<INT32_MIN?INT32_MIN:(int32_t)pitch;
    }
    return 1;
}
int ssa_pc_input_sample(SsaPcInput *p,SsaPcInputSample *out) {
    SsaPcInputSample n={0};uint32_t down;
    if(!ready(p) || !out)return -1;
    n.enabled=(uint8_t)(p->focused && p->enabled);down=p->down;
    if(n.enabled) {
        if((down&(SSA_PC_W|SSA_PC_S))==SSA_PC_W)n.keys|=0x40;
        else if((down&(SSA_PC_W|SSA_PC_S))==SSA_PC_S)n.keys|=0x80;
        if((down&(SSA_PC_A|SSA_PC_D))==SSA_PC_A)n.keys|=0x200;
        else if((down&(SSA_PC_A|SSA_PC_D))==SSA_PC_D)n.keys|=0x100;
        if(p->use_pressed)n.keys|=0x10000;
        n.yaw_delta=signed_bits(p->pending_yaw);
        n.pitch_delta=p->pending_pitch;
        n.held=(uint16_t)((p->left_down && !p->left_blocked) || p->fire_pressed);
        n.pressed=p->fire_pressed;
        n.jump_down=(uint8_t)(((down&SSA_PC_SPACE) && !(p->blocked&SSA_PC_SPACE)) || p->jump_pressed);
        n.view_toggle=p->view_toggle;
        n.packed=n.keys|n.held|((uint32_t)n.pressed<<16);
    }
    pending_clear(p);*out=n;return 1;
}
