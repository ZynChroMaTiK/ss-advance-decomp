#include "ssa/palette_effect.h"
#include <string.h>
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int offset(const SsaPaletteEffect *p,uint32_t address,size_t *at,size_t bytes)
{
    if(address<0x08000000 || (size_t)(address-0x08000000)>p->size ||
       bytes>p->size-(size_t)(address-0x08000000))return 0;
    *at=(size_t)(address-0x08000000);return 1;
}
int ssa_palette_effect_init(SsaPaletteEffect *p,const uint8_t *rom,size_t size,unsigned level)
{
    SsaPaletteEffect next={0};uint32_t request=1;
    if(!p || !rom || level>=16)return -1;
    next.rom=rom;next.size=size;next.level=level;
    if(ssa_palette_effect_step(&next,&request,0)<0)return -1;
    next.counter=0;*p=next;return 1;
}
int ssa_palette_effect_step(SsaPaletteEffect *p,uint32_t *request,uint16_t local_flags)
{
    uint32_t r,count,address=0,table,next;size_t at;uint16_t colors[256];unsigned i;
    uint8_t counter;int family;
    if(!p || !p->rom || !request || p->level>=16)return -1;
    r=*request;count=r&0x1fffffff;next=r;counter=(uint8_t)(p->counter+1);
    family=p->level<=5 || p->level==12 || p->level==13;
    if(local_flags&8){address=0x0846265c;next=1;}
    else if(count==1){address=family?0x08454a5c:0x08454c5c;next=0;}
    else if(count){
        if(r&0x80000000){table=family?0x0844a86c:0x0844a88c;if(!(counter&3))--next;}
        else if(r&0x40000000){table=family?0x0844a8ac:0x0844a8cc;if(!(counter&3))--next;}
        else if(r&0x20000000){
            if(count>31)return -1;
            table=family?0x0844a8ec:0x0844a964;
            if((counter&1) && count>2)--next;
        }else{table=family?0x0844a82c:0x0844a84c;if(counter&1)--next;}
        if(!(r&0xc0000000) && (r&0x20000000))i=31-count;
        else{if(count>9)return -1;i=count-2;}
        if(!offset(p,table+4*i,&at,4))return -1;
        address=u32(p->rom+at);
    }
    memcpy(colors,p->colors,sizeof(colors));
    if(address){
        if(!offset(p,address,&at,512))return -1;
        for(i=0;i<256;++i)colors[i]=(uint16_t)(p->rom[at+2*i]|(unsigned)p->rom[at+2*i+1]<<8);
    }
    /* The original uses the count captured BEFORE flag8's request overwrite. */
    if(!count)colors[0]=0;
    memcpy(p->colors,colors,sizeof(colors));p->counter=counter;*request=next;return 1;
}
