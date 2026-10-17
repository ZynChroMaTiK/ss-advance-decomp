#include "ssa/actor_animation.h"
#include <string.h>

static uint16_t r16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static uint32_t r32(const uint8_t *p) { return (uint32_t)r16(p) | (uint32_t)r16(p+2)<<16; }
int ssa_actor_animations_load(SsaActorAnimations *a,const void *data,size_t size) {
    const uint8_t *rom=data;SsaActorAnimations result;unsigned i;
    if(!a || !rom || size<0x449d30u+12u*SSA_ACTOR_ANIMATION_COUNT)return -1;
    memset(&result,0,sizeof(result));
    for(i=0;i<SSA_ACTOR_ANIMATION_WORDS;++i)result.words[i]=r16(rom+0x449728u+2u*i);
    for(i=0;i<SSA_ACTOR_ANIMATION_COUNT;++i) {
        uint32_t p=r32(rom+0x449d30u+12u*i);
        if(p<0x08449728u || p>=0x08449d30u || (p&1u))return -1;
        result.offsets[i]=(uint16_t)((p-0x08449728u)/2u);
    }
    result.loaded=1;*a=result;return 1;
}
int ssa_actor_animation_set(SsaActorAnimation *a,uint16_t id,uint16_t period) {
    if(!a)return -1;
    if(id>0x8d && id!=0xffff)return 0;
    a->animation=id;a->period=(uint8_t)period;a->index=a->elapsed=a->done=0;return 1;
}
int ssa_actor_animation_step(SsaActorAnimation *a,const SsaActorAnimations *data,
    uint16_t flags,unsigned ticks) {
    SsaActorAnimation n;unsigned base,pos;
    if(!a || !data || data->loaded!=1 || ticks>255)return -1;
    if(flags&0x800)return 1;
    if(a->animation>=SSA_ACTOR_ANIMATION_COUNT)return -1;
    base=data->offsets[a->animation];pos=base+a->index;
    if(pos>=SSA_ACTOR_ANIMATION_WORDS)return -1;
    n=*a;n.frame=data->words[pos];
    if(n.period) {
        n.elapsed=(uint8_t)(n.elapsed+ticks);
        while(n.elapsed>=n.period) {
            uint16_t value;
            n.elapsed=(uint8_t)(n.elapsed-n.period);
            if(pos+1>=SSA_ACTOR_ANIMATION_WORDS)return -1;
            value=data->words[pos+1];
            if(value==0xffff) {n.index=0;n.done=255;}
            else if(value==0xfffe) {n.done=254;n.period=255;break;}
            else {
                if(value>=0xfffa && value<=0xfffd) {n.done=(uint8_t)value;++n.index;}
                ++n.index;
            }
            pos=base+n.index;
            if(pos>=SSA_ACTOR_ANIMATION_WORDS)return -1;
            n.frame=data->words[pos];
        }
    }
    *a=n;return 1;
}
int ssa_actor_animation_pool_step(SsaActorAnimation states[100],const uint16_t flags[100],
    const SsaEntityPool *pool,const SsaActorAnimations *data,unsigned ticks) {
    SsaActorAnimation next[100];unsigned i,slot;
    if(!states || !flags || !pool || !data || data->loaded!=1 || ticks>255 ||
       pool->active_count>100)return -1;
    memcpy(next,states,sizeof(next));
    for(i=0;i<pool->active_count;++i) {
        slot=pool->active_slots[i];
        if(slot>=100 || !pool->occupied[slot] || pool->positions[slot]!=i ||
           ssa_actor_animation_step(&next[slot],data,flags[slot],ticks)<0)return -1;
    }
    memcpy(states,next,sizeof(next));return 1;
}
