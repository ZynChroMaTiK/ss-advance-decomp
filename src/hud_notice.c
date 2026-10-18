#include "ssa/hud_notice.h"
#include <string.h>

static uint32_t word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static const uint8_t *data(const SsaHudNotice *n,uint32_t address,size_t bytes)
{
    size_t off;if(address<0x08000000)return NULL;
    off=(size_t)(address-0x08000000);
    return off<=n->rom_size && bytes<=n->rom_size-off?n->rom+off:NULL;
}
static uint32_t shift(uint32_t value,unsigned bits){return bits<32?value>>bits:0;}
static int valid(const SsaHudNotice *n)
{return n && n->loaded==1 && n->rom && n->rom_size==0x800000 && n->language<3;}
int ssa_hud_notice_init(SsaHudNotice *out,const void *rom,size_t size,unsigned language)
{
    SsaHudNotice next={0};
    if(!out || !rom || size!=0x800000 || language>=3)return -1;
    next.rom=rom;next.rom_size=size;next.language=language;next.loaded=1;*out=next;return 1;
}
int ssa_hud_notice_set(SsaHudNotice *n,SsaHudSpriteState *s,unsigned id,unsigned value,
                       unsigned y,unsigned font)
{
    const uint8_t *table,*text,*end;size_t length,i;unsigned count=0;
    if(!valid(n) || !s || (uint16_t)id>=252)return -1;
    table=data(n,word(n->rom+0x44cf34+4*n->language),252*4);
    if(!table || !(text=data(n,word(table+4*(uint16_t)id),1)))return -1;
    end=memchr(text,0,n->rom_size-(size_t)(text-n->rom));
    if(!end || (length=(size_t)(end-text))>=sizeof(n->text))return -1;
    for(i=0;i<length;++i){
        if(text[i]=='%')return -1;
        if(text[i]!=32 && text[i]!=10 && ++count>42)return -1;
    }
    n->id=(uint16_t)id;n->value=(uint16_t)value;n->base_y=(uint8_t)y;n->shift=(uint8_t)font;
    memcpy(n->text,text,length+1);n->length=(uint8_t)length;n->count=(uint8_t)count;n->progress=0;
    s->notice_timer=120;s->game_flags&=(uint16_t)~0x200u;return 1;
}
static int layout(SsaHudNotice *n,SsaHudSpriteState *s)
{
    unsigned at=0,count=0,previous=0;uint16_t y=n->base_y;
    memset(n->x,0,sizeof(n->x));memset(n->y,0,sizeof(n->y));memset(n->image,0,sizeof(n->image));
    s->rectangle[0]=255;s->rectangle[1]=(uint8_t)(n->base_y-8u);
    s->rectangle[2]=s->rectangle[3]=0;
    while(n->text[at]){
        unsigned start=count,width=0,i;
        while(n->text[at] && n->text[at]!=10){
            unsigned c=n->text[at++],glyph;
            if(c==32){width+=shift(10,n->shift);previous=0;continue;}
            if(count>=42)return -1;
            glyph=n->rom[0x44c264+c];
            n->image[count]=(uint16_t)(glyph+0x5b);
            width+=previous>>1;n->x[count++]=(uint8_t)width;
            previous=shift(n->rom[0x44bc0b+glyph],n->shift);
            width+=1+(previous>>1);
        }
        if(n->text[at]==10){previous=0;++at;}
        if(width>s->rectangle[2])s->rectangle[2]=(uint8_t)(width+7);
        for(i=start;i<count;++i){
            n->x[i]=(uint8_t)(n->x[i]+120-(width>>1));n->y[i]=(uint8_t)y;
            if(n->x[i]<s->rectangle[0])s->rectangle[0]=n->x[i];
        }
        y=(uint16_t)(y+shift(16,n->shift));
    }
    s->rectangle[0]=(uint8_t)(s->rectangle[0]-8u);
    s->rectangle[2]=(uint8_t)(s->rectangle[2]+8u+(previous>>1));
    s->rectangle[3]=(uint8_t)((int)y-((int)s->rectangle[1]-4));return 1;
}
int ssa_hud_notice_step(SsaWeaponObject *o,SsaHudSpriteState *s,SsaHudNotice *n,
                        const SsaWeaponAnimations *animations,unsigned ticks)
{
    SsaWeaponObject object;SsaHudSpriteState state;SsaHudNotice next;unsigned index;uint16_t timer;
    if(!o || !s || !valid(n) || !animations || animations->loaded!=1 || ticks>255 ||
       o->type!=13 || o->subtype>=42 || n->count>42 || !memchr(n->text,0,sizeof(n->text)))return -1;
    object=*o;state=*s;next=*n;index=o->subtype;
    if((s->local_flags&8) || !s->notice_timer){
        state.game_flags&=(uint16_t)~0x200u;goto hidden;
    }
    object.flags|=1;
    if(!index){
        if(next.progress!=next.count)next.progress=0;
        timer=(uint16_t)((uint16_t)state.notice_timer-ticks);
        state.notice_timer=timer<=32767?(int16_t)timer:(int16_t)((int)timer-65536);
        if(state.notice_timer<=0){state.notice_timer=0;state.game_flags&=(uint16_t)~0x240u;goto hidden;}
        if(layout(&next,&state)<0)return -1;
    }
    if(!next.image[index])goto hidden;
    object.x=(int32_t)next.x[index]*256;object.y=(int32_t)next.y[index]*256;
    object.scale_x=object.scale_y=(uint16_t)shift(4096,next.shift);
    if(ssa_weapon_animation_set(&object,next.image[index],5)<0 ||
       ssa_weapon_animation_resolve(&object,animations)<0)return -1;
    state.game_flags|=0x40;
    if(next.progress!=next.count){
        object.flags&=~UINT32_C(1);if(object.previous_frame==object.frame)++next.progress;
    }else if(!(state.game_flags&0x200))state.game_flags|=0x200;
    goto done;
hidden:object.flags&=~UINT32_C(1);
done:*o=object;*s=state;*n=next;return 1;
}
