#include "ssa/campaign_screen.h"
#include "ssa/map_assets.h"
#include <stdlib.h>
#include <string.h>

static unsigned u16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16;}
static const uint8_t *data(const SsaCampaignScreen *s,uint32_t address,size_t count)
{
    size_t off;if(address<0x08000000)return NULL;off=address-0x08000000;
    return off<=s->rom_size && count<=s->rom_size-off?s->rom+off:NULL;
}
static const uint8_t *text(const SsaCampaignScreen *s,unsigned id)
{
    const uint8_t *table,*p;
    if(!s || !s->loaded || s->language>=3 || id>=252)return NULL;
    table=data(s,u32(s->rom+0x44cf34+4*s->language),252*4);
    if(!table || !(p=data(s,u32(table+4*id),1)) ||
       !memchr(p,0,s->rom_size-(size_t)(p-s->rom)))return NULL;
    return p;
}
static int decode(SsaCampaignScreen *s,uint32_t address,uint8_t *out,size_t bytes)
{
    const uint8_t *p=data(s,address,4);
    return p && (u32(p)>>8)==bytes &&
        ssa_lz77_decode(p,s->rom_size-(size_t)(p-s->rom),out,bytes,NULL);
}
int ssa_campaign_screen_lines(const SsaCampaignScreen *s,unsigned id)
{
    const uint8_t *p=text(s,id);int count=1;if(!p)return -1;
    while(*p)if(*p++==10)++count;
    return count;
}
int ssa_campaign_screen_row(const SsaCampaignScreen *s,unsigned id,unsigned row,uint8_t out[768])
{
    const uint8_t *p=text(s,id);unsigned line=0,palette=0,x=2,y,chars=0;
    uint8_t pixels[8][256]={{0}};
    if(!p || !out || row>65535)return -1;
    /* Prefix palettes persist across lines, including ones skipped by18D70. */
    for(;;){
        if(*p=='*'){if(p[1]<'0' || p[1]>'9')return -1;palette=p[1]-'0';p+=2;}
        if(line==row)break;
        while(*p && *p!=10)++p;
        if(!*p){memset(out,0,768);return 1;}
        ++p;++line;
    }
    while(*p && *p!=10){
        unsigned c,glyph,width,origin;
        if(*p=='*'){if(p[1]<'0' || p[1]>'9')return -1;palette=p[1]-'0';p+=2;continue;}
        if(++chars>31)return -1;
        c=*p++;glyph=s->rom[0x44c264+c];
        if(glyph>0){
            if(glyph>=126)return -1;
            origin=s->rom[0xb7ea8+2*glyph];width=s->rom[0xb7ea9+2*glyph];if(width>8)width=1;
            if(origin+width>8)return -1;
            for(y=0;y<8;++y){unsigned k;
                for(k=0;k<width && x+k<256;++k){
                    unsigned sx=origin+k,b=s->fonts[glyph*32+y*4+sx/2],color=(b>>((sx&1)*4))&15;
                    if(color)pixels[y][x+k]=(uint8_t)(color|(palette<<4));
                }
            }
        }else width=4;
        x+=width+1;
    }
    for(y=0;y<8;++y){unsigned k;for(k=0;k<96;++k)
        out[(k/8)*64+y*8+(k&7)]=pixels[y][k];}
    return 1;
}
static int event(void *context,const SsaCampaignStory *story,SsaCampaignStoryEvent kind,
                  uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    SsaCampaignScreen *s=context;unsigned i;(void)story;(void)d;
    switch(kind){
    case SSA_STORY_COMMAND:if(s->command_count>=4)return -1;s->commands[s->command_count++]=a;return 1;
    case SSA_STORY_MUSIC:if(s->fade_count>=2)return -1;s->fade_intervals[s->fade_count++]=a;return 1;
    case SSA_STORY_IMAGE:if(a>=8)return -1;s->image_ready=1;return 1;
    case SSA_STORY_TEXT:
        for(i=0;i<21;++i)if(ssa_campaign_screen_row(s,a,i,s->rows[i])!=1)return -1;
        return 1;
    case SSA_STORY_LINES:return ssa_campaign_screen_lines(s,a);
    case SSA_STORY_ROW:
        if(c>=21)return -1;
        return ssa_campaign_screen_row(s,a,b,s->rows[c]);
    default:return -1;
    }
}
int ssa_campaign_screen_init(SsaCampaignScreen *out,const uint8_t *rom,size_t size,
                            unsigned level,unsigned language,unsigned music)
{
    SsaCampaignScreen *s;unsigned i;int result=-1;
    if(!out || out->loaded || !rom || size!=0x800000 || language>=3 || music>1)return -1;
    s=calloc(1,sizeof(*s));if(!s)return -1;
    if(ssa_campaign_story_init(&s->story,rom,size,level,music)!=1)goto done;
    s->rom=rom;s->rom_size=size;s->language=language;s->next_level=level;s->loaded=1;
    if(!decode(s,0x080a8368,s->tiles,sizeof(s->tiles)) ||
       !decode(s,0x080b7a60,s->fonts,sizeof(s->fonts)))goto done;
    for(i=0;i<8;++i)if(!decode(s,u32(rom+0x44cfa4+4*i),s->panels[i],16384))goto done;
    for(i=0;i<256;++i)s->palette[i]=(uint16_t)u16(rom+0xa8168+2*i);
    for(i=197;i<=201;++i){int lines=ssa_campaign_screen_lines(s,i);unsigned j;uint8_t row[768];
        if(lines<1 || lines>512)goto done;
        for(j=0;j<(unsigned)lines;++j)if(ssa_campaign_screen_row(s,i,j,row)!=1)goto done;
    }
    for(i=0;i<4;++i){
        s->story.positions[i][0]=(int16_t)(104+(i&1)*64);
        s->story.positions[i][1]=(int16_t)(16+(i&2)*32);
    }
    s->fade_volume=0xf00;s->fade_step=-64;s->fading=1;s->commands[s->command_count++]=11;
    *out=*s;result=1;
done:free(s);return result;
}
int ssa_campaign_screen_step(SsaCampaignScreen *s,uint32_t input)
{
    SsaCampaignStoryOps ops={event,s};int result;
    if(!s || s->loaded!=1 || s->updating || s->text_fade>127 || s->story.fade_target>127)return -1;
    if(s->done)return 0;
    s->updating=1;s->command_count=s->fade_count=0;++s->frame;++s->story.counter;
    memcpy(s->draw_positions,s->story.positions,sizeof(s->draw_positions));
    if(s->text_fade<s->story.fade_target){s->text_fade+=2;if(s->text_fade>s->story.fade_target)s->text_fade=s->story.fade_target;}
    else if(s->text_fade>s->story.fade_target){s->text_fade=s->text_fade<2?0:s->text_fade-2;
        if(s->text_fade<s->story.fade_target)s->text_fade=s->story.fade_target;}
    s->text_enabled=s->text_fade!=0;
    if(!s->text_enabled){
        if(s->story.mode==1){
            s->fade_volume=0;s->fade_step=64;s->fading=1;s->alpha=0;s->story.mode=2;
            if(s->story.music)s->fade_intervals[s->fade_count++]=5;
        }
        if(s->fading){s->fade_volume+=s->fade_step;
            if(s->fade_volume<0){s->fade_volume=0;s->fading=0;}
            else if(s->fade_volume>0xf00){s->fade_volume=0xf00;s->fading=0;}
        }
    }else{
        s->alpha=1;s->blend_a=(s->text_fade+1)>>3;s->blend_b=(128-s->text_fade)>>3;
    }
    s->draw_text_scroll=s->story.scroll>>16;
    s->draw_scroll=s->scroll;if((s->frame-1)%8==0)s->scroll=(s->scroll+64)&511;
    result=ssa_campaign_story_step(&s->story,input,&ops);
    if(result<0){s->updating=0;return -1;}
    s->done=s->story.mode==2 && !s->fading;s->updating=0;
    return s->done?0:1;
}
static uint16_t blend(unsigned a,unsigned b,unsigned wa,unsigned wb)
{
    unsigned r=((a&31)*wa+(b&31)*wb)>>4,g=(((a>>5)&31)*wa+((b>>5)&31)*wb)>>4;
    unsigned blue=(((a>>10)&31)*wa+((b>>10)&31)*wb)>>4;
    if(r>31)r=31;
    if(g>31)g=31;
    if(blue>31)blue=31;
    return (uint16_t)(r|(g<<5)|(blue<<10));
}
static int prompt(const SsaCampaignScreen *s,uint16_t *out)
{
    const uint8_t *start=text(s,64),*p;unsigned width=0;int x;
    if(!start)return -1;
    for(p=start;*p;++p){unsigned glyph=s->rom[0x44c264+*p];
        if(*p==32)width+=4;
        else{if(glyph>=126)return -1;width+=s->rom[0xb7ea9+2*glyph]+1;}
    }
    x=164-(int)(width/2);
    for(p=start;*p;++p){unsigned glyph=s->rom[0x44c264+*p],y;
        if(*p==32){x+=4;continue;}
        for(y=0;y<8;++y){unsigned k;for(k=0;k<8;++k){
            int dx=x+(int)k-s->rom[0xb7ea8+2*glyph];
            unsigned color=(s->fonts[glyph*32+y*4+k/2]>>((k&1)*4))&15;
            if(color && dx>=0 && dx<240)out[(150+y)*240+(unsigned)dx]=s->palette[16+color];
        }}
        x+=s->rom[0xb7ea9+2*glyph]+1;
    }
    return 1;
}
int ssa_campaign_screen_draw(const SsaCampaignScreen *s,uint16_t *out,size_t pixels)
{
    unsigned x,y,i;if(!s || s->loaded!=1 || !out || pixels<38400 || s->story.image>=8)return -1;
    for(y=0;y<160;++y)for(x=0;x<240;++x){
        unsigned sx=(x+s->draw_scroll)&511,ty=y/8,tx=sx/8;
        unsigned tile=(64-(ty/8)*8+(ty&7)*64+tx)&511;
        out[y*240+x]=s->palette[s->tiles[tile*64+(y&7)*8+(sx&7)]];
    }
    if(s->text_enabled)for(y=0;y<160;++y)for(x=0;x<96;++x){
        unsigned sy=y+s->draw_text_scroll,row=(sy/8)%21,color=s->rows[row][(x/8)*64+(sy&7)*8+(x&7)];
        if(color)out[y*240+x]=blend(s->palette[color],out[y*240+x],s->blend_a,s->blend_b);
    }
    if(s->image_ready)for(i=4;i>0;--i){unsigned q=i-1;
        for(y=0;y<64;++y)for(x=0;x<64;++x){
            int dx=s->draw_positions[q][0]+(int)x,dy=s->draw_positions[q][1]+(int)y;
            unsigned sx=x+(q&1)*64,sy=y+(q&2)*32,at=((sy/8)*16+sx/8)*64+(sy&7)*8+(sx&7);
            unsigned color=s->panels[s->story.image][at];
            if(color && dx>=0 && dx<240 && dy>=0 && dy<160)out[(unsigned)dy*240+(unsigned)dx]=s->palette[color];
        }
    }
    if(s->text_enabled && (s->story.counter&32) && prompt(s,out)!=1)return -1;
    if(!s->alpha){unsigned darken=((unsigned)s->fade_volume>>8)&15;
        for(i=0;i<38400;++i)out[i]=blend(out[i],0,16-darken,0);}
    return 1;
}
