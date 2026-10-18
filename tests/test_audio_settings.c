#include "ssa/audio.h"
#include "ssa/soundfont.h"
#include "ssa/menu.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../third_party/TinySoundFont/tsf.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"settings check line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void *read_file(const char *path,size_t *size){FILE *f=fopen(path,"rb");long n;void *p;
    CHECK(f && !fseek(f,0,SEEK_END));n=ftell(f);CHECK(n>0);rewind(f);p=malloc((size_t)n);
    CHECK(p && fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return p;
}
static void screenshot(const char *dir,const char *page,unsigned language,const uint16_t *pixels){
    char path[1024];unsigned i;FILE *f;
    snprintf(path,sizeof(path),"%s/%s-%u.ppm",dir,page,language);f=fopen(path,"wb");CHECK(f);
    fprintf(f,"P6\n240 160\n255\n");
    for(i=0;i<38400;++i){unsigned c=pixels[i];unsigned char b[3]={(unsigned char)((c&31)*255/31),
        (unsigned char)(((c>>5)&31)*255/31),(unsigned char)(((c>>10)&31)*255/31)};fwrite(b,1,3,f);}
    fclose(f);
}
static unsigned peak(const int16_t *samples,unsigned count){unsigned i,p=0;
    for(i=0;i<count;++i){int v=samples[i];unsigned n=(unsigned)(v<0?-v:v);if(n>p)p=n;}return p;
}
static void probe_reset(void *p){(void)p;}
static int probe_note(void *p,unsigned n,const SsaAudioTrack *t,const SsaAudioVoice *v){(void)p;(void)n;(void)t;(void)v;return 1;}
static void probe_release(void *p,unsigned n){(void)p;(void)n;}
static void probe_control(void *p,unsigned n,const SsaAudioTrack *t){(void)p;(void)n;(void)t;}
static void probe_sample(void *p,float out[2]){out[0]=*(float *)p;out[1]=-out[0]/2;}
static void hybrid_checks(const uint8_t *rom,size_t rom_size,const void *sf,size_t sf_size,const SsaAudioSynth *synth){
    SsaAudio *native=calloc(1,sizeof(*native)),*hybrid=calloc(1,sizeof(*hybrid));
    int16_t expected[1600],actual[1600];unsigned song,track,i,pcm_cases=0;
    float signal=.125f,stereo[2],direct[64];SsaAudioSynth probe={&signal,probe_reset,probe_note,probe_release,probe_control,probe_sample,NULL};
    SsaAudioTrack t={0};SsaAudioVoice v={0};tsf *reference=tsf_load_memory(sf,(int)sf_size);
    CHECK(native && hybrid && reference);
    CHECK(ssa_audio_init(native,rom,rom_size,48000)==1 && ssa_audio_init(hybrid,rom,rom_size,48000)==1);
    CHECK(ssa_audio_set_synth(hybrid,&probe)==1);
    CHECK(ssa_audio_render(hybrid,actual,1)==1 && actual[0]==11571 && actual[1]==-5785 && hybrid->music_headroom_gain==1);
    CHECK(ssa_audio_set_volume(hybrid,50,100)==1);
    CHECK(ssa_audio_render(hybrid,actual,1)==1 && actual[0]==5785 && actual[1]==-2892);
    CHECK(ssa_audio_set_volume(hybrid,100,100)==1);signal=2;
    CHECK(ssa_audio_render(hybrid,actual,1)==1 && actual[0]==32760 && actual[1]==-16380 && !hybrid->clipped);
    CHECK(ssa_audio_set_synth(hybrid,NULL)==1 && hybrid->music_headroom_gain==1);
    /* Calibrated synth layer: -15dB relative to the bank's raw output, with
     * identical waveforms/dynamics. PCM isolation is checked separately below. */
    tsf_set_output(reference,TSF_STEREO_INTERLEAVED,48000,0);
    CHECK(tsf_channel_set_bank_preset(reference,0,0,80));
    CHECK(tsf_channel_set_pan(reference,0,64/127.0f));
    CHECK(tsf_channel_note_on(reference,0,60,1));
    t.voice=9;t.volume=127;t.pan=64;v.type=1;v.note=60;v.velocity=127;
    synth->reset(synth->context);CHECK(synth->note(synth->context,0,&t,&v)==1);
    {double raw_energy=0,balanced_energy=0;unsigned block;
        for(block=0;block<128;++block){tsf_render_float(reference,direct,32,0);
            for(i=0;i<32;++i){unsigned channel;synth->sample(synth->context,stereo);
                for(channel=0;channel<2;++channel){double raw=direct[2*i+channel],balanced=stereo[channel];
                    CHECK(fabs(balanced-raw*0.1778279410)<0.000001+fabs(raw)*0.00001);
                    raw_energy+=raw*raw;balanced_energy+=balanced*balanced;}
            }
        }
        CHECK(raw_energy>0 && balanced_energy/raw_energy>0.031 && balanced_energy/raw_energy<0.032);
    }
    tsf_close(reference);
    /* PCM routing follows the resolved type, including a rhythm split, not
     * its program number. It must produce no duplicate SoundFont layer. */
    v.type=0;
    for(i=0;i<128;++i){t.voice=i;synth->reset(synth->context);CHECK(synth->note(synth->context,0,&t,&v)==1);
        for(track=0;track<32;++track){synth->sample(synth->context,stereo);CHECK(!stereo[0] && !stereo[1]);}}
    /* Existing original PCM boost2 and music gain0.5 cancel. Compare the same
     * waveform with hybrid's +3dB balance and the final 2x/3x mode output gains. */
    for(song=0x6e;song<SSA_AUDIO_SONGS;++song){
        ssa_audio_reset(native);CHECK(ssa_audio_play(native,song)==1);
        CHECK(ssa_audio_render(native,expected,800)==1);
        for(track=0;track<SSA_AUDIO_MUSIC_VOICES;++track)if(native->voices[track].active && !native->voices[track].type){
            ssa_audio_reset(hybrid);*hybrid=*native;
            hybrid->players[0].active=0;hybrid->music_headroom_gain=1;
            for(i=0;i<SSA_AUDIO_VOICES;++i)if(i!=track)hybrid->voices[i].active=0;
            for(i=0;i<SSA_AUDIO_TRACKS;++i)if(i!=track)hybrid->players[0].tracks[i].synth_on=0;
            {SsaAudio *original=malloc(sizeof(*original));CHECK(original);*original=*hybrid;
                CHECK(ssa_audio_set_synth(hybrid,synth)==1);
                CHECK(ssa_audio_render(hybrid,actual,800)==1 && ssa_audio_render(original,expected,800)==1);
                {unsigned sample_index;int matches=1;
                    for(sample_index=0;sample_index<1600;++sample_index)
                        if(fabs(actual[sample_index]-expected[sample_index]*pow(10.0,3.0/20.0)*2.0/3.0)>2)matches=0;
                    CHECK(matches);}
                if(peak(actual,1600))++pcm_cases;
                CHECK(ssa_audio_set_synth(hybrid,NULL)==1);free(original);}
        }
    }
    CHECK(pcm_cases>=10);synth->reset(synth->context);free(native);free(hybrid);
}
int main(int argc,char **argv){
    size_t rom_size,sf_size;uint8_t *rom,*sf,encoded[16],bad[16];
    SsaSoundFont *font;SsaAudioSynth synth;SsaAudio *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    SsaMenu *m=calloc(1,sizeof(*m));SsaSettings prefs=SSA_SETTINGS_DEFAULT,decoded={0};
    uint16_t pixels[38402];int16_t out[1600],ref[1600];unsigned i,j,k,mode,lang,song,max_peak=0,original_peak=0;
    clock_t begin=clock();
    unsigned used[128]={0},lo[128],hi[128]={0};
    for(i=0;i<128;++i)lo[i]=127;
    CHECK(argc==4 && a && b && m);rom=read_file(argv[1],&rom_size);sf=read_file(argv[2],&sf_size);
    font=ssa_soundfont_load(sf,sf_size,48000);CHECK(font);synth=ssa_soundfont_synth(font);
    CHECK(!ssa_soundfont_load("invalid",7,48000));
    hybrid_checks(rom,rom_size,sf,sf_size,&synth);
    CHECK(ssa_settings_encode(&prefs,encoded,sizeof encoded)==1);
    CHECK(ssa_settings_decode(&decoded,encoded,sizeof encoded)==1 && decoded.music_volume==100 && decoded.sound_volume==100 && !decoded.music_style && decoded.renderer==SSA_RENDER_SOFTWARE && !decoded.jump_enabled && decoded.ingame_netricsa && decoded.thirdperson_enabled && decoded.display_mode==SSA_DISPLAY_FULLSCREEN && !decoded.alternate_weapons);
    prefs.alternate_weapons=1;
    CHECK(ssa_settings_encode(&prefs,encoded,16)==1 && encoded[8]==6 && (encoded[9]&128));
    CHECK(ssa_settings_decode(&decoded,encoded,16)==1 && decoded.alternate_weapons);
    prefs.alternate_weapons=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs.alternate_weapons=0;
    {static const uint8_t legacy[16]={0x53,0x53,0x41,0x2d,0x4f,0x50,0x54,0x00,0x01,0x02,0x46,0x50,0xb7,0x85,0xad,0xe1};
        CHECK(ssa_settings_decode(&decoded,legacy,16)==1 && decoded.music_style==SSA_MUSIC_MIDI && decoded.music_volume==70 && decoded.sound_volume==80 && !decoded.renderer && !decoded.jump_enabled && decoded.ingame_netricsa && decoded.thirdperson_enabled);
    }
    {static const uint8_t legacy[16]={0x53,0x53,0x41,0x2d,0x4f,0x50,0x54,0x00,0x02,0x06,0x46,0x50,0x85,0x82,0x11,0xf4};
        CHECK(ssa_settings_decode(&decoded,legacy,16)==1 && decoded.music_style==SSA_MUSIC_MIDI && decoded.music_volume==70 && decoded.sound_volume==80 && decoded.renderer==SSA_RENDER_GBA && !decoded.jump_enabled && decoded.ingame_netricsa && decoded.thirdperson_enabled);
    }
    {static const uint8_t legacy[16]={0x53,0x53,0x41,0x2d,0x4f,0x50,0x54,0x00,0x03,0x0e,0x46,0x50,0x58,0xb4,0xbe,0x42};
        CHECK(ssa_settings_decode(&decoded,legacy,16)==1 && decoded.music_style==SSA_MUSIC_MIDI && decoded.renderer==SSA_RENDER_GBA && decoded.jump_enabled && !decoded.ingame_netricsa && decoded.thirdperson_enabled);
    }
    {static const uint8_t legacy[16]={0x53,0x53,0x41,0x2d,0x4f,0x50,0x54,0x00,0x04,0x3e,0x46,0x50,0x71,0x69,0x02,0xfb};
        CHECK(ssa_settings_decode(&decoded,legacy,16)==1 && decoded.music_style==SSA_MUSIC_MIDI && decoded.renderer==SSA_RENDER_GBA && decoded.jump_enabled && decoded.ingame_netricsa && decoded.thirdperson_enabled && decoded.display_mode==SSA_DISPLAY_FULLSCREEN);
    }
    {static const uint8_t legacy[16]={0x53,0x53,0x41,0x2d,0x4f,0x50,0x54,0x00,0x05,0x7e,0x46,0x50,0xd4,0x83,0x24,0x33};
        CHECK(ssa_settings_decode(&decoded,legacy,16)==1 && !decoded.alternate_weapons && decoded.display_mode==SSA_DISPLAY_WINDOWED && decoded.music_style==SSA_MUSIC_MIDI && decoded.renderer==SSA_RENDER_GBA && decoded.jump_enabled && decoded.ingame_netricsa && decoded.thirdperson_enabled);
    }
    for(i=0;i<2;++i)for(j=0;j<2;++j)for(k=0;k<2;++k)for(mode=0;mode<2;++mode){prefs.renderer=i;prefs.jump_enabled=j;prefs.ingame_netricsa=k;prefs.thirdperson_enabled=mode;
        CHECK(ssa_settings_encode(&prefs,encoded,16)==1 && ssa_settings_decode(&decoded,encoded,16)==1 && !memcmp(&prefs,&decoded,sizeof prefs));}
    prefs.renderer=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);prefs.renderer=0;
    prefs.jump_enabled=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs.jump_enabled=1;prefs.ingame_netricsa=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs.ingame_netricsa=1;prefs.thirdperson_enabled=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs.thirdperson_enabled=1;prefs.display_mode=SSA_DISPLAY_WINDOWED;
    CHECK(ssa_settings_encode(&prefs,encoded,16)==1 && ssa_settings_decode(&decoded,encoded,16)==1 && decoded.display_mode==SSA_DISPLAY_WINDOWED);
    prefs.display_mode=2;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs=(SsaSettings){SSA_MUSIC_MIDI,70,80,SSA_RENDER_GBA,0,0,0,SSA_DISPLAY_WINDOWED,0};CHECK(ssa_settings_encode(&prefs,encoded,16)==1);
    CHECK(ssa_settings_decode(&decoded,encoded,16)==1 && decoded.music_style==SSA_MUSIC_MIDI && decoded.music_volume==70);
    prefs.music_style=SSA_MUSIC_STYLES;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    prefs=(SsaSettings){1,35,80,SSA_RENDER_SOFTWARE,1,1,1,SSA_DISPLAY_FULLSCREEN,0};CHECK(ssa_settings_encode(&prefs,encoded,16)==1);
    CHECK(ssa_settings_decode(&decoded,encoded,16)==1 && !memcmp(&prefs,&decoded,sizeof prefs));
    for(i=0;i<16;++i){memcpy(bad,encoded,16);bad[i]^=1;CHECK(ssa_settings_decode(&decoded,bad,16)==-1 && !memcmp(&prefs,&decoded,sizeof prefs));}
    prefs.music_volume=101;CHECK(ssa_settings_encode(&prefs,encoded,16)==-1);
    for(lang=0;lang<3;++lang){
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,rom_size,1)==1);m->language=lang;
        m->page=m->draw_page=SSA_MENU_OPTIONS;m->selection=4;m->fading=m->fade_volume=0;
        CHECK(ssa_menu_pointer(m,120,82,1)==SSA_MENU_CONFIRM && m->selection==4);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->page==SSA_MENU_SETTINGS);
        CHECK(m->settings.display_mode==SSA_DISPLAY_FULLSCREEN);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.display_mode==SSA_DISPLAY_WINDOWED);
        CHECK(ssa_menu_pointer(m,120,30,1)==SSA_MENU_CONFIRM && !m->selection);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.display_mode==SSA_DISPLAY_FULLSCREEN);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==1);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_GBA);
        CHECK(ssa_menu_pointer(m,120,50,1)==SSA_MENU_CONFIRM && m->selection==1);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.renderer==SSA_RENDER_SOFTWARE);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==2);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && !m->settings_unavailable && !m->settings.music_style);
        m->midi_available=1;
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.music_style==SSA_MUSIC_MIDI);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.music_style==SSA_MUSIC_ORIGINAL);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.music_style==SSA_MUSIC_MIDI);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.music_style==SSA_MUSIC_ORIGINAL);
        m->soundfont_available=1;
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.music_style);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.music_style==SSA_MUSIC_MIDI);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.music_style==SSA_MUSIC_SOUNDFONT);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==3);
        for(i=0;i<25;++i)CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1);
        CHECK(!m->settings.music_volume);
        for(i=0;i<25;++i)CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1);
        CHECK(m->settings.music_volume==100);
        CHECK(ssa_menu_pointer(m,68,100,1)==0 && m->settings.music_volume==25);
        CHECK(ssa_menu_pointer(m,148,132,1)==0 && m->settings.sound_volume==80 && m->selection==4);
        pixels[0]=0x1234;pixels[38401]=0x4321;
        CHECK(ssa_menu_draw(m,pixels+1,38400)==1 && pixels[0]==0x1234 && pixels[38401]==0x4321);
        CHECK(m->canvas.glyph_count<128);screenshot(argv[3],"settings",lang,pixels+1);
        CHECK(ssa_menu_step(m,SSA_MENU_BACK)==1 && m->page==SSA_MENU_OPTIONS && m->selection==4);
        CHECK(ssa_menu_step(m,0)==1);
        CHECK(ssa_menu_pointer(m,120,98,1)==SSA_MENU_CONFIRM && m->selection==5);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->page==SSA_MENU_EXTRA && !m->selection);
        CHECK(!m->settings.jump_enabled);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.jump_enabled);
        CHECK(ssa_menu_pointer(m,120,60,1)==SSA_MENU_CONFIRM && !m->selection);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && !m->settings.jump_enabled);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==1);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && !m->settings.thirdperson_enabled);
        CHECK(ssa_menu_pointer(m,120,72,1)==SSA_MENU_CONFIRM && m->selection==1);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.thirdperson_enabled);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==2);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && !m->settings.ingame_netricsa);
        CHECK(ssa_menu_step(m,0)==1 && ssa_menu_draw(m,pixels+1,38400)==1);
        screenshot(argv[3],"extra",lang,pixels+1);
        CHECK(ssa_menu_pointer(m,120,96,1)==SSA_MENU_CONFIRM && m->selection==2);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.ingame_netricsa);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==3);
        CHECK(!m->settings.alternate_weapons);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.alternate_weapons);
        CHECK(ssa_menu_pointer(m,120,120,1)==SSA_MENU_CONFIRM && m->selection==3);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && !m->settings.alternate_weapons);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==4);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->page==SSA_MENU_OPTIONS && m->selection==5);
        CHECK(ssa_menu_step(m,0)==1 && ssa_menu_draw(m,pixels+1,38400)==1);
        screenshot(argv[3],"options",lang,pixels+1);
    }
    CHECK(ssa_audio_init(a,rom,rom_size,48000)==1 && ssa_audio_init(b,rom,rom_size,48000)==1);
    CHECK(ssa_audio_set_volume(a,101,0)==-1 && a->music_volume==100);
    /* User bus gains affect only their category, with identical sequence clocks. */
    CHECK(ssa_audio_play(a,0x73)==1 && ssa_audio_play(b,0x73)==1);
    CHECK(ssa_audio_set_volume(a,0,100)==1);
    CHECK(ssa_audio_render(a,out,800)==1 && ssa_audio_render(b,ref,800)==1 && peak(out,1600)==0);
    CHECK(!memcmp(a->players,b->players,sizeof a->players));
    ssa_audio_reset(a);ssa_audio_reset(b);
    CHECK(ssa_audio_play(a,13)==1 && ssa_audio_play(b,13)==1);
    CHECK(ssa_audio_render(a,out,800)==1 && ssa_audio_render(b,ref,800)==1 && !memcmp(out,ref,sizeof out));
    CHECK(ssa_audio_set_volume(a,100,0)==1);ssa_audio_reset(a);
    CHECK(ssa_audio_play(a,13)==1 && ssa_audio_render(a,out,800)==1 && !peak(out,1600));
    ssa_audio_reset(a);ssa_audio_reset(b);CHECK(ssa_audio_set_volume(b,100,0)==1);
    CHECK(ssa_audio_play(a,0x73)==1 && ssa_audio_play(b,0x73)==1);
    CHECK(ssa_audio_render(a,out,800)==1 && ssa_audio_render(b,ref,800)==1 && !memcmp(out,ref,sizeof out));
    /* Switching instruments neither restarts the song nor changes sequencer state. */
    {SsaAudioPlayer saved=a->players[0];uint64_t starts=a->starts;
        CHECK(ssa_audio_set_synth(a,&synth)==1 && a->starts==starts && !memcmp(&saved,&a->players[0],sizeof saved));}
    CHECK(ssa_audio_set_volume(a,100,100)==1);
    CHECK(ssa_audio_render(a,out,800)==1 && peak(out,1600)>0);
    CHECK(ssa_audio_set_synth(a,NULL)==1);
    /* Every music sequence: same events/timing as Original, audible SF output,
     * independent sound-effect PCM, no hard clipping. No campaign playthrough. */
    for(song=0x6e;song<SSA_AUDIO_SONGS;++song){unsigned heard=0;
        ssa_audio_reset(a);ssa_audio_reset(b);CHECK(ssa_audio_set_volume(b,100,100)==1);
        CHECK(ssa_audio_set_synth(a,&synth)==1);
        CHECK(ssa_audio_play(a,song)==1 && ssa_audio_play(b,song)==1);
        for(i=0;i<480;++i){
            CHECK(ssa_audio_render(a,out,800)==1 && ssa_audio_render(b,ref,800)==1);
            CHECK(!memcmp(a->players,b->players,sizeof a->players));
            heard|=peak(out,1600);j=peak(out,1600);if(j>max_peak)max_peak=j;
            j=peak(ref,1600);if(j>original_peak)original_peak=j;
            for(j=0;j<a->players[0].count;++j){SsaAudioTrack *t=&a->players[0].tracks[j];unsigned v=t->voice,k=t->synth_note;
                if(t->synth_on && v<128){++used[v];if(k<lo[v])lo[v]=k;if(k>hi[v])hi[v]=k;}}
        }
        CHECK(heard && !a->errors && !a->clipped && !b->errors && !b->clipped);
    }
    fprintf(stderr,"Music peaks: SoundFont %u, Original %u\n",max_peak,original_peak);
    CHECK(max_peak>=32759 && max_peak<=32760 && original_peak>=32759 && original_peak<=32760);
    /* Muting advances SF and original music together; reset has no stuck tails. */
    a->muted=1;CHECK(ssa_audio_render(a,out,800)==1 && !peak(out,1600));a->muted=0;
    ssa_audio_reset(a);CHECK(ssa_audio_render(a,out,800)==1 && !peak(out,1600));
    CHECK(ssa_audio_set_volume(a,0,100)==1);ssa_audio_reset(b);
    CHECK(ssa_audio_play(a,13)==1 && ssa_audio_play(b,13)==1);
    CHECK(ssa_audio_render(a,out,800)==1 && ssa_audio_render(b,ref,800)==1 && !memcmp(out,ref,sizeof out));
    /* Buffer splitting does not change the SF sample stream. */
    ssa_audio_reset(a);CHECK(ssa_audio_set_volume(a,100,100)==1);CHECK(ssa_audio_play(a,0x73)==1);
    CHECK(ssa_audio_render(a,out,800)==1);ssa_audio_reset(a);CHECK(ssa_audio_play(a,0x73)==1);
    for(i=0;i<800;++i)CHECK(ssa_audio_render(a,ref+i*2,1)==1);
    CHECK(!memcmp(out,ref,sizeof out));
    CHECK(ssa_audio_set_synth(a,NULL)==1);ssa_soundfont_free(font);
    if(getenv("SSA_INSTRUMENT_REPORT"))for(i=0;i<128;++i)if(used[i])fprintf(stderr,"voice %u notes %u..%u observations %u\n",i,lo[i],hi[i],used[i]);
    printf("{\"result\":\"pass\",\"checks\":%u,\"music_sequences\":26,\"soundfont_seconds\":208,\"peak\":%u,\"hybrid_music\":true,\"music_limited_frames\":%llu,\"cpu_seconds\":%.2f}\n",checks,max_peak,(unsigned long long)a->music_limited_frames,(double)(clock()-begin)/CLOCKS_PER_SEC);
    free(rom);free(sf);free(a);free(b);free(m);return 0;
}
