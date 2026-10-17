#include "ssa/sector_clip.h"
#include <limits.h>

static int32_t bits(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return bits((uint32_t)a-(uint32_t)b);}
static int32_t abs32(int32_t n){return n<0?bits(0u-(uint32_t)n):n;}
static int32_t whole(int32_t n){return bits(((uint32_t)n>>16)|(n<0?0xffff0000u:0));}
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static int s16(const uint8_t *p){unsigned n=u16(p);return n<32768?(int)n:(int)n-65536;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static const uint8_t *record(const SsaMapAssets *m,uint32_t offset,size_t index,size_t stride){
    if(!m || !m->data || offset>m->size || stride>m->size-offset || index>(m->size-offset-stride)/stride)return NULL;
    return m->data+offset+index*stride;
}
static const uint8_t *sector(const SsaMapAssets *m,unsigned index){
    return m && index<m->sector_count?record(m,m->sectors,index,20):NULL;
}
static int32_t dot(int32_t x,int32_t y,int32_t s,int32_t c){
    uint64_t p=(uint64_t)((int64_t)x*s)+(uint64_t)((int64_t)y*c);
    return bits((uint32_t)(p>>16));
}
int ssa_sector_clip_find(const SsaMapAssets *m,const SsaMathTables *math,SsaSectorClip *state){
    const uint8_t *sec;SsaSectorClip next;unsigned i,count=0;
    if(!math || !state || !(sec=sector(m,state->sector)))return -1;
    next=*state;next.distance=INT32_MIN;next.line=0xffff;
    for(i=0;i<u16(sec+8);++i){
        const uint8_t *entry=record(m,u32(sec+12),i,2),*line,*v1,*v2;
        unsigned tagged,angle;int32_t dx,dy,s,c,along,length,distance,quotient;
        if(!entry)return -1;
        tagged=u16(entry);
        if((tagged&0x7fff)>=m->line_count || !(line=record(m,m->lines,tagged&0x7fff,40)) ||
           u16(line)>=m->vertex_count || u16(line+2)>=m->vertex_count ||
           !(v1=record(m,m->vertices,u16(line),4)) || !(v2=record(m,m->vertices,u16(line+2),4)))return -1;
        dx=sub(next.end_x,(int32_t)s16(v1)*65536);dy=sub(next.end_y,(int32_t)s16(v1+2)*65536);
        ssa_sincos(math,u16(line+28),&s,&c);
        along=dot(dx,dy,s,c);
        length=whole(dot(sub((int32_t)s16(v2)*65536,(int32_t)s16(v1)*65536),
                         sub((int32_t)s16(v2+2)*65536,(int32_t)s16(v1+2)*65536),s,c));
        if(!length)continue;
        quotient=bits((uint32_t)((int64_t)along/length));
        if((uint32_t)quotient>65536u)continue;
        angle=(u16(line+28)+(tagged&0x8000)+0x4000u)&0xffff;
        ssa_sincos(math,(uint16_t)angle,&s,&c);
        distance=sub(abs32(dot(dx,dy,s,c)),next.radius);
        if(distance<=0 && distance>next.distance){
            next.distance=distance;next.line=(uint16_t)tagged;count=(count+1)&0xffff;
        }
    }
    *state=next;return (int)count;
}
int ssa_sector_clip(const SsaMapAssets *m,const SsaMathTables *math,SsaSectorClip *state){
    SsaSectorClip next;uint16_t repeat=0xffff,last=0xffff;unsigned count=0,iterations;
    if(!state || !math || !sector(m,state->sector))return -1;
    next=*state;
    for(iterations=0;iterations<1024;++iterations){
        const uint8_t *old=sector(m,next.sector),*line;int found=ssa_sector_clip_find(m,math,&next),leaf;
        unsigned side;int32_t s,c,correction;
        if(found<0)return -1;
        if(!found)repeat=0xffff;
        else {
            int step;
            if(next.line==last)break;
            last=next.line;side=last>>15;line=record(m,m->lines,last&0x7fff,40);
            if(!line || !old)return -1;
            if(u16(line+26)&4){
                const uint8_t *other=sector(m,u16(line+12+(side^1u)*2));
                if(!other)return -1;
                step=(int)(uint16_t)(u16(other)-u16(old));if(step>=32768)step-=65536;
                if(s16(other+2)-s16(other)<=99 || s16(old+2)-s16(old)<=99)step=256;
                repeat=step>48?(uint16_t)(repeat+1):0xffff;
            }else repeat=(uint16_t)(repeat+1);
            if(repeat!=0xffff){
                ssa_sincos(math,(uint16_t)(u16(line+28)+(side?0x8000:0)+0x4000),&s,&c);
                correction=sub(next.distance,3*65536);
                next.end_x=sub(next.end_x,dot(s,0,correction,0));
                next.end_y=sub(next.end_y,dot(c,0,correction,0));++count;
            }
        }
        leaf=ssa_map_leaf(m,next.end_x,next.end_y);
        if(leaf<0 || leaf>32767)return -1;
        next.leaf=(uint16_t)leaf;
        {const uint8_t *l=record(m,m->leaves,(unsigned)leaf,8);if(!l)return -1;next.sector=u16(l+4);}
        if(!sector(m,next.sector))return -1;
        if(repeat==0xffff)break;
    }
    if(iterations==1024)return -1;
    *state=next;return (int)count;
}
int ssa_sector_relocate(const SsaMapAssets *m,const SsaMathTables *math,const SsaEntity *e,SsaSectorClip *out){
    SsaSectorClip state={0};const uint8_t *leaf,*sec;int result;int32_t floor,ceiling;
    if(!e || !out || e->leaf<0 || !m || (unsigned)e->leaf>=m->leaf_count ||
       !(leaf=record(m,m->leaves,(unsigned)e->leaf,8)))return -1;
    state.x=e->body.x;state.y=e->body.y;state.z=e->body.z;
    state.end_x=add(state.x,e->body.velocity_x);state.end_y=add(state.y,e->body.velocity_y);
    state.end_z=add(state.z,e->body.velocity_z);state.radius=32*65536;
    state.leaf=(uint16_t)e->leaf;state.sector=u16(leaf+4);
    result=ssa_sector_clip(m,math,&state);if(result<0 || !(sec=sector(m,state.sector)))return -1;
    floor=(int32_t)s16(sec)*65536;ceiling=(int32_t)s16(sec+2)*65536;
    if(state.end_z<floor)state.end_z=floor;else if(state.end_z>ceiling)state.end_z=ceiling;
    *out=state;return result!=0;
}
