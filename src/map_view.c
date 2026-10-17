#include "ssa/map_view.h"
#include "ssa/map_assets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint16_t u16(const uint8_t *p)
{return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p)
{return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16;}
static const uint8_t *data(const SsaMapView *v,uint32_t address,size_t count)
{
    size_t off;
    if(address<0x08000000)return NULL;
    off=address-0x08000000;
    return off<=v->rom_size && count<=v->rom_size-off?v->rom+off:NULL;
}
static const char *string(const SsaMapView *v,uint32_t address)
{
    const uint8_t *p=data(v,address,1);
    return p && memchr(p,0,v->rom_size-(size_t)(p-v->rom))?(const char *)p:NULL;
}
static const char *text(const SsaMapView *v,unsigned index)
{
    const uint8_t *table=data(v,u32(v->rom+0x44cf34+v->language*4),index*4u+4u);
    return table?string(v,u32(table+index*4u)):NULL;
}
static int decode(const SsaMapView *v,unsigned off,uint8_t *out,size_t bytes)
{
    if(off>v->rom_size || v->rom_size-off<4 || (u32(v->rom+off)>>8)!=bytes)return 0;
    return ssa_lz77_decode(v->rom+off,v->rom_size-off,out,bytes,NULL);
}
int ssa_map_view_init(SsaMapView *v,const uint8_t *rom,size_t size,
    unsigned level,unsigned language,unsigned difficulty)
{
    uint8_t *scratch;unsigned x,y,i;
    if(!v || v->loaded || !rom || size!=0x800000 || level>=12 || language>=3 || difficulty>=3)return -1;
    memset(v,0,sizeof(*v));v->rom=rom;v->rom_size=size;v->level=level;v->language=language;v->difficulty=difficulty;
    scratch=malloc(38400+1200);
    if(!scratch)return -1;
    if(!decode(v,level<6?0xab39c:0xafe4c,scratch,38400) ||
       !decode(v,0xb4eec,scratch+38400,1200) ||
       !decode(v,0xb697c,v->fonts,16128) || !decode(v,0xb7a60,v->fonts+16128,4032))goto fail;
    for(i=0;i<256;++i)v->palette[i]=u16(rom+0xa8168+i*2);
    for(y=0;y<160;++y)for(x=0;x<240;++x){
        unsigned entry=u16(scratch+38400+((y/8)*30+x/8)*2),tx=x&7,ty=y&7;
        if((entry&1023)>=600)goto fail;
        if(entry&1024)tx=7-tx;
        if(entry&2048)ty=7-ty;
        v->background[y*240+x]=scratch[(entry&1023)*64+ty*8+tx];
    }
    if(!text(v,48+level) || !text(v,64) || !text(v,4) ||
       !string(v,u32(rom+0x44c1d4+(difficulty*12+level)*4)))goto fail;
    for(i=0;i<=level;++i){const uint8_t *route=data(v,u32(rom+0x44d1c4+i*4),22);
        if(!route || route[20] || route[21])goto fail;
    }
    v->fade_volume=0xf00;v->fade_step=-0x40;v->fading=1;v->loaded=1;
    free(scratch);return 1;
fail:free(scratch);memset(v,0,sizeof(*v));return -1;
}
int ssa_map_view_step(SsaMapView *v,unsigned fire_pressed)
{
    int first=0;
    if(!v || !v->loaded || fire_pressed>1)return -1;
    if(v->done)return 0;
    /* 15698 runs before 1AAA0 input and route animation in the original loop. */
    if(v->fading){v->fade_volume+=v->fade_step;
        if(v->fade_volume<0){v->fade_volume=0;v->fading=0;}
        else if(v->fade_volume>0xf00){v->fade_volume=0xf00;v->fading=0;}
    }
    if(fire_pressed && !v->leaving){v->leaving=1;v->fade_volume=0;v->fade_step=0x40;v->fading=1;first=1;}
    ++v->frame;v->done=v->leaving && !v->fading;return first;
}
static const uint8_t *metric(const SsaMapView *v,unsigned font,unsigned glyph)
{return v->rom+(font?0xb7ea8:0xb7dac)+glyph*2;}
static unsigned width(const SsaMapView *v,const char *s,unsigned font)
{
    unsigned n=0,size=font?8:16;
    for(;*s && *s!='\n';++s){unsigned c=(unsigned char)*s;
        if(c>32){unsigned glyph=v->rom[0x44c264+c],w;
            if(glyph>=126)return 0;
            w=metric(v,font,glyph)[1];n+=(w==0xcd?1:w)+1;
        }else if(c==32)n+=size/2;
    }
    return n;
}
static void line(SsaMapView *v,const char *s,int x,int y,unsigned font,unsigned palette)
{
    unsigned size=font?8:16;
    for(;*s && *s!='\n' && v->glyph_count<128;++s){unsigned c=(unsigned char)*s;
        if(c>32){unsigned glyph=v->rom[0x44c264+c];const uint8_t *m;
            if(glyph>=126)return;
            m=metric(v,font,glyph);
            v->glyphs[v->glyph_count++]=(SsaMapGlyph){(int16_t)(x-m[0]),(int16_t)y,(uint8_t)glyph,(uint8_t)font,(uint8_t)palette};
            x+=m[1]+1;
        }else if(c==32)x+=(int)size/2;
    }
}
static void centered(SsaMapView *v,const char *s,int y,unsigned font,unsigned palette)
{
    while(*s){line(v,s,120-(int)(width(v,s,font)/2),y,font,palette);
        while(*s && *s!='\n')++s;
        if(*s)++s;
        y+=font?8:16;
    }
}
const char *ssa_map_view_text(const SsaMapView *v,unsigned index)
{return v && v->loaded && index<252?text(v,index):NULL;}
unsigned ssa_map_view_text_width(const SsaMapView *v,const char *s,unsigned font)
{return v && v->loaded && s && font<2?width(v,s,font):0;}
int ssa_map_view_text_line(SsaMapView *v,const char *s,int x,int y,unsigned font,unsigned palette)
{
    if(!v || !v->loaded || !s || font>1 || palette>15 || v->glyph_count>128)return -1;
    line(v,s,x,y,font,palette);return y+(font?8:16);
}
int ssa_map_view_text_centered(SsaMapView *v,const char *s,int y,unsigned font,unsigned palette,int offset)
{
    if(!v || !v->loaded || !s || font>1 || palette>15 || v->glyph_count>128)return -1;
    while(*s){line(v,s,120+offset-(int)(width(v,s,font)/2),y,font,palette);
        while(*s && *s!='\n')++s;
        if(*s)++s;
        y+=font?8:16;
    }
    return y;
}
int ssa_map_view_draw(SsaMapView *v,uint16_t *out,size_t pixels)
{
    unsigned i,level;
    char password[9],bottom[128];const char *s;
    if(!v || !v->loaded || !out || pixels<240*160)return -1;
    v->glyph_count=0;
    for(level=v->level<6?0:6;level<=v->level;++level){
        unsigned count=level<v->level?10:v->frame>>5;
        const uint8_t *route=data(v,u32(v->rom+0x44d1c4+level*4),22);
        if(count>10)count=10;
        for(i=0;i<count;++i)line(v,"~",(int)route[i*2]-4,(int)route[i*2+1]-16,0,0);
    }
    if(v->frame&32)centered(v,text(v,64),142,1,1);
    centered(v,text(v,48+v->level),8,0,3);
    s=string(v,u32(v->rom+0x44c1d4+(v->difficulty*12+v->level)*4));
    for(i=0;i<8 && s[i] && s[i]!='_';++i)password[i]=s[i];
    password[i]=0;
    (void)snprintf(bottom,sizeof(bottom),"%s: %s",text(v,4),password);
    line(v,bottom,8,150,1,1);
    if(ssa_map_view_compose(v,out,pixels)!=1)return -1;
    /* The original BLDY fade stops at 15/16 darkness. Finish the PC handoff
     * on solid black instead of exposing that residual map behind the level. */
    if(v->leaving && v->fade_volume==0xf00)memset(out,0,240*160*sizeof(*out));
    return 1;
}
int ssa_map_view_overlay(const SsaMapView *v,uint16_t *out,size_t pixels)
{
    unsigned i,x,y;
    if(!v || !v->loaded || !out || pixels<240*160 || v->glyph_count>128)return -1;
    /* Lower OAM indices win equal-priority overlap, including route/title. */
    for(i=v->glyph_count;i>0;--i){const SsaMapGlyph *g=&v->glyphs[i-1];unsigned size=g->font?8:16;
        const uint8_t *tiles=v->fonts+(g->font?16128:0)+g->glyph*size*size/2;
        for(y=0;y<size;++y)for(x=0;x<size;++x){
            int dx=g->x+(int)x,dy=g->y+(int)y;
            unsigned at=((y/8)*(size/8)+x/8)*32+(y&7)*4+(x&7)/2;
            unsigned color=(tiles[at]>>((x&1)*4))&15;
            if(color && dx>=0 && dx<240 && dy>=0 && dy<160)out[(unsigned)dy*240+(unsigned)dx]=v->palette[g->palette*16+color];
        }
    }
    return 1;
}
int ssa_map_view_compose(const SsaMapView *v,uint16_t *out,size_t pixels)
{
    unsigned i,darken;
    if(!v || !v->loaded || !out || pixels<240*160 || v->glyph_count>128)return -1;
    for(i=0;i<240*160;++i)out[i]=v->palette[v->background[i]];
    if(ssa_map_view_overlay(v,out,pixels)!=1)return -1;
    darken=((unsigned)v->fade_volume>>8)&15;
    for(i=0;i<240*160;++i){unsigned c=out[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        out[i]=(uint16_t)((r-r*darken/16)|((g-g*darken/16)<<5)|((b-b*darken/16)<<10));
    }
    return 1;
}
