#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,draws,paths;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Classic HUD line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t hash(const uint8_t *p){unsigned i;uint32_t n=2166136261u;for(i=0;i<38400;++i)n=(n^p[i])*16777619u;return n;}
static uint32_t hud_color(const SsaNativeGame *g,unsigned index)
{unsigned c=g->sprite_assets.palette[index],r=c&31,gg=(c>>5)&31,b=(c>>10)&31;
 return ((r*8+r/4)<<16)|((gg*8+gg/4)<<8)|(b*8+b/4);}
static int outside_equal(const uint8_t *a,const uint8_t *b,unsigned x,unsigned y,unsigned w,unsigned h)
{unsigned i;for(i=0;i<38400;++i)if(!(i%240>=x && i%240<x+w && i/240>=y && i/240<y+h) && a[i]!=b[i])return 0;return 1;}
static void original_reference(const SsaNativeGame *g)
{
    SsaSpriteOam reference=g->oam;SsaSpriteDispatch dispatch=g->sprites;SsaWeaponObject objects[84];
    uint16_t pixels[38400];unsigned i;
    CHECK(g->hud_style==SSA_HUD_ORIGINAL && !g->alternates.enabled);
    for(i=0;i<84;++i){objects[i]=*dispatch.objects[i];dispatch.objects[i]=&objects[i];}
    /* Bypass the PC HUD layout adapter entirely. Feed the original controller
     * coordinates directly to the recovered FCE4 builder and native display. */
    CHECK(ssa_sprite_oam_build(&reference,&dispatch,&g->sprite_assets,g->cache.entries,&g->math)==1);
    CHECK(reference.count==g->oam.count && reference.affine_count==g->oam.affine_count);
    CHECK(!memcmp(reference.entries,g->oam.entries,sizeof reference.entries));
    CHECK(ssa_display_compose_gameplay(&g->registers,ssa_world_scene_pixels(&g->world_scene),14400,
        g->palette_effect.colors,g->sprite_assets.palette,&reference,g->cache.tiles,sizeof g->cache.tiles,
        pixels,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)==1);
    for(i=0;i<38400;++i)CHECK(pixels[i]==g->rgb555[i]);
}
static void original_anchors(const SsaNativeGame *g,const SsaPcRenderer *pc)
{
    uint8_t zero[14400]={0};uint16_t black[256]={0},white[256],a[38400],b[38400];unsigned group,i,lit=0;
    double scale=pc->width/240.0<pc->height/160.0?pc->width/240.0:pc->height/160.0;
    double left=(pc->width-240*scale)/2,top=(pc->height-160*scale)/2;
    for(i=0;i<256;++i)white[i]=32767;
    for(group=1;group<6;++group){double ox=group==2 || group==3?0:group==1?left:2*left;
        double oy=group==3 || group==5?2*top:0;
        CHECK(ssa_display_compose_gameplay(&g->registers,zero,sizeof zero,black,g->sprite_assets.palette,&g->original_hud_oam[group],
            g->cache.tiles,sizeof g->cache.tiles,a,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)==1);
        CHECK(ssa_display_compose_gameplay(&g->registers,zero,sizeof zero,white,g->sprite_assets.palette,&g->original_hud_oam[group],
            g->cache.tiles,sizeof g->cache.tiles,b,38400,g->players[0].inventory.health,g->players[0].inventory.stat_c4)==1);
        for(i=0;i<38400;++i)if(a[i]==b[i]){unsigned c=a[i],x=(unsigned)(ox+(i%240+0.5)*scale),y=(unsigned)(oy+(i/240+0.5)*scale);
            uint32_t rgb=(((c&31)*8+(c&31)/4)<<16)|((((c>>5)&31)*8+((c>>5)&31)/4)<<8)|(((c>>10)&31)*8+((c>>10)&31)/4);
            CHECK(x<pc->width && y<pc->height && pc->pixels[y*pc->width+x]==rgb);++lit;}
    }
    CHECK(lit>300);
}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void screenshot(const char *directory,const char *kind,unsigned level,const uint32_t *pixels,unsigned w,unsigned h)
{
    char path[1024];uint8_t header[54]={0};FILE *f;
    CHECK(snprintf(path,sizeof path,"%s/classic-%s-%u.bmp",directory,kind,level)>0);
    f=fopen(path,"wb");CHECK(f);header[0]='B';header[1]='M';put32(header+2,54+w*h*4);
    put32(header+10,54);put32(header+14,40);put32(header+18,w);put32(header+22,0u-h);header[26]=1;header[28]=32;
    CHECK(fwrite(header,1,54,f)==54 && fwrite(pixels,4,(size_t)w*h,f)==(size_t)w*h);CHECK(!fclose(f));
}
static void pending_highlight(SsaNativeGame *g,SsaNativeGame *copy,unsigned selected)
{
    uint8_t canvas[38400],baseline[38400];unsigned slot,x,y,c,white=0;
    CHECK(g->gameplay.weapon.current==2 && g->gameplay.weapon.requested==selected);
    *copy=*g;CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
    copy->gameplay.weapon.requested=1;CHECK(ssa_native_game_classic_hud(copy,baseline,sizeof baseline)==1);
    for(slot=0;slot<10;++slot){c=g->sprite_assets.palette[canvas[103*240+35+slot*17+3]]&0x7fff;
        CHECK(c && (slot+1==selected?((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31)):!(c&0x7c1f)));}
    /* Packed ammo interiors track selection too; their frames and idle meters
     * remain green. Inspect art left of the separate right-hand meter. */
    for(slot=0;slot<8;++slot){unsigned left=111+slot*16;
        c=g->sprite_assets.palette[canvas[142*240+left+3]]&0x7fff;CHECK(c && !(c&0x7c1f));
        for(y=143;y<156;++y)for(x=left+1;x<left+10;++x){unsigned at=y*240+x,original=g->sprite_assets.palette[baseline[at]]&0x7fff;
            c=g->sprite_assets.palette[canvas[at]]&0x7fff;
            if(slot+3==selected && original && !(original&0x7c1f)){
                CHECK((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31));++white;
            }else CHECK(canvas[at]==baseline[at]);}
    }
    CHECK(white>8);
}
static void weapon_wheel(SsaNativeGame *g,SsaNativeGame *copy,SsaPcRenderer *pc,const char *directory)
{
    uint8_t canvas[38400],old[38400],background[38400];unsigned i,k,c,lit,green_shades,white_shades;
    uint32_t shots;
    CHECK(!g->hud_weapon_ticks);
    g->players[0].inventory.health=32767;
    g->players[0].inventory.weapons=0x3ff;
    for(i=0;i<10;++i)g->players[0].inventory.ammo[i]=(int16_t)g->inventory_rules.ammo_capacity[i];
    g->alternates.dual_colts=1;
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_native_game_classic_hud(g,old,sizeof old)==1);
    CHECK(ssa_native_game_cycle_weapon(g,1)==1 && g->alternates.requested_variant==1 && g->hud_weapon_ticks==180);
    *copy=*g;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
    CHECK(outside_equal(old,canvas,0,103,240,24));
    /* Ten centered entries: single/dual Colts share the same original icon. */
    for(i=0;i<10;++i){unsigned x=35+i*17;
        CHECK(background[103*240+x] && background[119*240+x+15]);
        c=g->sprite_assets.palette[canvas[103*240+x+3]]&0x7fff;
        CHECK(c && (i==1?((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31)):!(c&0x7c1f)));
        if(i<9)CHECK(!background[103*240+x+16]);
    }
    CHECK(!background[103*240+34] && !background[103*240+205]);
    memcpy(old,canvas,sizeof old);
    for(i=0;i<100 && g->alternates.variant!=1;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->alternates.variant==1 && g->hud_weapon_ticks==180);
    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
    CHECK(!memcmp(old+103*240,canvas+103*240,17*240));
    lit=green_shades=white_shades=0;
    for(i=103;i<120;++i)for(k=52;k<68;++k){c=g->sprite_assets.palette[canvas[i*240+k]]&0x7fff;
        if(c){++lit;CHECK((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31));white_shades|=1u<<(c&31);}}
    for(i=103;i<120;++i)for(k=69;k<85;++k){c=g->sprite_assets.palette[canvas[i*240+k]]&0x7fff;
        if(c){CHECK(!(c&0x7c1f));green_shades|=1u<<((c>>5)&31);}}
    CHECK(lit>20 && (white_shades&(white_shades-1)) && (green_shades&(green_shades-1)));
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    if(directory){screenshot(directory,"wheel-gba",0,g->rgb888,240,160);
        screenshot(directory,"wheel-pc",0,pc->pixels,pc->width,pc->height);}
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_ORIGINAL)==1);
    CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
    for(i=0;i<38400;++i)CHECK(!canvas[i] && !background[i]);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    g->hud_suppressed=1;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
    for(i=0;i<38400;++i)CHECK(!canvas[i] && !background[i]);
    g->hud_suppressed=0;
    /* A failed choice never opens/extends it. Timeout uses simulation time;
     * redraw and pause do not age it. */
    g->frame_game.flags|=1;
    for(i=0;i<3;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_weapon_ticks==180);
    g->frame_game.flags&=(uint16_t)~1u;
    for(i=0;i<179;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_weapon_ticks);
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_weapon_ticks);
    CHECK(ssa_native_game_select_weapon(g,4)==1 && g->hud_weapon_ticks==180);
    pending_highlight(g,copy,4);
    CHECK(ssa_native_game_select_weapon(g,6)==1);pending_highlight(g,copy,6);
    CHECK(ssa_native_game_select_weapon(g,4)==1);pending_highlight(g,copy,4);
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    if(directory){screenshot(directory,"pending-gba",0,g->rgb888,240,160);
        screenshot(directory,"pending-pc",0,pc->pixels,pc->width,pc->height);}
    for(i=0;i<120 && g->gameplay.weapon.current!=4;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->gameplay.weapon.current==4 && g->hud_weapon_ticks==180);
    CHECK(ssa_native_game_previous_weapon(g)==1 && g->gameplay.weapon.requested==2 && g->alternates.requested_variant==1);
    for(i=0;i<120;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->gameplay.weapon.current==2 && g->alternates.variant==1 && g->hud_weapon_ticks);
    /* Even an infinite-ammo Colt's very short click dismisses the wheel. */
    shots=g->weapon_game.shots[1];
    CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,1)==1);
    CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,0)==1);
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_weapon_ticks);
    for(i=0;i<60 && g->weapon_game.shots[1]==shots;++i)CHECK(ssa_native_game_tick(g)==1 && !g->hud_weapon_ticks);
    CHECK(g->weapon_game.shots[1]!=shots);
    CHECK(ssa_native_game_select_weapon(g,4)==1 && g->hud_weapon_ticks==180);
    shots=g->weapon_game.shots[3];
    CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,1)==1);
    CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,0)==1);
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_weapon_ticks);
    for(i=0;i<180 && g->weapon_game.shots[3]==shots;++i)CHECK(ssa_native_game_tick(g)==1 && !g->hud_weapon_ticks);
    CHECK(g->weapon_game.shots[3]!=shots && g->gameplay.weapon.current==4);
}
static void damage_and_text(SsaNativeGame *g,SsaNativeGame *copy,SsaPcRenderer *pc,const char *directory)
{
    uint8_t canvas[38400],background[38400];unsigned i,k,c,lit=0,shades=0;int16_t health=g->players[0].inventory.health;
    g->players[0].entity.body.flags&=(uint16_t)~1u;g->damage.hurt_flash=0;
    CHECK(ssa_combat_runtime_damage(&g->combat,&g->players[0].entity,&g->players[0].entity,NULL,50,&g->combat_ops)==0);
    CHECK(g->players[0].inventory.health<health);
    g->damage.god_mode=1;
    CHECK(ssa_native_game_tick(g)==1 && g->hud_health_flash==40);
    *copy=*g;CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
    for(i=143;i<154;++i)for(k=21;k<45;++k){c=g->sprite_assets.palette[canvas[i*240+k]]&0x7fff;
        if(c){++lit;CHECK((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31));shades|=1u<<(c&31);}}
    CHECK(lit>8 && (shades&(shades-1)));
    g->frame_game.flags|=1;
    for(i=0;i<3;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_health_flash==40);
    g->frame_game.flags&=(uint16_t)~1u;
    for(i=0;i<39;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_health_flash);
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_health_flash);
    ++g->players[0].inventory.health;
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_health_flash);
    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
    for(i=143;i<154;++i)for(k=21;k<45;++k)CHECK(!(g->sprite_assets.palette[canvas[i*240+k]]&0x7c1f));
    /* Original localization/timer stays authoritative; Classic draws a static
     * small white font below the selector, without animated red/yellow glyphs. */
    for(k=0;k<3;++k){unsigned letters=0,tones=0;
        g->hud_notice.language=k;
        CHECK(ssa_hud_notice_set(&g->hud_notice,&g->hud,0x8d,0,48,0)==1);
        g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=120;
        g->hud_weapon_ticks=180;
        CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
        CHECK(g->hud_notice.base_y==48 && background[103*240+35]);
        for(i=40*240;i<64*240;++i)CHECK(!canvas[i] && !background[i]);
        for(i=127*240;i<135*240;++i){c=g->sprite_assets.palette[canvas[i]]&0x7fff;
            CHECK((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31));
            if(c){++letters;tones|=1u<<(c&31);}}
        CHECK(letters>20 && (tones&(tones-1)));
        CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        if(directory && !k){screenshot(directory,"text-gba",0,g->rgb888,240,160);
            screenshot(directory,"text-pc",0,pc->pixels,pc->width,pc->height);}
    }
    CHECK(ssa_classic_hud_replaces(13) && ssa_classic_hud_replaces(14));
    /* Game Over also uses white text, at its original central position. */
    CHECK(ssa_hud_notice_set(&g->hud_notice,&g->hud,0xb4,0,64,0)==1);
    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
    lit=0;
    for(i=64*240;i<72*240;++i){c=g->sprite_assets.palette[canvas[i]]&0x7fff;
        CHECK((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31));lit+=c!=0;}
    CHECK(lit>20);
    g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=0;
    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
    for(i=64*240;i<72*240;++i)CHECK(!canvas[i]);
}
typedef struct NotificationAudio {
    SsaNativeGame *game;SsaAudio *mixer;unsigned legacy,source;
} NotificationAudio;
static void notification_play(void *context,unsigned id)
{
    NotificationAudio *s=context;
    if(id==0x0b){CHECK(s->game->hud_style==SSA_HUD_CLASSIC &&
        (s->game->hud_netricsa_ticks==300 || s->game->hud_netricsa_ticks==290));++s->legacy;}
    CHECK(ssa_audio_play(s->mixer,id)>=0);
}
static void notification_stop(void *context,unsigned id)
{NotificationAudio *s=context;CHECK(ssa_audio_stop(s->mixer,id)>=0);}
static void notification_source(void *context,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point)
{
    NotificationAudio *s=context;
    if(command==SSA_AUDIO_SOURCE_CUE_TWICE && id==0x0b){
        CHECK(source==1 && !point && s->game->hud_style==SSA_HUD_CLASSIC &&
            s->game->hud_netricsa_ticks==300);++s->source;}
    CHECK(ssa_audio_source(s->mixer,command,id,source,point)>=0);
}
static unsigned notification_queued(const SsaAudio *a)
{unsigned i,count=0;for(i=1;i<SSA_AUDIO_PLAYERS;++i)count+=a->players[i].cue_repeats;return count;}
static void notification_chain(const uint8_t *rom)
{
    SsaAudio *single=calloc(1,sizeof(*single)),*pair=calloc(1,sizeof(*pair));unsigned rate,i,j;
    int16_t first[512],second[512];
    CHECK(single && pair);
    for(rate=24000;rate<=48000;rate+=24000){unsigned slot=0,end=0,voices;
        memset(single,0,sizeof(*single));memset(pair,0,sizeof(*pair));
        CHECK(ssa_audio_init(single,rom,0x800000,rate)==1 && ssa_audio_init(pair,rom,0x800000,rate)==1);
        CHECK(ssa_audio_source(single,SSA_AUDIO_SOURCE_PLAY,0x0b,1,NULL)==1);
        CHECK(ssa_audio_source(pair,SSA_AUDIO_SOURCE_CUE_TWICE,0x0b,1,NULL)==1);
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(pair->players[i].source==1)slot=i;
        CHECK(slot && notification_queued(pair)==1);
        for(i=0;i<rate/2;++i){
            CHECK(ssa_audio_render(single,first,1)==1 && ssa_audio_render(pair,second,1)==1);
            CHECK(!memcmp(first,second,2*sizeof(*first)) && pair->starts==1);
            voices=0;for(j=SSA_AUDIO_MUSIC_VOICES;j<SSA_AUDIO_VOICES;++j)voices|=single->voices[j].active;
            if(!voices){end=i+1;break;}
        }
        CHECK(end>rate/10 && end<rate/8); /* Audible burst is about.108s, not the1.18s sequence. */
        CHECK(pair->players[slot].active && notification_queued(pair)==1);
        /* Wait precisely half the first-burst duration in silence. The second cue
         * must not start early or wait for the ROM's long sequence padding. */
        CHECK(pair->players[slot].cue_frames==end);
        for(i=0;i<(end+1)/2;++i){
            CHECK(ssa_audio_render(single,first,1)==1 && ssa_audio_render(pair,second,1)==1);
            CHECK(!second[0] && !second[1] && pair->starts==1 && notification_queued(pair)==1);}
        CHECK(pair->players[slot].cue_started==2 && !pair->players[slot].cue_gap);
        /* Manual restart after that gap is the sample-exact reference. */
        CHECK(ssa_audio_source(single,SSA_AUDIO_SOURCE_PLAY,0x0b,1,NULL)==1);
        CHECK(ssa_audio_render(single,first,1)==1 && ssa_audio_render(pair,second,1)==1);
        CHECK(pair->starts==2 && !notification_queued(pair) && !memcmp(first,second,2*sizeof(*first)));
        for(i=0;i<rate/128;++i){
            CHECK(ssa_audio_render(single,first,256)==1 && ssa_audio_render(pair,second,256)==1);
            CHECK(!memcmp(first,second,sizeof first) && pair->starts==2);
        }
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)CHECK(!pair->players[i].active);
        for(i=0;i<6;++i){uint64_t starts=pair->starts;
            ssa_audio_reset(pair);
            CHECK(ssa_audio_source(pair,SSA_AUDIO_SOURCE_CUE_TWICE,0x0b,1,NULL)==1);
            if(i==0)CHECK(ssa_audio_source(pair,SSA_AUDIO_SOURCE_CANCEL_REPEAT,0x0b,1,NULL)==1);
            if(i==1)CHECK(ssa_audio_source(pair,SSA_AUDIO_SOURCE_STOP,0x0b,1,NULL)==1);
            if(i==2)ssa_audio_reset(pair);
            if(i>=4){for(j=0;j<=end;++j)CHECK(ssa_audio_render(pair,first,1)==1);
                CHECK(pair->players[slot].cue_started==2 && pair->players[slot].cue_gap);
                if(i==4)CHECK(ssa_audio_source(pair,SSA_AUDIO_SOURCE_CANCEL_REPEAT,0x0b,1,NULL)==1);}
            if(i==3 || i==5){uint64_t gap=pair->players[slot].cue_gap;
                CHECK(ssa_audio_music(pair,4,0)==1);
                for(j=0;j<rate/256;++j)CHECK(ssa_audio_render(pair,first,256)==1);
                CHECK(pair->starts==starts+1 && notification_queued(pair)==1 && pair->players[slot].cue_gap==gap);
                CHECK(ssa_audio_music(pair,5,0)==1);}
            for(j=0;j<rate/128;++j)CHECK(ssa_audio_render(pair,first,256)==1);
            CHECK(pair->starts==starts+(i==3 || i==5?2u:1u) && !notification_queued(pair));
        }
    }
    free(single);free(pair);
}
static void netricsa_notification(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom,
    SsaPcRenderer *pc,const char *directory)
{
    uint8_t white[38400],green[38400],background[38400],sprite[640];unsigned i,x,y,lit=0;
    SsaAudio *mixer=calloc(1,sizeof(*mixer));NotificationAudio sink={g,mixer,0,0};SsaAudioPlayer music;
    SsaNativeAudioOps audio={notification_play,notification_stop,&sink,NULL,NULL,NULL,NULL,NULL};
    CHECK(mixer && ssa_audio_init(mixer,rom,0x800000,24000)==1);
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,0,NULL)==1);
    CHECK(ssa_native_game_bind_audio(g,&audio)==1 && ssa_audio_level_music(mixer,0)==1);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    CHECK(ssa_native_game_netricsa_unread(g)==0);
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_netricsa_ticks && !sink.legacy && !sink.source);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,3)==1);
    music=mixer->players[0];
    CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==300);
    CHECK(sink.legacy==1 && !sink.source && !memcmp(&music,&mixer->players[0],sizeof music));
    g->damage.god_mode=1;
    *copy=*g;
    CHECK(ssa_native_game_classic_hud_layers(g,white,background,sizeof white)==1 && !memcmp(copy,g,sizeof(*g)));
    CHECK(!background[28*240+100] && !background[28*240+99]);
    CHECK(ssa_sprite_image_indices(&g->sprite_assets.images[79],sprite,40,sizeof sprite)==1);
    { /* Expected corrected lettering, including the y descender and shadows. */
        static const char *rows[10]={
            "+#+         #",
            "#.#         #         #",
            "# # ##+ ##+ # # # +## . ##+ +##",
            "### #.# ..# # # # #.. # #.# #.#",
            "#.# # # +## # # # +#+ # # # # #",
            "# # # # #.# # # # ..# # # # # #",
            "# # # # +## # +## ##+ # # # +## # # #",
            ". . . . ... . ..# ... . . . ..# . . .",
            "              ##+           ##+",
            "              ...           ..."
        };
        memset(sprite,0,sizeof sprite);
        for(y=0;y<10;++y)for(x=0;rows[y][x];++x){unsigned c=(unsigned)rows[y][x];
            sprite[y*40+x]=(uint8_t)(c=='#'?0x4c:c=='+'?0x4d:c=='.'?0x90:0);}
    }
    for(y=0;y<16;++y)for(x=0;x<40;++x){
        unsigned source=sprite[y*40+x],index=white[(28+y)*240+100+x];
        CHECK((index!=0)==(source!=0) && !background[(28+y)*240+100+x]);
        if(source){unsigned old_color=g->sprite_assets.palette[source]&0x7fff,color=g->sprite_assets.palette[index]&0x7fff;
            CHECK(color==(!old_color?0u:((old_color>>5)==24?29u:24u)<<5));}
        lit+=source!=0;}
    CHECK(lit>30);
    /* The on phase uses the complete HUD green ramp in both compositors. */
    {uint16_t behind555[38400];uint32_t behind888[640];
        g->hud_suppressed=1;CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        memcpy(behind555,g->rgb555,sizeof behind555);
        for(y=0;y<16;++y)for(x=0;x<40;++x)behind888[y*40+x]=pc->pixels[((28+y)*2)*pc->width+(100+x)*2];
        g->hud_suppressed=0;CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        for(y=0;y<16;++y)for(x=0;x<40;++x)if(sprite[y*40+x]){unsigned p=(28+y)*240+100+x;
            CHECK(g->rgb555[p]==ssa_classic_hud_blend555(g->sprite_assets.palette[white[p]]&0x7fff,behind555[p],255));
            CHECK(pc->pixels[((28+y)*2)*pc->width+(100+x)*2]==ssa_classic_hud_blend888(hud_color(g,white[p]),behind888[y*40+x],255));}}
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    if(directory){screenshot(directory,"analyzing-white-gba",0,g->rgb888,240,160);
        screenshot(directory,"analyzing-white-pc",0,pc->pixels,pc->width,pc->height);}
    for(i=0;i<9;++i)CHECK(ssa_native_game_tick(g)==1 && sink.legacy==1);
    CHECK(ssa_native_game_tick(g)==1 && sink.legacy==2 && g->hud_netricsa_ticks==290);
    for(i=0;i<5;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(sink.legacy==2 && !sink.source);
    CHECK(g->hud_netricsa_ticks==285 && ssa_native_game_classic_hud(g,green,sizeof green)==1);
    CHECK(hash(white)!=hash(green) && outside_equal(white,green,215,2,24,16));
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    {uint16_t behind555[38400];uint32_t behind888[640];
        g->hud_suppressed=1;CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        memcpy(behind555,g->rgb555,sizeof behind555);
        for(y=0;y<16;++y)for(x=0;x<40;++x)behind888[y*40+x]=pc->pixels[((28+y)*2)*pc->width+(100+x)*2];
        g->hud_suppressed=0;CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        for(y=0;y<16;++y)for(x=0;x<40;++x){unsigned p=(28+y)*240+100+x;
            CHECK(g->rgb555[p]==behind555[p]);
            CHECK(pc->pixels[((28+y)*2)*pc->width+(100+x)*2]==behind888[y*40+x]);}}
    if(directory){screenshot(directory,"analyzing-green-gba",0,g->rgb888,240,160);
        screenshot(directory,"analyzing-green-pc",0,pc->pixels,pc->width,pc->height);}
    g->frame_game.flags|=1;
    for(i=0;i<3;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==285);
    g->frame_game.flags&=(uint16_t)~1u;
    CHECK(!g->hud_netricsa_repeat && !memcmp(&music,&mixer->players[0],sizeof music));
    for(i=0;i<105;++i)CHECK(ssa_native_game_tick(g)==1 && sink.legacy==2);
    CHECK(g->hud_netricsa_ticks==180 && ssa_native_game_classic_hud_layers(g,green,background,sizeof green)==1);
    CHECK(background[28*240+108] && !background[2*240+215]);
    for(i=0;i<30;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->hud_netricsa_ticks==150 && ssa_native_game_classic_hud_layers(g,green,background,sizeof green)==1);
    CHECK(background[28*240+108] && !background[2*240+215]);
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    if(directory){screenshot(directory,"inbox-center-gba",0,g->rgb888,240,160);
        screenshot(directory,"inbox-center-pc",0,pc->pixels,pc->width,pc->height);}
    CHECK(ssa_pc_renderer_resize(pc,854,480)==1 && ssa_pc_renderer_draw(pc,g)==1);++draws;
    CHECK(pc->pixels[84*854+391]==hud_color(g,green[28*240+108]));
    if(directory)screenshot(directory,"inbox-center-wide",0,pc->pixels,pc->width,pc->height);
    CHECK(ssa_pc_renderer_resize(pc,480,320)==1);
    for(i=0;i<120;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->hud_netricsa_ticks==30 && ssa_native_game_classic_hud(g,white,sizeof white)==1);
    for(y=28;y<44;++y)for(x=108;x<132;++x)CHECK(!white[y*240+x]); /* Already snapped back. */
    for(i=0;i<15;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(g->hud_netricsa_ticks==15 && ssa_native_game_classic_hud_layers(g,green,background,sizeof green)==1);
    CHECK(background[2*240+215] && !background[28*240+108] && !background[15*240+161]);
    for(i=0;i<15;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(!g->hud_netricsa_ticks && ssa_native_game_netricsa_unread(g)==1);
    CHECK(sink.legacy==2 && !sink.source);
    audio.source=notification_source;CHECK(ssa_native_game_bind_audio(g,&audio)==1);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,4)==1);
    music=mixer->players[0];
    CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==300);
    CHECK(sink.legacy==2 && sink.source==1 && !memcmp(&music,&mixer->players[0],sizeof music));
    CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==299);
    CHECK(notification_queued(mixer)==1);
    CHECK(ssa_native_game_netricsa_read(g,15,0)==1 && !g->hud_netricsa_ticks && !notification_queued(mixer));
    for(i=0;i<80;++i)CHECK(ssa_native_game_tick(g)==1 && !g->hud_netricsa_ticks);
    CHECK(sink.legacy==2 && sink.source==1); /* Reading cancels the pending repeat. */
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,5)==1);
    CHECK(ssa_native_game_tick(g)==1 && sink.source==2 && g->hud_netricsa_repeat);
    CHECK(notification_queued(mixer)==1);
    {int16_t block[800];uint64_t starts=mixer->starts;
        for(i=0;i<20;++i)CHECK(ssa_audio_render(mixer,block,400)==1);
        CHECK(mixer->starts==starts+1 && !notification_queued(mixer));}
    music=mixer->players[0];
    for(i=0;i<110;++i)CHECK(ssa_native_game_tick(g)==1 && sink.source==2);
    CHECK(!memcmp(&music,&mixer->players[0],sizeof music));
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,6)==1);
    CHECK(ssa_native_game_tick(g)==1 && sink.source==3 && g->hud_netricsa_repeat);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_ORIGINAL)==1 && !g->hud_netricsa_repeat && !notification_queued(mixer));
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    for(i=0;i<80;++i)CHECK(ssa_native_game_tick(g)==1 && sink.source==3);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_ORIGINAL)==1);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,7)==1);
    CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==300 && !g->hud_netricsa_repeat);
    for(i=0;i<80;++i)CHECK(ssa_native_game_tick(g)==1 && sink.source==3);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    for(i=0;i<80;++i)CHECK(ssa_native_game_tick(g)==1 && sink.source==3);
    /* Original is silent through the legacy fallback as well. */
    audio.source=NULL;CHECK(ssa_native_game_bind_audio(g,&audio)==1);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_ORIGINAL)==1);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,8)==1);
    CHECK(ssa_native_game_tick(g)==1 && g->hud_netricsa_ticks==300);
    for(i=0;i<80;++i)CHECK(ssa_native_game_tick(g)==1 && sink.legacy==2 && sink.source==3);
    memset(white,0,sizeof white);memset(green,0,sizeof green);
    CHECK(ssa_classic_hud_netricsa(&g->sprite_assets,&g->weapon_animations,22,150,0,
        white,NULL,sizeof white)==1);
    CHECK(outside_equal(green,white,108,28,23,16)); /* Both digits stay inside the retained frame. */
    CHECK(ssa_native_game_bind_audio(g,NULL)==1);ssa_native_game_free(g);
    /* The real ROM cue produces audio, completes by itself, and follows the
     * effects slider even with music disabled. No physical device is opened. */
    {int16_t samples[800];unsigned tick,energy=0;
        ssa_audio_reset(mixer);mixer->music_enabled=0;
        CHECK(ssa_audio_set_volume(mixer,100,100)==1 && ssa_audio_source(mixer,SSA_AUDIO_SOURCE_PLAY,0x0b,1,NULL)==1);
        for(tick=0;tick<120;++tick){CHECK(ssa_audio_render(mixer,samples,400)==1);
            for(i=0;i<800;++i)energy|=samples[i]!=0;
            if(tick==74)for(i=1;i<SSA_AUDIO_PLAYERS;++i)CHECK(!mixer->players[i].active);}
        CHECK(energy && !mixer->errors);
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)CHECK(!mixer->players[i].active);
        ssa_audio_reset(mixer);CHECK(ssa_audio_set_volume(mixer,100,0)==1 && ssa_audio_play(mixer,0x0b)==1);
        CHECK(ssa_audio_render(mixer,samples,400)==1);
        for(i=0;i<800;++i)CHECK(!samples[i]);
    }
    free(mixer);
    notification_chain(rom);
}
static void netricsa_journal(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom)
{
    SsaNetricsaJournal journal;SsaNativeCarry carry;uint8_t before[38400],after[38400];unsigned i,type=12,other,serial;
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,0,NULL)==1);
    for(i=0;i<24;++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    *copy=*g;CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && !memcmp(copy,g,sizeof(*g)));
    CHECK(journal.weapons==1 && journal.read_weapons==1 && !journal.enemies && ssa_native_game_netricsa_unread(g)==0);
    CHECK(!g->hud_netricsa_ticks);
    g->damage.god_mode=1;
    /* First-level Colt acquisition, including repeat copies, is familiar.
     * Its dossier remains available; a genuinely new shotgun still alerts. */
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,2)==1);
    CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_netricsa_unread(g)==0 && !g->hud_netricsa_ticks);
    CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && journal.weapons==3 && journal.read_weapons==3);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,3)==1);
    for(i=0;i<120 && !(g->players[0].inventory.weapons&4);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_netricsa_unread(g)==1 && g->hud_netricsa_ticks==300);
    g->hud_netricsa_ticks=0; /* Isolate the steady unread counter below. */
    CHECK(ssa_native_game_classic_hud(g,before,sizeof before)==1);
    CHECK(ssa_native_game_netricsa_read(g,4,0)==1 && ssa_native_game_netricsa_unread(g)==0);
    CHECK(ssa_native_game_classic_hud(g,after,sizeof after)==1 && hash(before)!=hash(after));
    CHECK(outside_equal(before,after,215,2,24,16) && g->game.life_game.continue_flag==5);
    for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];
        if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd6 && a->activation.health>0){
            type=a->entity.type-0xcd;a->entity.body.flags&=(uint16_t)~1u;
            CHECK(ssa_combat_runtime_damage(&g->combat,&a->entity,&g->players[0].entity,NULL,10000,&g->combat_ops)==0);
            CHECK(a->activation.health==0);break;}}
    CHECK(type<12 && ssa_native_game_netricsa_unread(g)==1);
    CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && journal.enemies==(1u<<type));
    other=(type+1)%12;
    serial=g->sound_serial;
    CHECK(ssa_native_game_netricsa_read(g,0,1u<<other)==1 && ssa_native_game_netricsa_unread(g)==1);
    CHECK(g->sound_serial==serial+1 && g->last_sound==0x0b);
    CHECK(g->hud_netricsa_ticks==300); /* Previously unseen kill, not a spawn. */
    serial=g->sound_serial;
    CHECK(ssa_native_game_netricsa_read(g,0,1u<<type)==1 && ssa_native_game_netricsa_unread(g)==0);
    CHECK(g->sound_serial==serial);
    for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];
        if(a->owned && a->entity.type==0xcd+type && a->activation.health>0){
            a->entity.body.flags&=(uint16_t)~1u;
            CHECK(ssa_combat_runtime_damage(&g->combat,&a->entity,&g->players[0].entity,NULL,10000,&g->combat_ops)==0);break;}}
    CHECK(ssa_native_game_netricsa_unread(g)==0); /* Repeated kills never duplicate a dossier. */
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,4)==1);
    for(i=0;i<120 && !(g->players[0].inventory.weapons&8);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_netricsa_unread(g)==1);
    CHECK(ssa_native_game_carry_capture(&carry,g)==1 && carry.journal.enemies==(1u<<type));
    ssa_native_game_free(g);
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,1,0,&carry)==1);
    CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && journal.read_enemies==(1u<<type));
    CHECK(ssa_native_game_netricsa_unread(g)==1 && !(journal.read_weapons&8));
    CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_netricsa_unread(g)==1 && !g->hud_netricsa_ticks);
    CHECK(ssa_native_game_netricsa_read(g,8,0)==1);
    CHECK(ssa_native_game_netricsa_unread(g)==0);
    ssa_native_game_free(g);
    /* A later-level loadout is familiar; a new in-level acquisition alerts. */
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,1,0,NULL)==1);
    CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && journal.read_weapons==journal.weapons);
    CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_netricsa_unread(g)==0 && !g->hud_netricsa_ticks);
    for(i=2;i<10 && (g->players[0].inventory.weapons&(1u<<i));++i){}
    CHECK(i<10 && ssa_inventory_queue_weapon(&g->players[0].inventory,i+1)==1);
    for(other=0;other<120 && !(g->players[0].inventory.weapons&(1u<<i));++other)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_netricsa_unread(g)==1 && g->hud_netricsa_ticks==300);
    ssa_native_game_free(g);
}
static void netricsa_starters(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom)
{
    unsigned level,difficulty;SsaNetricsaJournal journal;
    for(level=0;level<12;++level)for(difficulty=0;difficulty<3;++difficulty){
        CHECK(ssa_native_game_init_campaign(g,rom,0x800000,level,difficulty,NULL)==1);
        *copy=*g;
        CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && !memcmp(copy,g,sizeof(*g)));
        CHECK(journal.weapons==g->players[0].inventory.weapons && journal.read_weapons==journal.weapons);
        CHECK(!journal.enemies && ssa_native_game_netricsa_unread(g)==0 && !g->hud_netricsa_ticks);
        if(difficulty==2){
            CHECK(ssa_native_game_alternate_weapons(g,1,level?SSA_ALTERNATE_LOADOUT:SSA_ALTERNATE_NEW_GAME)==1);
            CHECK(ssa_native_game_netricsa_entries(&journal,g)==1 && journal.read_weapons==journal.weapons);
            CHECK(journal.weapons==g->players[0].inventory.weapons && ssa_native_game_netricsa_unread(g)==0);
        }
        CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_netricsa_unread(g)==0 && !g->hud_netricsa_ticks);
        ssa_native_game_free(g);
    }
}
static void netricsa_latest_order(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom)
{
    SsaNativeCarry carry;SsaNetricsaJournal j;unsigned i,type=12;
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,0,1,NULL)==1);
    CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_netricsa_latest(g)==0);
    g->damage.god_mode=1;
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,5)==1);
    for(i=0;i<180 && !(g->players[0].inventory.weapons&16);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_netricsa_latest(g)==5);
    CHECK(ssa_inventory_queue_weapon(&g->players[0].inventory,3)==1);
    for(i=0;i<180 && !(g->players[0].inventory.weapons&4);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(ssa_native_game_netricsa_latest(g)==3 && ssa_native_game_netricsa_unread(g)==2);
    for(i=0;i<100;++i){SsaScriptActor *a=&g->spawn.actors[i];
        if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd6 && a->activation.health>0){
            type=a->entity.type-0xcd;a->entity.body.flags&=(uint16_t)~1u;
            CHECK(ssa_combat_runtime_damage(&g->combat,&a->entity,&g->players[0].entity,NULL,10000,&g->combat_ops)==0);break;}}
    CHECK(type<12 && ssa_native_game_netricsa_latest(g)==(int)(11+type));
    *copy=*g;CHECK(ssa_native_game_netricsa_entries(&j,g)==1 && ssa_native_game_netricsa_latest(g)==(int)(11+type) &&
        !memcmp(copy,g,sizeof(*g))); /* A pre-tick kill snapshot is also ordered without mutation. */
    CHECK(ssa_native_game_carry_capture(&carry,g)==1);
    ssa_native_game_free(g);
    CHECK(ssa_native_game_init_campaign(g,rom,0x800000,1,1,&carry)==1);
    CHECK(ssa_native_game_netricsa_latest(g)==(int)(11+type) && ssa_native_game_netricsa_unread(g)==3);
    CHECK(ssa_native_game_netricsa_read(g,0,1u<<type)==1 && ssa_native_game_netricsa_latest(g)==3);
    CHECK(ssa_native_game_netricsa_read(g,4,0)==1 && ssa_native_game_netricsa_latest(g)==5);
    CHECK(ssa_native_game_netricsa_read(g,16,0)==1 && ssa_native_game_netricsa_latest(g)==0);
    CHECK(ssa_native_game_netricsa_entries(&j,g)==1 && j.order_count>=4);
    g->journal.order_count=23;CHECK(ssa_native_game_netricsa_latest(g)==-1);g->journal=j;
    g->journal.order[1]=g->journal.order[0];CHECK(ssa_native_game_netricsa_latest(g)==-1);g->journal=j;
    ssa_native_game_free(g);
    carry.journal.order[0]=0;
    CHECK(ssa_native_game_init(g,rom,0x800000,1)==1 && ssa_native_game_carry_apply(g,&carry)==-1);
    ssa_native_game_free(g);
}
static unsigned read16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int read_signed16(const uint8_t *p){unsigned v=read16(p);return v<32768?(int)v:(int)v-65536;}
static void use_prompt(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom,SsaPcRenderer *pc,const char *directory)
{
    unsigned level,i,j,facing,found=0,peak=0;SsaEntity probe;SsaSwitchEntry *button=NULL;
    uint8_t canvas[38400],background[38400],sprite[128];
    for(level=0;level<12 && !found;++level){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);
        for(i=0;i<g->level_runtime.switches.count && !found;++i){
            SsaSwitchEntry *s=&g->level_runtime.switches.entries[i];
            const uint8_t *line=g->map.data+g->map.lines+s->line*40;
            const uint8_t *v=g->map.data+g->map.vertices+read16(line)*4;
            const uint8_t *v2=g->map.data+g->map.vertices+read16(line+2)*4;
            for(facing=0;facing<256 && !found;facing+=64){int32_t sine,cosine;
                ssa_sincos(&g->math,(uint16_t)(facing<<8),&sine,&cosine);
                probe=g->players[0].entity;
                probe.body.x=(read_signed16(v)+read_signed16(v2))*32768-sine*48;
                probe.body.y=(read_signed16(v+2)+read_signed16(v2+2))*32768-cosine*48;
                if(ssa_player_interaction_query(&g->use_query,&probe,(uint8_t)facing,&g->math)<0)continue;
                for(j=0;j<g->use_query.special_count;++j)if(g->use_query.special_lines[j]==s->line){
                    CHECK(ssa_world_remove(&g->world,&g->players[0].entity)==1);
                    g->players[0].entity.body.x=probe.body.x;g->players[0].entity.body.y=probe.body.y;
                    CHECK(ssa_world_insert(&g->world,&g->players[0].entity)==1);
                    g->players[0].motion.movement.heading=(uint16_t)(facing<<8);
                    g->players[0].motion.facing=(uint8_t)facing;button=s;found=1;break;
                }
            }
        }
        if(!found)ssa_native_game_free(g);
    }
    CHECK(found && button && !(button->flags&1));
    g->frame_game.flags|=1;CHECK(ssa_native_game_tick(g)==1 && g->hud_use_available);
    CHECK(g->use_query.visited!=g->gameplay.interaction.visited && g->use_query.visited!=g->world.collision.visited);
    CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
    *copy=*g;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
    CHECK(ssa_sprite_image_indices(&g->sprite_assets.images[77],sprite,16,sizeof sprite)==1);
    for(i=0;i<8;++i)for(j=0;j<16;++j){unsigned at=(92+i)*240+112+j,source=sprite[i*16+j];
        unsigned color=g->sprite_assets.palette[canvas[at]]&0x7fff,light=(color>>5)&31;
        CHECK(!background[at] && (source?canvas[at]!=0:canvas[at]==0) && !(color&0x7c1f));
        if(light>peak)peak=light;}
    CHECK(peak==29); /* Retained HUD green is RGB555(0,29,0). */
    CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
    if(directory){screenshot(directory,"use-gba",level-1,g->rgb888,240,160);
        screenshot(directory,"use-pc",level-1,pc->pixels,pc->width,pc->height);}
    CHECK(ssa_level_runtime_line(&g->level_runtime,2,button->line,&g->mover_ops)==0 && (button->flags&1));
    CHECK(ssa_native_game_tick(g)==1 && !g->hud_use_available);
    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
    for(i=92;i<100;++i)for(j=112;j<128;++j)CHECK(!canvas[i*240+j]);
    ssa_native_game_free(g);
}
/* Verify the displayed glyphs/meter, including their shaded pixels, rather
 * than duplicating a private color-selection helper in a unit test. */
