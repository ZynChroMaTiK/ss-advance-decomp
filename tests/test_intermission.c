#include "ssa/intermission.h"
#include "ssa/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,starts;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"intermission line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void word(FILE *f,uint32_t n,unsigned bytes){while(bytes--){fputc(n&255,f);n>>=8;}}
static void bitmap(const char *path,const uint32_t *pixels){
    FILE *f=fopen(path,"wb");unsigned i;CHECK(f);
    fputs("BM",f);word(f,54+38400*4,4);word(f,0,4);word(f,54,4);word(f,40,4);word(f,240,4);word(f,(uint32_t)-160,4);
    word(f,1,2);word(f,32,2);word(f,0,4);word(f,38400*4,4);for(i=0;i<4;++i)word(f,0,4);
    for(i=0;i<38400;++i)word(f,pixels[i],4);
    CHECK(fclose(f)==0);
}
static void menu_colors(const uint32_t *pixels,unsigned top,unsigned bottom,unsigned want_green,unsigned want_white){
    unsigned i,green=0,white=0,bad=0;
    for(i=top*240;i<bottom*240;++i){unsigned c=pixels[i],r=(c>>16)&255,g=(c>>8)&255,b=c&255;
        if(!r && !b && g)++green;
        else if(r==g && g==b){if(r)++white;}
        else ++bad;
    }
    CHECK(!bad && (!want_green || green>10) && (!want_white || white>10));
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned lang,map,i,t;char path[512];
    SsaNativeGame *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*g));SsaIntermission *s=calloc(1,sizeof(*s));
    uint32_t *back=malloc(38400*4),*out=malloc(38400*4),*repeat=malloc(38400*4);
    SsaAudio *audio=calloc(1,sizeof(*audio));int16_t pcm[1600];
    CHECK(argc==3 && rom && g && before && s && back && out && repeat && audio);
    f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_audio_init(audio,rom,0x800000,48000)==1 && ssa_audio_level_music(audio,0)==1);
    CHECK(ssa_audio_play(audio,0x19)==1 && ssa_audio_render(audio,pcm,800)==1);
    CHECK(ssa_audio_music(audio,4,0)==1);
    {SsaAudioPlayer music=audio->players[0];uint64_t notes=audio->notes;
        CHECK(ssa_audio_render(audio,pcm,800)==1 && !memcmp(&music,&audio->players[0],sizeof music) && notes==audio->notes);
        for(i=0;i<1600;++i)CHECK(!pcm[i]);
        CHECK(ssa_audio_play(audio,2)==1 && ssa_audio_render(audio,pcm,800)==1);
        CHECK(!memcmp(&music,&audio->players[0],sizeof music));
        CHECK(ssa_audio_music(audio,5,0)==1 && ssa_audio_render(audio,pcm,800)==1 && audio->players[0].clock!=music.clock);
    }
    for(i=0;i<2;++i){unsigned j,cue_ticks=0;
        ssa_audio_reset(audio);
        CHECK(ssa_audio_play(audio,i?0x7f:0x78)==1 && ssa_audio_play(audio,7)==1);
        for(j=SSA_AUDIO_MUSIC_VOICES;j<SSA_AUDIO_VOICES;++j)
            if(audio->voices[j].active)CHECK(audio->voices[j].type!=0); /* Cue, no speech sample. */
        for(t=0;t<1200 && audio->players[0].active;++t){unsigned busy=0;
            for(j=1;j<SSA_AUDIO_PLAYERS;++j)if(audio->players[j].active && audio->players[j].song==7)busy=1;
            cue_ticks+=busy;CHECK(ssa_audio_render(audio,pcm,800)==1);
            for(j=SSA_AUDIO_MUSIC_VOICES;j<SSA_AUDIO_VOICES;++j)
                if(audio->voices[j].active)CHECK(audio->voices[j].type!=0);
        }
        CHECK(t>60 && t<1200 && cue_ticks<60 && !audio->errors);
    }
    for(lang=0;lang<3;++lang){
        CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);CHECK(ssa_native_game_tick(g)==1);
        CHECK(ssa_native_game_draw(g)==1);memcpy(back,g->rgb888,38400*4);memcpy(before,g,sizeof(*g));
        CHECK(ssa_intermission_init(s,g,lang,0)==1);
        for(t=0;t<20;++t)CHECK(ssa_intermission_step(s,0,0,1)==1);
        CHECK(ssa_intermission_overlay(s,back,240,160,out)==1 && s->notice.progress==s->notice.count && s->oam.count>0);
        snprintf(path,sizeof path,"%s.pause%u.bmp",argv[2],lang);bitmap(path,out);
        CHECK(ssa_intermission_step(s,0,0x800000,1)==1 && s->pause.selection==1);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==1 && s->pause.selection==3 && s->notice.id==0xb2);
        for(t=0;t<20;++t)CHECK(ssa_intermission_step(s,0,0,1)==1);
        CHECK(ssa_intermission_overlay(s,back,240,160,out)==1);
        snprintf(path,sizeof path,"%s.quit%u.bmp",argv[2],lang);bitmap(path,out);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==1 && !s->pause.selection && s->notice.id==0xb1);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==0 && s->pause.input_state==0);
        CHECK(!memcmp(before,g,sizeof(*g)));
        CHECK(ssa_intermission_init(s,g,lang,0)==1);
        CHECK(ssa_intermission_pointer(s,120,72,1)==0x10000 && s->pause.selection==1);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==1 && s->pause.selection==3);
        CHECK(ssa_intermission_pointer(s,120,56,1)==0x10000 && s->pause.selection==2);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==0 && s->pause.input_state==5);
        CHECK(!memcmp(before,g,sizeof(*g)));
        CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);memcpy(before,g,sizeof(*g));
        CHECK(ssa_intermission_init(s,g,lang,0)==1 && s->classic);
        for(t=0;t<20;++t)CHECK(ssa_intermission_step(s,0,0,1)==1);
        memset(repeat,0,38400*4);CHECK(ssa_intermission_overlay(s,repeat,240,160,out)==1);
        menu_colors(out,40,104,1,1);menu_colors(out,72,88,0,1);menu_colors(out,88,104,1,0);
        CHECK(ssa_intermission_overlay(s,back,240,160,out)==1);
        snprintf(path,sizeof path,"%s.classic-pause%u.bmp",argv[2],lang);bitmap(path,out);
        CHECK(ssa_intermission_pointer(s,120,88,0)==0 && s->pause.selection==1);
        CHECK(ssa_intermission_overlay(s,repeat,240,160,out)==1);
        menu_colors(out,72,88,1,0);menu_colors(out,88,104,0,1);
        CHECK(ssa_intermission_step(s,0,0x10000,1)==1 && s->notice.id==0xb2);
        for(t=0;t<20;++t)CHECK(ssa_intermission_step(s,0,0,1)==1);
        CHECK(ssa_intermission_overlay(s,repeat,240,160,out)==1);menu_colors(out,40,104,1,1);
        CHECK(!memcmp(before,g,sizeof(*g)));ssa_native_game_free(g);
        for(map=0;map<12;++map){SsaCompletionRecord record;SsaStatisticsTotals totals={0};uint32_t expected;
            CHECK(ssa_native_game_init(g,rom,0x800000,map)==1);CHECK(ssa_native_game_tick(g)==1);
            CHECK(ssa_native_game_draw(g)==1);memcpy(back,g->rgb888,38400*4);
            g->finished=1;g->gameplay.life_game.input_state=3;g->score=1234;g->damage.difficulty=(uint8_t)(map%3);
            g->frame_game.elapsed=60*(map%2?1000:30);
            for(i=2;i<10;++i)g->players[0].inventory.ammo[i]=2;
            memcpy(before,g,sizeof(*g));CHECK(ssa_intermission_init(s,g,lang,1)==1);++starts;
            expected=1234+1600*(map%3+1)*(map%3+1);
            if(!(map%2))expected+=(s->tally.par_seconds-30)*1000*(map%3+1);
            for(t=0;t<20000;++t){unsigned stage=s->tally.stage;
                uint32_t pending=s->tally.pending,score=s->tally.score;
                unsigned skip=!(map%3) && (stage==1 || stage==4) && s->tally.hold==30;
                int active=ssa_intermission_step(s,map%3==1,skip,1);
                if(skip)CHECK(!s->tally.pending && s->tally.score==score+pending && s->tally.stage==(stage==1?3u:5u));
                if(s->final_visible_ticks<180)CHECK(active==1);
                if(active<0)fprintf(stderr,"map%u lang%u tick%u stage%u score%u pending%u notice%u count%u progress%u error%u\n",map,lang,t,s->tally.stage,s->tally.score,s->tally.pending,s->notice.id,s->notice.count,s->notice.progress,s->error_line);
                CHECK(active>=0);++frames;
                if(t==35 || (s->tally.stage==4 && s->tally.hold==30) || (s->tally.stage==6 && s->tally.hold==30)){
                    CHECK(ssa_intermission_overlay(s,back,240,160,out)==1 && s->oam.count>0);
                    CHECK(ssa_intermission_overlay(s,back,240,160,repeat)==1 && !memcmp(out,repeat,38400*4));
                    if(!map){snprintf(path,sizeof path,"%s.complete%u-%u.bmp",argv[2],lang,s->tally.stage);bitmap(path,out);}
                }
                if(!active)break;
            }
            CHECK(t<20000 && s->final_visible_ticks==180 && s->tally.score==expected && !memcmp(before,g,sizeof(*g)));
            CHECK(ssa_native_completion_record(&record,&s->tally,g,&totals,0)==1 && record.totals.score==expected);
            CHECK(ssa_native_completion_record(&record,&s->tally,g,&totals,0)==-1);
            if(!map){uint32_t score,pending;
                /* Fast skip must retain every point (including score wrap),
                 * then a fresh final confirm may dismiss only after the
                 * readable minimum AND original music completion. */
                CHECK(ssa_intermission_init(s,g,lang,1)==1);
                CHECK(ssa_intermission_step(s,0,0,1)==1 && s->tally.stage==1);
                s->tally.score=UINT32_MAX-50;score=s->tally.score;pending=s->tally.pending;
                CHECK(ssa_intermission_step(s,0,1,1)==1 && s->tally.stage==3 && s->tally.score==score+pending);
                CHECK(ssa_intermission_step(s,0,0,1)==1 && s->tally.stage==4);
                CHECK(ssa_intermission_step(s,0,1,1)==1 && s->tally.stage==5 && !s->tally.pending);
                for(i=0;i<300 && s->final_visible_ticks<60;++i)CHECK(ssa_intermission_step(s,1,1,1)==1);
                CHECK(i<300 && s->final_visible_ticks==60);
                CHECK(ssa_intermission_step(s,1,1,0)==1); /* Music still active. */
                CHECK(ssa_intermission_step(s,1,0,1)==1); /* Held is not confirm. */
                CHECK(ssa_intermission_step(s,0,1,1)==0);
                CHECK(!memcmp(before,g,sizeof(*g)));
                /* No input/device: automatic readable hold, still wait for
                 * music even after the full three-second window expires. */
                CHECK(ssa_intermission_init(s,g,lang,1)==1);
                for(i=0;i<500 && s->final_visible_ticks<180;++i){
                    unsigned skip=s->tally.stage==1 || s->tally.stage==4;
                    CHECK(ssa_intermission_step(s,0,skip,0)==1);
                }
                CHECK(i<500 && s->final_visible_ticks==180 && s->tally.running);
                CHECK(ssa_intermission_step(s,0,0,0)==1);
                CHECK(ssa_intermission_step(s,0,0,1)==0);
            }
            if(!map || map==11){unsigned seen=0;
                CHECK(ssa_native_game_hud_style(g,SSA_HUD_CLASSIC)==1);memcpy(before,g,sizeof(*g));
                CHECK(ssa_intermission_init(s,g,lang,1)==1 && s->classic);
                for(i=0;i<1000;++i){unsigned stage=s->tally.stage;
                    unsigned skip=(stage==1 || stage==4) && s->tally.hold==0;
                    int active=ssa_intermission_step(s,0,skip,1);
                    CHECK(active>=0);
                    stage=s->tally.stage;
                    if((stage==1 || stage==4 || stage==6) && (s->hud.game_flags&0x200) &&
                       (stage!=6 || (s->notice.id==0xb9u+map && s->final_visible_ticks)) &&
                       !(seen&(1u<<stage)) && s->frame>=30){
                        memset(repeat,0,38400*4);
                        CHECK(ssa_intermission_overlay(s,repeat,240,160,out)==1);
                        menu_colors(out,0,160,1,1);
                        CHECK(ssa_intermission_overlay(s,back,240,160,out)==1);
                        snprintf(path,sizeof path,"%s.classic-complete%u-%u-%u.bmp",argv[2],lang,map,stage);bitmap(path,out);
                        seen|=1u<<stage;
                    }
                    if(!active)break;
                }
                CHECK(i<1000 && (seen&(1u<<4)) && (seen&(1u<<6)) &&
                    (map || (seen&(1u<<1))) && s->tally.score==expected && s->final_visible_ticks==180);
                CHECK(!memcmp(before,g,sizeof(*g)));
            }
            ssa_native_game_free(g);
        }
    }
    free(rom);free(g);free(before);free(s);free(back);free(out);free(repeat);free(audio);
    printf("{\"result\":\"pass\",\"checks\":%u,\"completion_starts\":%u,\"tally_frames\":%u,\"languages\":3,\"pause_paths\":6}\n",checks,starts,frames);return 0;
}
