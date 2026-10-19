#include "ssa/intermission.h"
#include "ssa/display.h"
#include "ssa/menu.h"
#include <stdio.h>
#include <string.h>
enum {PAUSE_TOP=40};
static int make(SsaIntermission *s,unsigned type,unsigned sub,int y,unsigned scale){
    SsaWeaponObject *o;
    if(ssa_sprite_pool_allocate(&s->pool,&o)!=1)return -1;
    o->type=(uint16_t)type;o->subtype=(uint16_t)sub;o->y=y*256;o->layer=15;
    o->scale_x=o->scale_y=(uint16_t)scale;o->flags&=~UINT32_C(1);
    return ssa_weapon_animation_set(o,0x5c,5);
}
static int message(SsaIntermission *s,unsigned id,unsigned y){
    if(ssa_hud_notice_set(&s->notice,&s->hud,id,0,y,0)!=1)return -1;
    s->hud.notice_timer=30000;return 1;
}
static int completion_event(void *context,const SsaCompletion *t,SsaCompletionEvent kind,
    uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    SsaIntermission *s=context;(void)t;(void)b;(void)d;
    if(kind==SSA_COMPLETION_NOTICE)return message(s,a,c);
    if(s->command_count>=8)return -1;
    s->commands[s->command_count++]=a+(kind==SSA_COMPLETION_SOUND?256:0);return 1;
}
static int sprites(SsaIntermission *s){
    unsigned i,pass;const SsaNativeGame *g=s->game;
#define BAD() do{s->error_line=__LINE__;return -1;}while(0)
    s->hud.game_flags=s->completion?s->tally.flags:s->pause.flags;
    s->hud.score=s->tally.score;
    /* Stage0 can retain wrapped negative par-time difference for one tick.
     * It is not a bonus; do not expose original scratch overflow as digits. */
    s->bonus.score=(s->tally.stage==1 || s->tally.stage==4)?s->tally.pending:0;
    for(i=0;i<s->sprites.active_count;++i){SsaWeaponObject *o=s->sprites.objects[s->sprites.active[i]];
        if(o->type==13){if(ssa_hud_notice_step(o,&s->hud,&s->notice,&g->weapon_animations,1)<0)BAD();}
        else if(o->type==9){if(ssa_hud_sprite_step(o,&s->hud)<0)BAD();}
        else if(o->type==10){
            /* EE60 uses the same8-digit spacing and centering as ECF8. */
            if(!s->bonus.score)o->flags&=~UINT32_C(1);
            else{
                o->type=9;if(ssa_hud_sprite_step(o,&s->bonus)<0){o->type=10;BAD();}o->type=10;
                o->y=(int32_t)s->tally.bonus_y*256;
            }
        }
        if(ssa_weapon_animation_step(o,&g->weapon_animations,s->hud.game_flags,1)<0)BAD();
    }
    /* Presentation owns its own cache; resolve transfer readiness without
     * advancing the simulation or burning extra screen-animation ticks. */
    for(pass=0;pass<84;++pass){unsigned pending=s->cache.read_cursor!=s->cache.write_cursor;
        if(ssa_sprite_cache_prepare(&s->cache,&s->sprites,&g->sprite_assets,0)<0 ||
           ssa_sprite_cache_flush(&s->cache,&g->sprite_assets)<0)BAD();
        if(!pending && s->cache.read_cursor==s->cache.write_cursor)break;
    }
    if(pass==84)BAD();
    s->oam.counter=s->frame;
    if(ssa_sprite_oam_build(&s->oam,&s->sprites,&g->sprite_assets,s->cache.entries,&g->math)<0)BAD();
    return 1;
#undef BAD
}
int ssa_intermission_init(SsaIntermission *s,const SsaNativeGame *g,unsigned language,unsigned completion){
    unsigned i;
    if(!s || !g || !g->loaded || g->game.updating || g->error_line || language>=3 || completion>1)return -1;
    memset(s,0,sizeof(*s));s->game=g;s->completion=completion;
    s->classic=g->hud_style==SSA_HUD_CLASSIC;
    if(s->classic){
        if(ssa_map_view_init(&s->text,g->rom,g->rom_size,g->level,language,g->damage.difficulty)!=1)return -1;
        /* The overlay's zero means transparent. Keep opaque black font
         * outlines distinguishable without changing their RGB555 color. */
        for(i=0;i<256;++i)s->text.palette[i]|=0x8000;
    }
    for(i=0;i<84;++i)s->sprites.objects[i]=&s->objects[i];
    if(ssa_sprite_pool_init(&s->pool,&s->sprites)!=1 || ssa_sprite_cache_init(&s->cache)!=1 ||
       ssa_sprite_oam_init(&s->oam,g->rom,g->rom_size)!=1 ||
       ssa_hud_notice_init(&s->notice,g->rom,g->rom_size,language)!=1 ||
       ssa_hud_glyphs_load(&s->hud,g->rom,g->rom_size)!=1)return -1;
    s->bonus=s->hud;
    for(i=0;i<42;++i)if(make(s,13,i,48,0x800)<0)return -1;
    if(completion){
        if(ssa_native_completion_init(&s->tally,g)!=1)return -1;
        for(i=0;i<8;++i)if(make(s,9,i,8,0x1000)<0)return -1;
        for(i=0;i<8;++i)if(make(s,10,i,120,0x1000)<0)return -1;
    }else{
        if(g->finished || g->players[0].inventory.health<=0)return -1;
        s->pause.flags=(uint16_t)(g->frame_game.flags&~0x900u);
        if(ssa_pause_step(&s->pause,0x80000)<0 || message(s,0xb1,PAUSE_TOP)<0)return -1;
        for(i=0;i<s->pause.event_count;++i)if(s->pause.events[i]<256)s->commands[s->command_count++]=s->pause.events[i];
    }
    s->loaded=1;return sprites(s);
}
int ssa_intermission_step(SsaIntermission *s,uint32_t held,uint32_t pressed,unsigned voice_finished){
    unsigned i;int result;
    if(!s || !s->loaded)return -1;
    s->command_count=0;++s->frame;
    if(s->completion){SsaCompletionOps ops={completion_event,s};
        unsigned finished;
        if(voice_finished>1)return -1;
        /* PC confirm skips the current count, including its initial hold.
         * Let the recovered controller perform the phase change/notices. */
        if((pressed&1) && (s->tally.stage==1 || s->tally.stage==4)){
            s->tally.score+=s->tally.pending;s->tally.pending=0;s->tally.hold=0;
        }
        finished=voice_finished && (s->final_visible_ticks>=180 ||
            (s->final_visible_ticks>=60 && (pressed&1)));
        result=ssa_completion_step(&s->tally,held,pressed,finished,&ops);if(result<0)return -1;
    }else{
        if(ssa_pause_step(&s->pause,pressed)<0)return -1;
        for(i=0;i<s->pause.event_count;++i){unsigned event=s->pause.events[i];
            if(event>=256){if(message(s,event-256,PAUSE_TOP)<0)return -1;}
            else s->commands[s->command_count++]=event;
        }
        result=(s->pause.flags&1)!=0;
    }
    if(sprites(s)<0)return -1;
    /* Count readable frames, not the glyph-transfer/setup frames. Neither
     * held fire nor a bonus-skip edge bypasses this final reading window. */
    if(s->completion && s->notice.id==0xb9u+s->tally.level &&
       (s->hud.game_flags&0x200) && s->final_visible_ticks<180)++s->final_visible_ticks;
    return result;
}
uint32_t ssa_intermission_pointer(SsaIntermission *s,int x,int y,unsigned click){
    unsigned row;int top;
    if(!s || !s->loaded || s->completion || x<24 || x>=216)return 0;
    top=PAUSE_TOP+(s->classic?32:16);
    if(y<top || y>=top+32)return 0;
    row=(unsigned)(y-top)/16;s->pause.selection=(uint8_t)((s->pause.selection>=2?2:0)+row);
    return click?0x10000:0;
}
static int classic_layer(SsaIntermission *s,uint16_t *layer){
    const char *p=(const char *)s->notice.text;char line[512],number[16];unsigned row=0;
    s->text.glyph_count=0;memset(layer,0,38400*sizeof(*layer));
    if(s->hud.game_flags&0x200)while(*p){
        const char *end=strchr(p,'\n');size_t length=end?(size_t)(end-p):strlen(p);
        unsigned palette=4,font;
        if(length>=sizeof line)return -1;
        memcpy(line,p,length);line[length]=0;
        if(!s->completion){if((!row && s->notice.id!=0xb1) || row==1u+(s->pause.selection&1u))palette=3;}
        else if(!strcmp(line,"0"))palette=3;
        font=ssa_map_view_text_width(&s->text,line,0)>224?1:0;
        if(ssa_map_view_text_centered(&s->text,line,s->notice.base_y+(int)row*16+
            (!s->completion && row?16:0),font,palette,0)<0)return -1;
        ++row;if(!end)break;p=end+1;
    }
    if(s->completion){
        snprintf(number,sizeof number,"%u",(unsigned)s->tally.score);
        if(ssa_map_view_text_centered(&s->text,number,8,0,3,0)<0)return -1;
        if(s->bonus.score){
            snprintf(number,sizeof number,"%u",(unsigned)s->bonus.score);
            if(ssa_map_view_text_centered(&s->text,number,(int)s->tally.bonus_y,0,3,0)<0)return -1;
        }
    }
    return ssa_map_view_overlay(&s->text,layer,38400);
}
int ssa_intermission_overlay(SsaIntermission *s,const uint32_t *back,unsigned w,unsigned h,uint32_t *out){
    uint8_t world[14400]={0};uint16_t palette[256]={0},layer[38400];SsaDisplayRegisters registers;
    int left,top,width,height;unsigned x,y,alpha,shade;size_t i;
    if(!s || !s->loaded || !back || !out || !w || !h || w>INT32_MAX || h>INT32_MAX ||
       (uint64_t)w*h>SIZE_MAX/sizeof(*out) ||
       !ssa_ui_viewport((int)w,(int)h,240,160,&left,&top,&width,&height))return -1;
    if(s->classic){if(classic_layer(s,layer)<0)return -1;}
    else if(ssa_display_registers_default(&registers)<0 ||
       ssa_display_compose(&registers,world,sizeof(world),palette,s->game->sprite_assets.palette,&s->oam,
        s->cache.tiles,sizeof(s->cache.tiles),layer,38400)<0)return -1;
    alpha=s->completion?(s->tally.blend&31):16;if(alpha>16)alpha=16;
    shade=s->completion && s->frame<30?32-s->frame*16/30:16;
    for(i=0;i<(size_t)w*h;++i){unsigned c=back[i];
        out[i]=((((c>>16)&255)*shade/32)<<16)|((((c>>8)&255)*shade/32)<<8)|((c&255)*shade/32);}
    for(y=0;y<(unsigned)height;++y)for(x=0;x<(unsigned)width;++x){
        unsigned sy=(unsigned)((uint64_t)y*160/(unsigned)height),sx=(unsigned)((uint64_t)x*240/(unsigned)width),c=layer[sy*240+sx];
        if(c){unsigned r=(c&31)*255/31,g=((c>>5)&31)*255/31,b=((c>>10)&31)*255/31;
            /* Retain the glyph outline; make the selected row's fill white. */
            if(!s->classic && !s->completion && sy>=PAUSE_TOP+16u+16u*(s->pause.selection&1u) && sy<PAUSE_TOP+32u+16u*(s->pause.selection&1u) && g>=r/2)
                b=g=r;
            size_t at=(size_t)(top+(int)y)*w+left+x;unsigned bg=out[at];
            r=(r*alpha+((bg>>16)&255)*(16-alpha))/16;
            g=(g*alpha+((bg>>8)&255)*(16-alpha))/16;b=(b*alpha+(bg&255)*(16-alpha))/16;
            out[at]=(r<<16)|(g<<8)|b;
        }
    }
    return 1;
}