enum {EXPECT_GREEN,EXPECT_RED,EXPECT_GRAY,EXPECT_WHITE};
static void check_resource_color(const SsaNativeGame *g,const uint8_t *canvas,
    unsigned left,unsigned top,unsigned width,unsigned height,unsigned expected)
{
    unsigned x,y,lit=0,peak=0;
    for(y=top;y<top+height;++y)for(x=left;x<left+width;++x){unsigned c=g->sprite_assets.palette[canvas[y*240+x]]&0x7fff;
        unsigned r=c&31,gg=(c>>5)&31,b=(c>>10)&31,v=r>gg?r:gg;
        if(!c)continue;
        ++lit;if(v>peak)peak=v;
        if(expected==EXPECT_GREEN)CHECK(!r && !b && gg);
        else if(expected==EXPECT_RED)CHECK(r>gg*2 && r>b*2);
        else CHECK(r==gg && gg==b);
    }
    CHECK(lit);
    if(expected==EXPECT_GRAY)CHECK(peak<=24);
    if(expected==EXPECT_WHITE)CHECK(peak>=28);
}
static void resource_alerts(SsaNativeGame *g,SsaNativeGame *copy,const uint8_t *rom,
    SsaPcRenderer *pc,const char *directory)
{
    static const unsigned values[]={0,1,24,25,50,100,200};
    uint8_t canvas[38400],normal[38400],background[38400];unsigned level,i,j,weapon;
    for(level=0;level<=6;level+=6){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);
        for(i=0;i<24;++i)CHECK(ssa_native_game_tick(g)==1);
        CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
        g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=0;
        g->hud_health_flash=0;g->hud_weapon_ticks=0;g->hud_netricsa_ticks=0;
        g->players[0].inventory.weapons=0x3ff;
        memset(g->players[0].inventory.ammo,0,sizeof g->players[0].inventory.ammo);
        for(i=0;i<sizeof values/sizeof values[0];++i){
            g->players[0].inventory.health=(int16_t)values[i];g->players[0].inventory.stat_c4=(int16_t)values[i];
            *copy=*g;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
            check_resource_color(g,canvas,21,143,24,11,values[i]<25?EXPECT_RED:EXPECT_GREEN);
            if(values[i])check_resource_color(g,canvas,21,123,24,11,values[i]<25?EXPECT_GRAY:EXPECT_GREEN);
            else for(j=121;j<138;++j)CHECK(!background[j*240+21]);
        }
        g->players[0].inventory.health=24;g->hud_health_flash=40;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
        check_resource_color(g,canvas,21,143,24,11,EXPECT_WHITE);g->hud_health_flash=0;
        for(weapon=3;weapon<=10;++weapon){unsigned limit=g->inventory_rules.ammo_capacity[weapon-1];
            unsigned amounts[7]={0,(limit*15+99)/100-1,(limit*15+99)/100,
                (limit+3)/4-1,(limit+3)/4,limit,limit+1};
            g->gameplay.weapon.requested=(uint16_t)weapon;
            for(i=0;i<7;++i){unsigned amount=amounts[i];
                g->players[0].inventory.ammo[weapon-1]=(int16_t)amount;
                CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
                check_resource_color(g,canvas,87,143,24,11,amount*100<limit*15?EXPECT_RED:EXPECT_GREEN);
                if(amount){unsigned meter_height=(((amount>limit?limit:amount)*11)+limit-1)/limit;
                    check_resource_color(g,canvas,233,155-meter_height,3,meter_height,amount*4<limit?EXPECT_RED:EXPECT_GREEN);
                    memcpy(normal,canvas,sizeof normal);g->hud_ammo_flash[weapon-1]=40;
                    CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
                    check_resource_color(g,canvas,87,143,24,11,EXPECT_WHITE);
                    check_resource_color(g,canvas,233,155-meter_height,3,meter_height,EXPECT_WHITE);
                    CHECK(outside_equal(normal,canvas,87,141,24,17)==0); /* The separate meter flashes too. */
                    g->hud_ammo_flash[weapon-1]=0;
                }else for(j=142;j<158;++j)CHECK(!canvas[j*240+223]); /* Empty pools remain omitted. */
            }
            g->players[0].inventory.ammo[weapon-1]=0;
        }
        /* Both renderers display the same alert palette. Some ammo is between
         *15% and25%: its counter is green while its small meter is already red. */
        g->players[0].inventory.health=24;g->players[0].inventory.stat_c4=24;
        for(i=2;i<10;++i)g->players[0].inventory.ammo[i]=(int16_t)((g->inventory_rules.ammo_capacity[i]+9)/10);
        g->gameplay.weapon.requested=4;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
        CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        for(i=0;i<38400;++i)if(canvas[i]){
            CHECK(g->rgb555[i]==(g->sprite_assets.palette[canvas[i]]&0x7fff));
            CHECK(pc->pixels[(i/240*2)*pc->width+i%240*2]==hud_color(g,canvas[i]));}
        if(directory){screenshot(directory,"low-resources-gba",level,g->rgb888,240,160);
            screenshot(directory,"low-resources-pc",level,pc->pixels,pc->width,pc->height);}
        g->players[0].inventory.ammo[3]=(int16_t)((g->inventory_rules.ammo_capacity[3]+4)/5);
        CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(pc,g)==1);draws+=2;
        if(directory){screenshot(directory,"low-bars-gba",level,g->rgb888,240,160);
            screenshot(directory,"low-bars-pc",level,pc->pixels,pc->width,pc->height);}
        ssa_native_game_free(g);
    }
}
static void original_equipped_layout(SsaNativeGame *g,const uint8_t *rom,SsaPcRenderer *pc,const char *directory)
{
    unsigned level,i;
    for(level=0;level<=6;level+=6){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);
        g->damage.god_mode=1;g->players[0].inventory.health=150;g->players[0].inventory.stat_c4=75;
        g->score=12345678;g->players[0].inventory.weapons=0x3ff;
        for(i=0;i<10;++i)g->players[0].inventory.ammo[i]=(int16_t)g->inventory_rules.ammo_capacity[i];
        CHECK(ssa_native_game_select_weapon(g,4)==1);
        for(i=0;i<120;++i)CHECK(ssa_native_game_step(g)==1);
        CHECK(g->gameplay.weapon.current==4 && g->hud.current_weapon==4 && g->players[0].inventory.stat_c4==75);
        original_reference(g);
        CHECK(ssa_pc_renderer_resize(pc,854,480)==1 && ssa_pc_renderer_draw(pc,g)==1);++draws;
        original_anchors(g,pc);
        if(directory){screenshot(directory,"original-equipped-gba",level,g->rgb888,240,160);
            screenshot(directory,"original-equipped-wide",level,pc->pixels,pc->width,pc->height);}
        CHECK(ssa_pc_renderer_resize(pc,480,320)==1);
        ssa_native_game_free(g);
    }
}
int main(int argc,char **argv)
{
    SsaNativeGame *g=calloc(1,sizeof(*g)),*copy=malloc(sizeof(*copy));SsaPcRenderer pc={0};
    uint8_t *rom=malloc(0x800000),canvas[38400],old[38400],background[38400];uint16_t reference[38400];
    SsaSettings settings=SSA_SETTINGS_DEFAULT,loaded;uint8_t encoded[16];FILE *f;unsigned level,i,k,visible;
    CHECK(argc>=2 && g && copy && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000 && !fclose(f));
    CHECK(settings.hud_style==SSA_HUD_ORIGINAL && !settings.automap_enabled);
    CHECK(ssa_classic_hud_darken555(0x7fff)==0x39ce);
    CHECK(ssa_classic_hud_darken888(0xffffff)==0x737373);
    CHECK(ssa_classic_hud_alpha(110,30,300,0)==255);
    CHECK(ssa_classic_hud_alpha(110,30,286,0)==255 && ssa_classic_hud_alpha(110,30,285,0)==0);
    CHECK(ssa_classic_hud_alpha(110,30,271,0)==0 && ssa_classic_hud_alpha(110,30,270,0)==255);
    for(i=181;i<=300;++i){unsigned expected=((300-i)/15)%2?0:255;
        CHECK(ssa_classic_hud_alpha(110,30,i,0)==expected);
        CHECK(ssa_classic_hud_alpha(110,42,i,1)==expected);}
    CHECK(ssa_classic_hud_alpha(110,30,180,0)==255 && ssa_classic_hud_alpha(110,30,291,1)==255);
    CHECK(ssa_classic_hud_blend555(0x7fff,0,0)==0 && ssa_classic_hud_blend555(0x7fff,0,255)==0x7fff);
    CHECK(ssa_classic_hud_blend888(0xffffff,0,128)==0x808080);
    for(i=0;i<2;++i)for(k=0;k<=100;++k){settings.hud_style=i;settings.sound_volume=k;
        CHECK(ssa_settings_encode(&settings,encoded,16)==1 && encoded[3]==19);
        CHECK(ssa_settings_decode(&loaded,encoded,16)==1 && !memcmp(&settings,&loaded,sizeof settings));}
    settings.hud_style=2;CHECK(ssa_settings_encode(&settings,encoded,16)==-1);
    CHECK(ssa_pc_renderer_resize(&pc,480,320)==1);
    /* The alternate starting Colt has no resolved sprite before the first
     * simulation frame. Menu style changes must not render or advance it. */
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(ssa_native_game_alternate_weapons(g,1,SSA_ALTERNATE_NEW_GAME)==1);
    for(i=0;i<4;++i){SsaSpriteOam saved=g->oam;SsaInventory inventory=g->players[0].inventory;
        CHECK(ssa_native_game_hud_style(g,i&1)==1 && g->hud_style==(i&1));
        CHECK(!g->frame_game.counter && !memcmp(&saved,&g->oam,sizeof saved));
        CHECK(!memcmp(&inventory,&g->players[0].inventory,sizeof inventory));}
    CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(&pc,g)==1);
    CHECK(g->hud_style==SSA_HUD_CLASSIC && g->gameplay.weapon.current==2);
    CHECK(ssa_native_game_netricsa_unread(g)==0 && !g->hud_netricsa_ticks); /* Alternate starting Colt. */
    weapon_wheel(g,copy,&pc,argc>2?argv[2]:NULL);
    damage_and_text(g,copy,&pc,argc>2?argv[2]:NULL);
    ssa_native_game_free(g);
    netricsa_notification(g,copy,rom,&pc,argc>2?argv[2]:NULL);
    netricsa_journal(g,copy,rom);
    netricsa_starters(g,copy,rom);
    netricsa_latest_order(g,copy,rom);
    use_prompt(g,copy,rom,&pc,argc>2?argv[2]:NULL);
    resource_alerts(g,copy,rom,&pc,argc>2?argv[2]:NULL);
    original_equipped_layout(g,rom,&pc,argc>2?argv[2]:NULL);
    for(level=0;level<=6;level+=6){SsaInventory inventory;uint32_t original_hash,changed,pc_behind;uint16_t gba_behind;
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1);
        for(i=0;i<24;++i)CHECK(ssa_native_game_step(g)==1);
        CHECK(g->hud_style==SSA_HUD_ORIGINAL);
        CHECK(g->hud.game_value==g->game.life_game.continue_flag && g->hud.game_value==3);
        original_reference(g);
        CHECK(ssa_pc_renderer_draw(&pc,g)==1);++draws;
        CHECK(ssa_pc_renderer_resize(&pc,854,480)==1 && ssa_pc_renderer_draw(&pc,g)==1);++draws;
        original_anchors(g,&pc);
        if(argc>2)screenshot(argv[2],"original-wide",level,pc.pixels,pc.width,pc.height);
        CHECK(ssa_pc_renderer_resize(&pc,640,480)==1 && ssa_pc_renderer_draw(&pc,g)==1);++draws;
        original_anchors(g,&pc);
        if(argc>2)screenshot(argv[2],"original-4by3",level,pc.pixels,pc.width,pc.height);
        CHECK(ssa_pc_renderer_resize(&pc,480,320)==1 && ssa_pc_renderer_draw(&pc,g)==1);++draws;
        if(argc>2){screenshot(argv[2],"original-lives-gba",level,g->rgb888,240,160);
            screenshot(argv[2],"original-lives-pc",level,pc.pixels,pc.width,pc.height);}
        memcpy(reference,g->rgb555,sizeof reference);inventory=g->players[0].inventory;
        CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
        CHECK(g->sprites.active_count==75 && !memcmp(&inventory,&g->players[0].inventory,sizeof inventory));
        *copy=*g;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1 && !memcmp(copy,g,sizeof(*g)));
        visible=0;for(i=0;i<38400;++i)visible+=canvas[i]!=0;CHECK(visible>300);
        CHECK(!g->players[0].inventory.stat_c4);
        for(i=120;i<140;++i)for(k=0;k<48;++k)CHECK(!canvas[i*240+k] && !background[i*240+k]);
        CHECK(background[6*240+22] && !canvas[6*240+22] && !background[80*240+120]);
        g->hud_suppressed=1;
        CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(&pc,g)==1);draws+=2;
        gba_behind=g->rgb555[6*240+22];pc_behind=pc.pixels[12*480+44];
        g->hud_suppressed=0;
        CHECK(ssa_native_game_draw(g)==1);++draws;
        CHECK(g->rgb555[6*240+22]==ssa_classic_hud_darken555(gba_behind));
        for(i=0;i<38400;++i)if(canvas[i])CHECK(g->rgb555[i]==(g->sprite_assets.palette[canvas[i]]&0x7fff));
        *copy=*g;CHECK(ssa_pc_renderer_draw(&pc,g)==1 && !memcmp(copy,g,sizeof(*g)));++draws;
        CHECK(pc.pixels[12*480+44]==ssa_classic_hud_darken888(pc_behind));
        for(i=0;i<38400;++i)if(canvas[i]){unsigned c=g->sprite_assets.palette[canvas[i]],r=c&31,gg=(c>>5)&31,b=(c>>10)&31;
            CHECK(pc.pixels[(i/240*2)*480+(i%240*2)]==(((r*8+r/4)<<16)|((gg*8+gg/4)<<8)|(b*8+b/4)));}
        if(argc>2){screenshot(argv[2],"gba",level,g->rgb888,240,160);screenshot(argv[2],"pc",level,pc.pixels,pc.width,pc.height);}
        CHECK(ssa_native_game_hud_style(g,SSA_HUD_ORIGINAL)==1 && ssa_native_game_draw(g)==1);
        CHECK(!memcmp(reference,g->rgb555,sizeof reference)); /* Same-frame retail restoration. */
        CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);
        g->high_score=90000000;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);original_hash=hash(canvas);
        memcpy(old,canvas,sizeof old);g->score=12345678;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(canvas)!=original_hash);
        CHECK(outside_equal(old,canvas,0,0,64,16)); /* Score lives only inside the top-left frame. */
        memcpy(old,canvas,sizeof old);g->high_score=80000000;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(old)!=hash(canvas));
        CHECK(outside_equal(old,canvas,88,2,59,10));
        g->high_score=1;CHECK(ssa_native_game_classic_hud(g,old,sizeof old)==1);
        g->high_score=g->score;CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && !memcmp(old,canvas,sizeof old));
        memcpy(old,canvas,sizeof old);g->frame_game.elapsed+=60;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(old)!=hash(canvas));
        CHECK(outside_equal(old,canvas,103,12,29,10));
        memcpy(old,canvas,sizeof old);g->players[0].inventory.health=25;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
        CHECK(!memcmp(old,canvas,16*240)); /* Header no longer displays a health bar. */
        g->players[0].inventory.health=inventory.health;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);original_hash=hash(canvas);
        g->players[0].inventory.health=25;CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(canvas)!=original_hash);
        original_hash=hash(canvas);g->players[0].inventory.stat_c4=75;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(canvas)!=original_hash);
        original_hash=hash(canvas);g->score=12345678;g->frame_game.elapsed=12345;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(canvas)!=original_hash);
        for(i=1;i<=10;++i){static const unsigned ids[10]={68,67,69,70,71,72,74,73,0,75};unsigned count=0,wx,j;
            g->gameplay.weapon.requested=(uint16_t)i;g->players[0].inventory.ammo[i-1]=42;
            for(j=2;j<10;++j)if(g->players[0].inventory.ammo[j]>0)++count;
            wx=i<=2?77:count==8?58:66;
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);++draws;
            if(ids[i-1]){uint8_t icon[2048];const SsaSpriteImage *im=&g->sprite_assets.images[ids[i-1]];unsigned x,y;
                CHECK(ssa_sprite_image_indices(im,icon,im->width,sizeof icon)==1);
                for(y=0;y<17 && y<im->height;++y)for(x=0;x<16;++x)if(icon[y*im->width+x])
                    CHECK(canvas[(141+y)*240+wx+x]==icon[y*im->width+x]);}
            else{unsigned x,y,distinct=0,seen[256]={0};
                /* The supplied PNG has a16x16 frame and a black shadow row,
                 * unlike the old15px frame and reduced world-sprite silhouette. */
                for(x=0;x<16;++x){
                    CHECK((g->sprite_assets.palette[canvas[141*240+wx+x]]&0x7fff)==0x3a0);
                    CHECK((g->sprite_assets.palette[canvas[156*240+wx+x]]&0x7fff)==0x3a0);
                    CHECK(canvas[157*240+wx+x] && !(g->sprite_assets.palette[canvas[157*240+wx+x]]&0x7fff));}
                CHECK(!canvas[143*240+wx+1]); /* Original transparent hole. */
                CHECK(canvas[145*240+wx+10]); /* Original larger device detail. */
                for(y=141;y<158;++y)for(x=wx;x<wx+16;++x){unsigned index=canvas[y*240+x],c=g->sprite_assets.palette[index]&0x7fff;
                    if(index){CHECK(!(c&0x7c1f));if(c && !seen[index]){seen[index]=1;++distinct;}}}
                CHECK(distinct>=3);
            }
        }
        g->gameplay.weapon.requested=1;memset(g->players[0].inventory.ammo,0,sizeof g->players[0].inventory.ammo);
        for(i=0;i<8;++i){unsigned x=223,limit=g->inventory_rules.ammo_capacity[i+2],meter;
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);memcpy(old,canvas,sizeof old);
            g->players[0].inventory.ammo[i+2]=(int16_t)limit;
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && hash(canvas)!=hash(old));
            CHECK(outside_equal(old,canvas,x,142,16,16)); /* Icons/meters stay inside their frames. */
            CHECK(canvas[144*240+x+10]==canvas[154*240+x+12] && canvas[144*240+x+10]!=old[144*240+x+10]);
            meter=canvas[154*240+x+10];g->gameplay.weapon.requested=(uint16_t)(i+3);
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && canvas[154*240+x+10]==meter);
            g->gameplay.weapon.requested=1;g->players[0].inventory.ammo[i+2]=1;
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
            {unsigned c=g->sprite_assets.palette[canvas[154*240+x+10]]&0x7fff;
                CHECK(limit>4?((c&31)>2*((c>>5)&31) && (c&31)>2*((c>>10)&31)):!(c&0x7c1f));}
            /* Empty meter space reveals the same pixels as the bare icon. */
            g->inventory_rules.ammo_capacity[i+2]=0;
            CHECK(ssa_native_game_classic_hud(g,old,sizeof old)==1);
            g->inventory_rules.ammo_capacity[i+2]=(uint16_t)limit;
            CHECK(canvas[144*240+x+10]==old[144*240+x+10]);
            g->players[0].inventory.ammo[i+2]=0;
        }
        if(argc>2){unsigned x;for(i=2;i<10;++i)g->players[0].inventory.ammo[i]=(int16_t)g->inventory_rules.ammo_capacity[i];
            g->gameplay.weapon.requested=9;CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
            for(x=0;x<38400;++x){unsigned c=g->sprite_assets.palette[canvas[x]],r=c&31,gg=(c>>5)&31,b=(c>>10)&31;
                g->rgb888[x]=canvas[x]?(((r*8+r/4)<<16)|((gg*8+gg/4)<<8)|(b*8+b/4)):0x202020;}
            screenshot(argv[2],"ammo-panel",level,g->rgb888,240,160);}
        memcpy(old,canvas,sizeof old);g->hud.boss_present=1;g->hud.boss_health=1500;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && memcmp(canvas,old,sizeof old));
        CHECK(canvas[3*240+91]); /* Tall middle step of the high-score podium. */
        g->hud.boss_health=3000;
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
        CHECK(canvas[29*240+92] && !canvas[29*240+93]); /* Skull eye stays open at full health. */
        CHECK(canvas[29*240+99] && !canvas[29*240+100] && !canvas[29*240+148]);
        for(i=101;i<148;++i)CHECK(canvas[29*240+i]==canvas[29*240+101]);
        g->hud.boss_health=1500;
        /* Whole-layout fixture: original-size badges have2px edge insets;
         * armor has a3px gap above health, all eight ammo slots stay clear of
         * the selected weapon/counter, with high score above the timer. */
        CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
        CHECK(background[2*240+2] && !background[2*240+1]);
        CHECK((background[215+2*240]!=0)==(ssa_native_game_netricsa_unread(g)>0) && !background[239+2*240]);
        CHECK(background[2*240+88] && !background[88] && background[26*240+91] && !background[26*240+90]);
        CHECK(background[14*240+105] && !background[14*240+88]);
        CHECK(background[121*240+2] && background[141*240+2]);
        for(i=138;i<140;++i)for(k=2;k<47;++k)CHECK(!canvas[i*240+k] && !background[i*240+k]);
        for(i=158;i<160;++i)for(k=0;k<240;++k)CHECK(!canvas[i*240+k] && !background[i*240+k]);
        for(i=0;i<8;++i){unsigned left=111+i*16;
            CHECK(background[142*240+left] && background[157*240+left+14]);
            CHECK(!background[142*240+left+15]);}
        for(i=141;i<158;++i)for(k=105;k<111;++k)CHECK(!canvas[i*240+k] && !background[i*240+k]);
        if(argc>2){
            g->players[0].inventory.weapons=0x3ff;g->hud_weapon_ticks=180;
            g->players[0].inventory.ammo[0]=g->players[0].inventory.ammo[1]=999;
            CHECK(ssa_native_game_netricsa_read(g,0,0)==1 && g->hud_netricsa_ticks==300);
            CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
            CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(&pc,g)==1);draws+=2;
            screenshot(argv[2],"reference-gba",level,g->rgb888,240,160);
            screenshot(argv[2],"reference-pc",level,pc.pixels,pc.width,pc.height);
            CHECK(ssa_pc_renderer_resize(&pc,854,480)==1 && ssa_pc_renderer_draw(&pc,g)==1);++draws;
            screenshot(argv[2],"reference-wide",level,pc.pixels,pc.width,pc.height);
            CHECK(canvas[2*240+2] && pc.pixels[6*854+6]==hud_color(g,canvas[2*240+2]));
            CHECK(canvas[2*240+88] && pc.pixels[6*854+331]==hud_color(g,canvas[2*240+88]));
            CHECK(canvas[2*240+215] && pc.pixels[6*854+779]==hud_color(g,canvas[2*240+215]));
            CHECK(canvas[142*240+223] && pc.pixels[426*854+803]==hud_color(g,canvas[142*240+223]));
            CHECK(canvas[141*240+58] && pc.pixels[423*854+235]==hud_color(g,canvas[141*240+58]));
            CHECK(ssa_pc_renderer_resize(&pc,640,480)==1 && ssa_pc_renderer_draw(&pc,g)==1);++draws;
            CHECK(pc.pixels[6*640+6]==hud_color(g,canvas[2*240+2]) && pc.pixels[6*640+235]==hud_color(g,canvas[2*240+88]));
            CHECK(pc.pixels[432*640+595]==hud_color(g,canvas[142*240+223]));
            CHECK(canvas[26*240+91] && pc.pixels[70*640+243]==hud_color(g,canvas[26*240+91]));
            for(i=100;i<140 && !canvas[40*240+i];++i){}
            CHECK(i<140); /* On/off blink is checked against the scene above. */
            screenshot(argv[2],"reference-4by3",level,pc.pixels,pc.width,pc.height);
            CHECK(ssa_pc_renderer_resize(&pc,480,320)==1);
            g->hud_weapon_ticks=0;
        }
        g->hud_suppressed=1;CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);
        changed=0;for(i=0;i<38400;++i)changed+=canvas[i]!=0 || background[i]!=0;CHECK(!changed);g->hud_suppressed=0;
        g->players[0].motion.movement.flags|=8;
        CHECK(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)==1);changed=0;
        for(i=0;i<38400;++i)changed+=canvas[i]!=0 || background[i]!=0;
        CHECK(!changed);g->players[0].motion.movement.flags&=(uint16_t)~8u;
        g->players[0].inventory=inventory;g->gameplay.weapon.requested=g->gameplay.weapon.current;
        CHECK(ssa_native_game_alternate_weapons(g,1,0)==1);
        g->players[0].inventory.weapons|=2;g->players[0].inventory.ammo[1]=999;g->alternates.dual_colts=1;
        CHECK(ssa_native_game_select_weapon(g,2)==1);
        for(i=0;i<80;++i){CHECK(ssa_native_game_tick(g)==1 && ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(&pc,g)==1);draws+=2;}
        CHECK(g->gameplay.weapon.current==2 && g->alternates.variant==1);
        CHECK(ssa_native_game_hud_style(g,2)==-1 && g->hud_style==SSA_HUD_CLASSIC);
        /* Actual consumption starts a40-tick palette cue. Pickups/loadouts do
         * not, redraws do not age it, and pause freezes it. */
        g->players[0].inventory.health=32767;
        g->players[0].inventory.weapons|=8;g->players[0].inventory.ammo[3]=20;
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        CHECK(ssa_native_game_select_weapon(g,4)==1);
        for(i=0;i<120;++i)CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->gameplay.weapon.current==4 && !g->hud_ammo_flash[3]);
        CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,1)==1);
        for(i=0;i<90 && g->players[0].inventory.ammo[3]==20;++i)CHECK(ssa_native_game_tick(g)==1);
        CHECK(ssa_pc_input_button(&g->input,SSA_PC_MOUSE_LEFT,0)==1);
        CHECK(g->players[0].inventory.ammo[3]<20 && g->hud_ammo_flash[3]==40);
        CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1 && g->hud_ammo_flash[3]==40);
        for(k=0;k<2;++k){unsigned x,y,lit=0,c,distinct=0,seen[256]={0},count=0,rank=0,meter_x,wx;
            for(i=2;i<10;++i)if(g->players[0].inventory.ammo[i]>0){++count;if(i<3)++rank;}
            meter_x=239-16*count+16*rank+10;wx=count==8?58:66;
            CHECK(ssa_native_game_classic_hud(g,canvas,sizeof canvas)==1);
            for(x=meter_x;x<meter_x+3;++x){c=g->sprite_assets.palette[canvas[154*240+x]]&0x7fff;
                CHECK(c && (k?((c&31)>2*((c>>5)&31) && (c&31)>2*((c>>10)&31)):
                    ((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31))));}
            for(y=143;y<154;++y)for(x=wx+21;x<wx+45;++x){c=g->sprite_assets.palette[canvas[y*240+x]]&0x7fff;
                if(c){++lit;CHECK(k?((c&31)>2*((c>>5)&31) && (c&31)>2*((c>>10)&31)):
                    ((c&31)==((c>>5)&31) && (c&31)==((c>>10)&31)));
                    if(!seen[canvas[y*240+x]]){seen[canvas[y*240+x]]=1;++distinct;}}}
            CHECK(lit>8 && distinct>=2); /* Shading survives the palette swap. */
            CHECK(ssa_native_game_draw(g)==1 && ssa_pc_renderer_draw(&pc,g)==1);draws+=2;
            if(argc>2){screenshot(argv[2],k?"settled-gba":"flash-gba",level,g->rgb888,240,160);
                screenshot(argv[2],k?"settled-pc":"flash-pc",level,pc.pixels,pc.width,pc.height);}
            if(!k){g->frame_game.flags|=1;
                for(i=0;i<3;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_ammo_flash[3]==40);
                g->frame_game.flags&=(uint16_t)~1u;
                for(i=0;i<39;++i)CHECK(ssa_native_game_tick(g)==1 && g->hud_ammo_flash[3]);
                CHECK(ssa_native_game_tick(g)==1);
                CHECK(!g->hud_ammo_flash[3]);}
        }
        ssa_native_game_free(g);++paths;
    }
    ssa_pc_renderer_free(&pc);free(copy);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"draws\":%u,\"paths\":%u}\n",checks,draws,paths);return 0;
}
