#include "ssa/campaign_story.h"
#include <string.h>

static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int32_t signed32(uint32_t n)
{return n<=INT32_MAX?(int32_t)n:(int32_t)((int64_t)n-INT64_C(4294967296));}
static int32_t high16(uint32_t n)
{unsigned h=n>>16;return h<32768?(int32_t)h:(int32_t)h-65536;}
static int32_t shift(uint32_t n,unsigned bits)
{return signed32((n>>bits)|((n&0x80000000u)?(~0u<<(32-bits)):0));}
int ssa_campaign_story_init(SsaCampaignStory *out,const void *data,size_t size,
                           unsigned level,unsigned music)
{
    SsaCampaignStory s={0};const uint8_t *rom=data;unsigned i,j;
    if(!out || !rom || size!=0x800000 || (level!=0 && level!=6 && level!=12) || music>1)return -1;
    for(i=0;i<8;++i){
        for(j=0;j<5;++j)s.records[i][j]=word(rom+0x44cfc4+20*i+4*j);
        if(s.records[i][0]>2 || s.records[i][1]>2 || s.records[i][3]>4 || s.records[i][4]>=252 ||
           ((i==3 || i==5 || i==7)?s.records[i][2]!=0:s.records[i][2]==0))return -1;
    }
    s.index=level==0?0:level==6?4:6;s.display_image=69;s.music=(uint8_t)music;s.loaded=1;
    *out=s;return 1;
}
static int script(SsaCampaignStory *s)
{
    const uint32_t *r=s->records[s->index];unsigned ready=0;
    switch(s->stage){
    case 0:
        switch(r[0]){
        case 0:s->x=104u<<16;s->y=0xff800000;s->vx=0;s->vy=4u<<16;break;
        case 1:s->x=240u<<16;s->y=16u<<16;s->vx=0xfffc0000;s->vy=0;break;
        case 2:s->x=104u<<16;s->y=160u<<16;s->vx=0;s->vy=0xfffc0000;break;
        }
        s->image=r[3];++s->stage;break;
    case 1:
        if(r[0]==0 && signed32(s->y)>0xfffff){s->y=16u<<16;s->vy=0;ready=1;}
        if(r[0]==1 && signed32(s->x)<=104*65536){s->x=104u<<16;s->vx=0;ready=1;}
        if(r[0]==2 && signed32(s->y)<=16*65536){s->y=16u<<16;s->vy=0;ready=1;}
        if(ready){s->wait=r[2]+180;++s->stage;if(!s->mode){s->text=r[4];s->fade_target=127;}}
        break;
    case 2:
        if(signed32(--s->wait)<=0){s->speed=0x4000;++s->stage;}break;
    case 3:
        if(!s->speed){s->wait=r[2]+300;++s->stage;}break;
    case 4:
        if(signed32(--s->wait)<=0){
            if(r[1]==0)s->vy=0xfffc0000;
            if(r[1]==1)s->vx=4u<<16;
            if(r[1]==2)s->vy=4u<<16;
            ++s->stage;s->fade_target=0;
        }break;
    case 5:
        if(r[1]==0 && signed32(s->y)<=-128*65536){s->y=0xff800000;s->vy=0;ready=1;}
        if(r[1]==1 && signed32(s->x)>0xefffff){s->x=240u<<16;s->vx=0;ready=1;}
        if(r[1]==2 && signed32(s->y)>0x9fffff){s->y=160u<<16;s->vy=0;ready=1;}
        if(ready){++s->index;if(!s->records[s->index][2]){++s->stage;return 0;}s->stage=0;}
        break;
    default:break;
    }
    return 1;
}
int ssa_campaign_story_step(SsaCampaignStory *s,uint32_t input,const SsaCampaignStoryOps *ops)
{
    unsigned i;int result=-1,lines;uint32_t delta;int32_t row,limit;
    if(!s || s->loaded!=1 || s->updating || s->index>=8 || s->stage>6 || s->mode>2 || s->music>1 ||
       (s->stage==5 && s->index==7) || !ops || !ops->event)return -1;
    s->updating=1;
#define EVENT(k,a,b,c,d) do{if(ops->event(ops->context,s,k,a,b,c,d)<0)goto done;}while(0)
    if(!s->mode && (input&0x10000)){
        EVENT(SSA_STORY_COMMAND,13,0,0,0);s->fade_target=0;s->mode=1;
        if(s->music)EVENT(SSA_STORY_MUSIC,7,0,0,0);
        input=0;
    }
    if(!script(s)){s->fade_target=0;s->mode=1;}
    s->x+=s->vx;s->y+=s->vy;
    for(i=0;i<4;++i){int32_t x=high16(s->x)+(int32_t)((i&1)*64),y=high16(s->y)+(int32_t)((i&2)*32);
        if(x>240)x=240;
        if(x< -64)x= -64;
        if(y>160)y=160;
        if(y< -64)y= -64;
        s->positions[i][0]=(int16_t)x;s->positions[i][1]=(int16_t)y;
    }
    if(s->display_image!=s->image){EVENT(SSA_STORY_IMAGE,s->image,0,0,0);s->display_image=s->image;}
    if(s->display_text!=s->text){s->scroll=0;s->display_text=s->text;EVENT(SSA_STORY_TEXT,s->text,0,0,0);}
    if(input&0x40)s->rate=0;
    else{if(input&0x800000)s->wait=0;s->rate=(input&0x80)?4:1;}
    delta=s->rate*s->speed;s->scroll+=delta;
    if(signed32(s->speed)>0){
        lines=ops->event(ops->context,s,SSA_STORY_LINES,s->display_text,0,0,0);
        if(lines<0)goto done;
        limit=signed32(((uint32_t)lines-21u)*8u);
        if(high16(s->scroll)>limit){
            s->scroll=((uint32_t)lines-21u)<<19;
            if(signed32(s->scroll)<0)s->scroll=0;
            s->speed=0;
        }
        if(((s->scroll>>16)&7u)>((uint32_t)shift(s->rate*s->speed,16)&7u)){
            row=shift(s->scroll,19)+20;
            EVENT(SSA_STORY_ROW,s->display_text,(uint16_t)row,(uint32_t)(row%21),12);
        }
    }
    result=s->mode?0:1;
done:s->updating=0;return result;
#undef EVENT
}
