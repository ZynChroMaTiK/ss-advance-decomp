#include "ssa/map_view.h"
#include "ssa/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"map-view check failed: %s line%d\n",#x,__LINE__);exit(1);}}while(0)
static unsigned checks,terminal_frames;
static unsigned energy(const uint16_t *pixels)
{
    unsigned i,total=0;
    for(i=0;i<38400;++i){unsigned c=pixels[i];total+=(c&31)+((c>>5)&31)+((c>>10)&31);}
    return total;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000),*copy;SsaMapView *v=calloc(1,sizeof(*v)),*saved=malloc(sizeof(*saved));
    SsaAudio *audio=calloc(1,sizeof(*audio));uint16_t *pixels=malloc(38402*sizeof(*pixels));FILE *f;
    unsigned family,mode,tick;
    CHECK(argc==2 && rom && v && saved && audio && pixels);
    f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_map_view_init(v,rom,0x800000,0,0,1)==1);
    *saved=*v;CHECK(ssa_map_view_init(v,rom,0x800000,0,0,1)==-1 && !memcmp(v,saved,sizeof(*v)));
    CHECK(ssa_map_view_step(v,2)==-1 && !memcmp(v,saved,sizeof(*v)));
    CHECK(ssa_map_view_draw(v,pixels,38399)==-1 && !memcmp(v,saved,sizeof(*v)));
    for(family=0;family<2;++family)for(mode=0;mode<3;++mode){
        int16_t pcm[800];unsigned level=family*6,song=family?0x85:0x84,expected,bright;
        memset(v,0,sizeof(*v));CHECK(ssa_map_view_init(v,rom,0x800000,level,0,1)==1);
        if(!audio->loaded)CHECK(ssa_audio_init(audio,rom,0x800000,24000)==1);
        ssa_audio_reset(audio);audio->muted=mode==1;audio->music_enabled=mode!=2;
        CHECK(ssa_audio_music(audio,3,level)==(mode==2?0:1));
        /* Let the actual map song end. Command3 must never auto-return to a
         * level song, nor finish the map without a fire press. */
        for(tick=0;tick<18000;++tick){
            CHECK(ssa_map_view_step(v,0)==0 && !v->leaving && !v->done);
            CHECK(ssa_audio_render(audio,pcm,400)==1 && !audio->errors);
            if(mode!=2)CHECK(audio->players[0].song==song && audio->music_command==3);
            if(tick>120 && !audio->players[0].active)break;
        }
        CHECK(tick<18000);
        for(tick=0;tick<180;++tick){
            CHECK(ssa_map_view_step(v,0)==0 && !v->done);
            CHECK(ssa_audio_render(audio,pcm,400)==1 && !audio->players[0].active);
            ++terminal_frames;
        }
        CHECK(ssa_map_view_step(v,1)==1 && v->leaving && !v->done);
        CHECK(ssa_map_view_draw(v,pixels+1,38400)==1);
        bright=energy(pixels+1);CHECK(bright>0);
        CHECK(ssa_audio_play(audio,2)==1);
        if(audio->music_enabled)CHECK(ssa_audio_fade(audio,5)==1);
        for(tick=1;tick<=61;++tick){
            CHECK(ssa_map_view_step(v,1)==0 && v->done==(tick==61));
            CHECK(ssa_audio_render(audio,pcm,400)==1);
            pixels[0]=0x1234;pixels[38401]=0x4321;
            CHECK(ssa_map_view_draw(v,pixels+1,38400)==1 && pixels[0]==0x1234 && pixels[38401]==0x4321);
            if(tick==30)CHECK(energy(pixels+1)>0 && energy(pixels+1)<bright*3/4);
            if(tick>=60)CHECK(energy(pixels+1)==0);
        }
        *saved=*v;CHECK(ssa_map_view_step(v,1)==0 && !memcmp(v,saved,sizeof(*v)));
        ssa_audio_reset(audio);CHECK(audio->muted==(mode==1) && audio->music_enabled==(mode!=2));
        CHECK(ssa_audio_level_music(audio,level)==(mode==2?0:1));
        CHECK(ssa_audio_music_select(rom,0x800000,2,level,1,&expected)==1);
        CHECK(audio->master_gain==1 && !(audio->music.flags&12));
        CHECK(mode==2?!audio->players[0].active:audio->players[0].active && audio->players[0].song==expected && audio->music_command==2);
    }
    memset(v,0,sizeof(*v));CHECK(ssa_map_view_init(v,rom,0x7fffff,0,0,1)==-1);
    CHECK(ssa_map_view_init(v,rom,0x800000,12,0,1)==-1);
    CHECK(ssa_map_view_init(v,rom,0x800000,0,3,1)==-1);
    CHECK(ssa_map_view_init(v,rom,0x800000,0,0,3)==-1);
    copy=malloc(0x800000);CHECK(copy);memcpy(copy,rom,0x800000);copy[0xab39c]=0;
    CHECK(ssa_map_view_init(v,copy,0x800000,0,0,1)==-1 && !v->loaded);
    free(copy);free(pixels);free(audio);free(saved);free(v);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"map_song_terminal_wait_frames\":%u,\"music_modes\":3,\"map_families\":2}\n",checks,terminal_frames);
    return 0;
}
