#include "ssa/completion.h"
#include <string.h>

static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int positive(uint32_t n){return n && n<UINT32_C(0x80000000);}
static int signed_greater(uint32_t a,uint32_t b)
{return (a^UINT32_C(0x80000000))>(b^UINT32_C(0x80000000));}
int ssa_completion_init(SsaCompletion *out,const void *data,size_t size,unsigned level,
    unsigned difficulty,uint32_t counter,uint32_t score,uint32_t elapsed,
    uint16_t flags,const int16_t ammo[8])
{
    const uint8_t *rom=data;SsaCompletion next={0};unsigned i;
    if(!out || !rom || size!=0x800000 || level>=12 || difficulty>=3 || !ammo)return -1;
    next.counter=counter;next.score=score;next.elapsed=elapsed;
    next.source_score=score;next.source_counter=counter;
    next.par_seconds=word(rom+0x44a610+level*4);memcpy(next.ammo,ammo,sizeof next.ammo);
    next.flags=(uint16_t)(flags|0x12u);next.level=(uint16_t)level;next.difficulty=(uint8_t)difficulty;
    next.bonus_y=88;next.hold=60;next.flash=29;next.loaded=next.running=1;
    for(i=0;i<30;++i)next.blend_table[i]=(uint16_t)((unsigned)rom[0x44a5d4+i*2]|(unsigned)rom[0x44a5d5+i*2]<<8);
    *out=next;return 1;
}
int ssa_completion_step(SsaCompletion *s,uint32_t held,uint32_t pressed,
    unsigned voice_finished,const SsaCompletionOps *ops)
{
    uint32_t amount=(held&1)?1000u:100u,multiplier;
    unsigned i;int result=-1;
    if(!s || !s->loaded || s->updating || s->stage>6 || s->flash>29 ||
       !ops || !ops->event || voice_finished>1)return -1;
    if(!s->running)return 0;
    s->updating=1;++s->counter;
#define EVENT(kind,a) do{if(ops->event(ops->context,s,kind,a,0,kind==SSA_COMPLETION_NOTICE?40u:0u,0)<0)goto done;\
    if(kind==SSA_COMPLETION_NOTICE){s->notice_timer=120;s->flags&=(uint16_t)~0x200u;}}while(0)
    switch(s->stage){
    case 0:
        s->pending=s->par_seconds-s->elapsed/60u;
        if(positive(s->pending)){
            s->pending*=1000u*((uint32_t)s->difficulty+1u);s->stage=1;
            EVENT(SSA_COMPLETION_NOTICE,0xb6);s->notice_timer=30000;
        }else s->stage=3;
        break;
    case 1:case 4:
        if(s->hold){--s->hold;break;}
        if(signed_greater(s->pending,amount)){
            s->pending-=amount;s->score+=amount;
            if(!(s->counter&7))EVENT(SSA_COMPLETION_COMMAND,16);
        }else{
            s->score+=s->pending;s->pending=s->hold;
            if(s->stage==1){s->stage=3;s->delay=30;}
            else{
                s->stage=5;s->delay=30;EVENT(SSA_COMPLETION_NOTICE,0xb8);
                s->notice_timer=30000;s->hold=60;
            }
        }
        break;
    case 2:
        --s->delay;if(!positive(s->delay) || (held&1))s->stage=3;
        break;
    case 3:
        s->pending=0;
        for(i=0;i<8;++i)s->pending+=(uint32_t)(int32_t)s->ammo[i]*10u;
        multiplier=(uint32_t)s->difficulty+1u;s->pending*=multiplier;
        if(positive(s->pending)){
            s->pending*=10u*multiplier;s->stage=4;EVENT(SSA_COMPLETION_NOTICE,0xb7);
            s->notice_timer=30000;s->bonus_y=120;
        }else{s->pending=0;s->stage=5;s->delay=30;}
        s->hold=60;break;
    case 5:
        --s->delay;if(!positive(s->delay) || (held&1))s->stage=6;
        break;
    case 6:
        if(s->hold==60){
            s->flags=(uint16_t)((s->flags&~0x10u)|0x20u);
            EVENT(SSA_COMPLETION_NOTICE,(uint16_t)(0xb9u+s->level));
            s->notice_timer=30000;EVENT(SSA_COMPLETION_SOUND,7);
        }else if((!s->hold || (pressed&1)) && voice_finished)s->running=0;
        if(s->hold)--s->hold;
        break;
    }
    /* BLDALPHA is selected before the flash index decreases. */
    s->blend=s->blend_table[s->flash];
    if(s->flash)--s->flash;
    s->flags|=2;s->updating=0;return s->running?1:0;
done:
    s->updating=0;return result;
#undef EVENT
}
