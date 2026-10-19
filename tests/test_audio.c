#include "ssa/audio.h"
#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks,songs,audible,play_events,stop_events,walking_events,airborne_frames,blocked_frames,intro_returns;
static unsigned max_pcm,max_psg,catalog_peak;
static unsigned effects_scope_cases;
static uint64_t samples,catalog_clipped;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"audio line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static uint32_t read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static unsigned latest(const SsaAudio *a,unsigned song){unsigned i,best=0;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].song==song &&
       a->players[i].serial>a->players[best].serial)best=i;
    CHECK(best);return best;
}
static unsigned instances(const SsaAudio *a,unsigned song){unsigned i,count=0;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)count+=a->players[i].active && a->players[i].song==song;
    return count;
}
static void mix_frames(SsaAudio *a,size_t frames){int16_t out[1600];
    while(frames){size_t count=frames>800?800:frames;CHECK(ssa_audio_render(a,out,count)==1);frames-=count;}
}
static void catalog(SsaAudio *a){unsigned song;int16_t block[800];
    for(song=0;song<SSA_AUDIO_SONGS;++song){unsigned tick,energy=0,frames=song>=0x6e?45*60:3*60;int result;uint64_t before=a->clipped;
        ssa_audio_reset(a);result=ssa_audio_play(a,song);CHECK(result>=0);
        if(!result)continue;
        ++songs;
        for(tick=0;tick<frames;++tick){unsigned i;
            CHECK(ssa_audio_render(a,block,400)==1);
            if(a->errors)fprintf(stderr,"song%x error song%x command%x tick%u\n",song,a->last_error_song,a->last_error_command,tick);
            CHECK(!a->errors);
            {unsigned pcm=0,psg=0,tracks=0;
                for(i=0;i<SSA_AUDIO_VOICES;++i)if(a->voices[i].active){
                    unsigned type=a->voices[i].type;
                    if(!a->voices[i].player){unsigned bit=1u<<a->voices[i].track;
                        CHECK(i==a->voices[i].track && !(tracks&bit));tracks|=bit;}
                    if(type)++psg;else ++pcm;
                }
                CHECK(pcm+psg<=SSA_AUDIO_VOICES);if(pcm>max_pcm)max_pcm=pcm;if(psg>max_psg)max_psg=psg;
            }
            for(i=0;i<800;++i){unsigned peak=(unsigned)abs(block[i]);energy|=(unsigned)(block[i]!=0);
                if(peak>catalog_peak)catalog_peak=peak;}
            samples+=400;
        }
        CHECK(energy);++audible;
        catalog_clipped+=a->clipped-before;
        if(a->clipped!=before)fprintf(stderr,"song%x clipped %llu samples\n",song,(unsigned long long)(a->clipped-before));
        CHECK(a->clipped==before);
        CHECK(ssa_audio_stop(a,song)==1);
    }
}
static void pcm_bus_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0};unsigned player,slot,i;int16_t out[16];
    static const unsigned slots[]={0,1,SSA_AUDIO_MUSIC_VOICES,SSA_AUDIO_VOICES-1};
    CHECK(copy);memcpy(copy,rom,0x800000);memset(copy+0x7fdf00,64,4);
    /* Every shared PCM channel and the footstep slot use the same gain,
     * whether their owner is music, effects or the PC footstep player. */
    for(player=0;player<SSA_AUDIO_PLAYERS;++player)for(slot=0;slot<4;++slot){
        SsaAudioVoice *v;SsaAudioPlayer *p;memset(&a,0,sizeof(a));
        CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1);p=&a.players[player];
        p->left=p->right=p->target_left=p->target_right=1;p->tracks[0].volume=127;p->tracks[0].pan=64;
        v=&a.voices[slots[slot]];v->active=1;v->player=player;v->velocity=127;
        v->wave=0x7fdf00;v->length=4;v->looped=1;v->step=1;v->stage=2;v->envelope_level=255;
        CHECK(ssa_audio_render(&a,out,8)==1);
        for(i=0;i<8;++i)CHECK(out[i*2]==6912 && out[i*2+1]==6976);
        CHECK(!a.clipped && !a.errors);
    }
    /* Actual summation of all three sample channels retains their relative
     * dynamics and stays within16-bit headroom for this reference signal. */
    ssa_audio_reset(&a);a.players[0].left=a.players[0].right=a.players[0].target_left=a.players[0].target_right=1;
    a.players[0].tracks[0].volume=127;a.players[0].tracks[0].pan=64;
    for(i=0;i<3;++i){SsaAudioVoice *v=&a.voices[i];v->active=1;v->velocity=127;
        v->wave=0x7fdf00;v->length=4;v->looped=1;v->step=1;v->stage=2;v->envelope_level=255;}
    CHECK(ssa_audio_render(&a,out,1)==1 && out[0]==20736 && out[1]==20928 && !a.clipped);
    free(copy);
}
static void effects_bus_fixture(const uint8_t *rom){
    static const struct {unsigned song,boosted;} cases[]={
        {0,0},{1,0},{2,0},{5,0},{6,0},{7,1},{13,1},{14,1},
        {0x14,1},{0x68,1},{0x6d,1},{0x6e,0},{0x73,0},{0x85,0}};
    uint8_t *copy=malloc(0x800000);SsaAudio a={0};unsigned c,player,type;int16_t out[2];
    CHECK(copy);memcpy(copy,rom,0x800000);memset(copy+0x7fdf00,64,4);memset(copy+0x7fdf20,15,16);
    /* Gameplay effects get50% more amplitude on every player and oscillator
     * kind. Menu cues stay unchanged; the music balance trim and new output
     * lift combine to1.5x, preserving the PCM/PSG balance. */
    for(c=0;c<sizeof(cases)/sizeof(cases[0]);++c)for(player=0;player<SSA_AUDIO_PLAYERS;++player)
    for(type=0;type<=4;++type){SsaAudioVoice *v;SsaAudioPlayer *p;int left,right;
        memset(&a,0,sizeof(a));CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1);p=&a.players[player];
        p->song=cases[c].song;p->left=p->right=p->target_left=p->target_right=1;
        p->tracks[0].volume=127;p->tracks[0].pan=64;
        v=&a.voices[0];v->active=1;v->player=player;v->type=type;v->key=60;v->velocity=127;
        v->wave=type==3?0x7fdf20:0x7fdf00;v->length=4;v->looped=1;v->step=1;
        v->stage=2;v->envelope_level=type?15:255;v->duty=2;v->lfsr=0x7fff;
        left=type?(type==3?-3840:3840):6912;right=type?left:6976;
        if(cases[c].boosted){left=left*3/2;right=right*3/2;}
        if(cases[c].song>=0x6e){left=left*3/2;right=right*3/2;}
        CHECK(ssa_audio_render(&a,out,1)==1 && out[0]==left && out[1]==right);
        CHECK(!a.clipped && !a.errors);++effects_scope_cases;
    }
    free(copy);
}
static void effects_headroom_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0},b={0};unsigned pass,i;int16_t whole[800],chunks[800];double held;
    CHECK(copy);memcpy(copy,rom,0x800000);memset(copy+0x7fdf00,64,4);
    for(pass=0;pass<2;++pass){SsaAudio *m=pass?&b:&a;
        CHECK(ssa_audio_init(m,copy,0x800000,24000)==1 && m->effects_headroom_gain==1);
        for(i=0;i<4;++i){SsaAudioPlayer *p=&m->players[i];SsaAudioVoice *v=&m->voices[i==3?SSA_AUDIO_VOICES-1:i];
            p->song=i?0x14:0x73;p->local_priority=i!=0;p->left=p->right=p->target_left=p->target_right=1;
            p->tracks[0].volume=127;p->tracks[0].pan=64;
            v->active=1;v->player=i;v->velocity=127;v->wave=0x7fdf00;v->length=4;
            v->looped=1;v->step=1;v->stage=2;v->envelope_level=255;
        }
        /* Music10368/10464 plus three effects10368/10464 would overflow.
         * The right side fixes a common FX gain22296/31392, preserving pan. */
        CHECK(ssa_audio_render(m,whole,1)==1 && whole[0]==32459 && whole[1]==32760);
        CHECK(m->effects_headroom_gain>0.71 && m->effects_headroom_gain<0.72);
        CHECK(!m->clipped && m->effects_limited_frames==1);
        for(i=1;i<SSA_AUDIO_VOICES;++i)m->voices[i].active=0;
    }
    held=a.effects_headroom_gain;
    CHECK(ssa_audio_render(&a,whole,400)==1);
    for(i=0;i<400;){unsigned count=400-i;if(count>37)count=37;
        CHECK(ssa_audio_render(&b,chunks+2*i,count)==1);i+=count;}
    CHECK(!memcmp(whole,chunks,sizeof(whole)) && a.effects_headroom_gain==b.effects_headroom_gain);
    /* Music is unaffected during both peak protection and recovery. */
    for(i=0;i<400;++i)CHECK(whole[2*i]==10368 && whole[2*i+1]==10464);
    CHECK(a.effects_headroom_gain>held && a.effects_headroom_gain<1);
    a.muted=1;mix_frames(&a,48000);CHECK(a.effects_headroom_gain==1 && !a.clipped);
    ssa_audio_reset(&a);CHECK(a.effects_headroom_gain==1 && a.muted);
    free(copy);
}
static void channel_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0};uint32_t h,bank,wave;unsigned type,i,which;
    CHECK(copy);memcpy(copy,rom,0x800000);h=read32(copy+0x3e99d0+0x14*8)-0x08000000;
    bank=0x7fe000;wave=0x7fe800;copy[h]=4;copy[h+2]=10;put32(copy+h+4,0x08000000+bank);
    put32(copy+wave,0x40000000);put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,16);
    memset(copy+wave+16,64,16);
    for(type=0;type<=4;++type)for(which=0;which<2;++which){unsigned active=0;SsaAudioVoice *last=NULL;
        uint8_t tone[]={0,60,0,0,0,0,0,0,0,0,15,0};tone[0]=(uint8_t)type;
        if(!type){tone[8]=255;tone[10]=255;}
        memcpy(copy+bank+20*12,tone,12);
        put32(copy+bank+20*12+4,type==0?0x08000000+wave:type==3?0x08000000+wave+16:2);
        for(i=0;i<4;++i){uint8_t seq[]={0xba,0,0xbd,20,0xbe,127,0xcf,60,127,0xb0,0xb1};
            seq[1]=(uint8_t)(which?i:0);seq[7]=(uint8_t)(60+i);
            put32(copy+h+8+i*4,0x087ff000+i*32);memcpy(copy+0x7ff000+i*32,seq,sizeof(seq));}
        memset(&a,0,sizeof(a));CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_play(&a,0x14)==1);
        for(i=0;i<SSA_AUDIO_VOICES;++i)if(a.voices[i].active){++active;last=&a.voices[i];CHECK(last->type==type);}
        CHECK(active==4);CHECK(last && last->track==3 && last->priority==(which?13u:10u));
        mix_frames(&a,4000);CHECK(!a.errors && !a.clipped);
        CHECK(ssa_audio_stop(&a,0x14)==1);for(i=0;i<SSA_AUDIO_VOICES;++i)CHECK(!a.voices[i].active);
    }
    /* Independent peak-to-peak balance: PCM64 at master13 ->3456/3488,
     * then the PC sample bus and effect gain produce10368/10464;
     * full PSG pulse/noise span60 FIFO units; wave nibble span15*4=60. */
    copy[h]=1;
    for(type=0;type<=4;++type)for(which=0;which<4;++which){int16_t block[2400];int low=32767,high=-32768;
        static const unsigned volume[]={127,64,32,105};
        static const int pcm_left[]={10368,5184,2496,8448},pcm_right[]={10464,5184,2592,8640};
        static const int wave_peak[]={5760,2688,1152,4224},other_peak[]={5760,2688,1152,4608};
        uint8_t tone[]={0,60,0,0,2,0,0,0,0,0,15,0};tone[0]=(uint8_t)type;
        if(!type){tone[8]=255;tone[10]=255;}
        memcpy(copy+bank+20*12,tone,12);
        put32(copy+bank+20*12+4,type==0?0x08000000+wave:type==3?0x08000000+wave+16:2);
        memset(copy+wave+16,type==3?0x0f:64,16);
        copy[0x7ff005]=(uint8_t)volume[which];
        memset(&a,0,sizeof(a));CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_play(&a,0x14)==1);
        CHECK(ssa_audio_render(&a,block,1200)==1);
        for(i=0;i<1200;++i){if(block[i*2]<low)low=block[i*2];if(block[i*2]>high)high=block[i*2];}
        if(type){int peak=type==3?wave_peak[which]:other_peak[which];CHECK(low==-peak && high==peak);}
        else CHECK(low==pcm_left[which] && high==low && block[1]==pcm_right[which]);
    }
    /* A32-step wave runs at half the pulse frequency for the same ROM
     * period register. At key60, the half-cycle ends after sample91. */
    copy[bank+20*12]=3;put32(copy+bank+20*12+4,0x08000000+wave+16);copy[0x7ff005]=127;
    memset(copy+wave+16,255,8);memset(copy+wave+24,0,8);
    memset(&a,0,sizeof(a));CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_play(&a,0x14)==1);
    {int16_t block[184];CHECK(ssa_audio_render(&a,block,92)==1);CHECK(block[180]==5760 && block[182]==-5760);}
    free(copy);
}
static void semantics(SsaAudio *a,const uint8_t *rom){
    SsaAudio b={0};int16_t *whole=malloc(48000*sizeof(*whole)),*chunks=malloc(48000*sizeof(*chunks));size_t n;
    CHECK(whole && chunks);CHECK(ssa_audio_init(&b,rom,0x800000,24000)==1);
    ssa_audio_reset(a);CHECK(ssa_audio_play(a,0x12)==1);
    CHECK(ssa_audio_play(a,0x22)==1);CHECK(a->players[2].song==0x12 && a->players[3].song==0x22);
    CHECK(ssa_audio_stop(a,0x22)==1 && a->players[2].active);
    CHECK(ssa_audio_play(a,0xf)==1 && instances(a,0x12)==1 && instances(a,0xf)==1);
    CHECK(ssa_audio_play(a,0x14)==1);CHECK(ssa_audio_stop(a,0x12)==1 && instances(a,0x14)==1 && instances(a,0xf)==1);
    ssa_audio_reset(a);CHECK(ssa_audio_play(a,0x12)==1);
    CHECK(ssa_audio_render(a,whole,24000)==1 && a->loops);
    CHECK(ssa_audio_play(&b,0x12)==1);
    for(n=0;n<24000;){size_t count=24000-n;if(count>317)count=317;
        CHECK(ssa_audio_render(&b,chunks+2*n,count)==1);n+=count;}
    CHECK(!memcmp(whole,chunks,48000*sizeof(*whole)));
    CHECK(ssa_audio_stop(a,0x12)==1);CHECK(ssa_audio_render(a,whole,24000)==1);
    for(n=0;n<48000;++n)CHECK(whole[n]==0);
    ssa_audio_reset(a);CHECK(ssa_audio_play(a,0x14)==1);a->muted=1;
    CHECK(ssa_audio_render(a,whole,24000)==1);
    for(n=0;n<48000;++n)CHECK(whole[n]==0);
    a->muted=0;a->music_enabled=0;CHECK(ssa_audio_level_music(a,0)==0);
    CHECK(ssa_audio_play(a,0x14)==1);a->music_enabled=1;
    {unsigned tick;uint64_t before=a->loops;
        ssa_audio_reset(a);CHECK(ssa_audio_level_music(a,0)==1);
        for(tick=0;tick<300*60 && a->loops==before;++tick){CHECK(ssa_audio_render(a,whole,400)==1);CHECK(!a->errors);}
        CHECK(tick<300*60 && a->players[0].active); /* First-level song reaches its real loop. */
    }
    for(n=0;n<14;++n){ssa_audio_reset(a);CHECK(ssa_audio_level_music(a,(unsigned)n)==1);}
    CHECK(ssa_audio_level_music(a,14)==0 && ssa_audio_level_music(a,15)==0);
    CHECK(ssa_audio_play(a,SSA_AUDIO_SONGS)==-1 && ssa_audio_level_music(a,16)==-1);
    CHECK(ssa_audio_render(a,NULL,1)==-1 && ssa_audio_render(a,NULL,0)==1);
    /* Both feet use ROM player0, which would otherwise replace the music.
     * Repeated steps must preserve every music track and its clock. */
    ssa_audio_reset(a);ssa_audio_reset(&b);
    CHECK(ssa_audio_level_music(a,0)==1 && ssa_audio_level_music(&b,0)==1);
    for(n=0;n<180;++n){
        if(n%18==0){CHECK(ssa_audio_play(a,n%36?13:14)==1);
            CHECK(instances(a,n%36?13:14)>=1);}
        CHECK(ssa_audio_render(a,whole,400)==1 && ssa_audio_render(&b,chunks,400)==1);
        CHECK(!memcmp(&a->players[0],&b.players[0],sizeof(a->players[0])));
        CHECK(!memcmp(a->voices,b.voices,SSA_AUDIO_MUSIC_VOICES*sizeof(a->voices[0])));
    }
    CHECK(ssa_audio_stop(a,13)==1 && a->players[0].active);
    CHECK(ssa_audio_stop(a,14)==1 && a->players[0].song==0x73 && a->players[0].active);
    ssa_audio_reset(a);a->music_enabled=0;
    CHECK(ssa_audio_play(a,13)==1 && !a->players[0].active);
    CHECK(ssa_audio_stop(a,13)==1 && !a->players[SSA_AUDIO_FOOTSTEP_PLAYER].active);a->music_enabled=1;
    free(whole);free(chunks);
}
static void spatial_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0},b={0};uint32_t h,tone,wave,track;unsigned i;
    SsaAudioListener ear={{0,0,0},65536,0};int16_t out[2];
    static const struct {int x,y,z,left,right;} cases[]={
        {0,256,0,5184,5232},{256,0,0,1296,5232},{-256,0,0,5184,1308},
        {0,-256,0,5184,5232},{0,1024,0,2073,2092},{0,0,1024,2073,2092},{0,0,0,10368,10464}};
    static const uint8_t sequence[]={0xbd,20,0xcf,60,127,0xb0,0xb1};
    CHECK(copy);memcpy(copy,rom,0x800000);
    h=read32(copy+0x3e99d0+0x14*8)-0x08000000;tone=read32(copy+h+4)-0x08000000+20*12;
    wave=read32(copy+tone+4)-0x08000000;track=read32(copy+h+8)-0x08000000;
    put32(copy+wave,0x40000000);put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,4);
    memset(copy+wave+16,64,4);copy[tone+8]=255;copy[tone+9]=0;copy[tone+10]=255;copy[tone+11]=0;
    memcpy(copy+track,sequence,sizeof(sequence));
    CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_init(&b,copy,0x800000,24000)==1);
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i){SsaAudioPoint point={cases[i].x*65536,cases[i].y*65536,cases[i].z*65536};
        ssa_audio_reset(&a);CHECK(ssa_audio_listen(&a,&ear)==1 && ssa_audio_play_at(&a,0x14,&point)==1);
        a.players[2].tracks[0].volume=127;CHECK(ssa_audio_render(&a,out,1)==1);
        CHECK(abs(out[0]-cases[i].left)<=2 && abs(out[1]-cases[i].right)<=2);
    }
    {SsaAudioPoint point={256*65536,0,0};uint64_t notes,clock;uint32_t pc;
        ssa_audio_reset(&a);CHECK(ssa_audio_listen(&a,&ear)==1 && ssa_audio_play_at(&a,0x14,&point)==1);
        a.players[2].tracks[0].volume=127;CHECK(ssa_audio_render(&a,out,1)==1 && out[1]>out[0]*3);
        ear.right_x=-65536;CHECK(ssa_audio_listen(&a,&ear)==1);
        CHECK(ssa_audio_render(&a,out,1)==1 && out[1]>out[0]*3); /* No gain discontinuity. */
        mix_frames(&a,2400);CHECK(ssa_audio_render(&a,out,1)==1 && out[0]>out[1]*3);
        notes=a.notes;clock=a.players[2].clock;pc=a.players[2].tracks[0].pc;point.x=-256*65536;
        CHECK(ssa_audio_move(&a,0x14,&point)==1);
        CHECK(a.notes==notes && a.players[2].clock==clock && a.players[2].tracks[0].pc==pc);
        mix_frames(&a,2400);CHECK(ssa_audio_render(&a,out,1)==1 && out[1]>out[0]*3);
        CHECK(ssa_audio_move(&a,0x12,&point)==0 && ssa_audio_move(&a,SSA_AUDIO_SONGS,&point)==-1);
        CHECK(ssa_audio_stop(&a,0x14)==1 && ssa_audio_play(&a,0x14)==1 && !a.players[2].positioned);
        a.players[2].tracks[0].volume=127;mix_frames(&a,400);
        CHECK(ssa_audio_render(&a,out,1)==1 && out[0]==10368 && out[1]==10464);
        CHECK(ssa_audio_level_music(&a,0)==1);pc=a.players[0].tracks[0].pc;
        CHECK(ssa_audio_play_at(&a,0x73,&point)==-1 && a.players[0].tracks[0].pc==pc);
        CHECK(!a.players[0].positioned && a.players[0].left==1 && a.players[0].right==1);
        CHECK(ssa_audio_play_at(&a,0x14,NULL)==-1 && ssa_audio_listen(&a,NULL)==-1);
        ear.right_x=0;CHECK(ssa_audio_listen(&a,&ear)==-1);ear.right_x=65537;CHECK(ssa_audio_listen(&a,&ear)==-1);
    }
    {SsaAudioPoint point={INT32_MAX,INT32_MIN,INT32_MAX};SsaAudioListener limit={{INT32_MIN,INT32_MAX,INT32_MIN},65536,0};
        ssa_audio_reset(&a);CHECK(ssa_audio_listen(&a,&limit)==1 && ssa_audio_play_at(&a,0x14,&point)==1);
        a.players[2].tracks[0].volume=127;CHECK(ssa_audio_render(&a,out,1)==1 && out[0]>=0 && out[1]>out[0]);
        CHECK(a.players[2].left>0 && a.players[2].left<0.01 && a.players[2].right<0.01);
    }
    {SsaAudioPoint point={-256*65536,0,0};int16_t whole[16000],chunks[16000];size_t n;
        ssa_audio_reset(&a);ssa_audio_reset(&b);ear.right_x=65536;
        CHECK(ssa_audio_listen(&a,&ear)==1 && ssa_audio_listen(&b,&ear)==1);
        CHECK(ssa_audio_play_at(&a,0x14,&point)==1 && ssa_audio_play_at(&b,0x14,&point)==1);
        mix_frames(&a,400);mix_frames(&b,400);point.x=1024*65536;
        CHECK(ssa_audio_move(&a,0x14,&point)==1 && ssa_audio_move(&b,0x14,&point)==1);
        CHECK(ssa_audio_render(&a,whole,8000)==1);
        for(n=0;n<8000;){size_t count=8000-n;if(count>53)count=53;CHECK(ssa_audio_render(&b,chunks+2*n,count)==1);n+=count;}
        CHECK(!memcmp(whole,chunks,sizeof(whole)));
        ssa_audio_reset(&a);CHECK(!a.listener_ready && !a.players[2].positioned);
    }
    free(copy);
}
static void psg_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0};SsaAudioVoice *v;uint32_t h,tone,track;int16_t out[1500];unsigned i;
    static const uint8_t sequence[]={0xbd,20,0xcf,60,127,0xb0,0xce,60,0xb0,0xb1};
    static const uint8_t pulse[]={1,60,0,0,2,0,0,0,2,1,8,3};
    CHECK(copy);memcpy(copy,rom,0x800000);
    h=read32(copy+0x3e99d0+0x14*8)-0x08000000;
    tone=read32(copy+h+4)-0x08000000+20*12;track=read32(copy+h+8)-0x08000000;
    memcpy(copy+tone,pulse,sizeof(pulse));memcpy(copy+track,sequence,sizeof(sequence));
    CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_play(&a,0x14)==1);v=&a.voices[SSA_AUDIO_MUSIC_VOICES];
    CHECK(v->type==1 && v->stage==0 && v->envelope==0);
    CHECK(ssa_audio_render(&a,out,749)==1);
    for(i=0;i<1498;++i)CHECK(!out[i]);
    mix_frames(&a,1);CHECK(v->envelope_level==1 && v->stage==0);
    mix_frames(&a,10500);CHECK(v->envelope_level==15 && v->stage==1);
    mix_frames(&a,2625);CHECK(v->envelope_level==8 && v->stage==2);
    mix_frames(&a,24525);CHECK(v->envelope_level==8 && v->stage==3);
    mix_frames(&a,750);CHECK(v->envelope_level==8 && v->active);
    mix_frames(&a,225);CHECK(v->envelope_level==7 && v->active);
    mix_frames(&a,7875);CHECK(!v->active);
    /* Zero rates skip phases; a zero release must end exactly at EOT. */
    copy[tone+8]=0;copy[tone+9]=0;copy[tone+10]=15;copy[tone+11]=0;
    ssa_audio_reset(&a);CHECK(ssa_audio_play(&a,0x14)==1);
    CHECK(v->stage==2 && v->envelope==1);mix_frames(&a,38400);CHECK(!v->active);
    /*44.1kHz does not divide by64. Buffer splits must not round the PSG
     * period or move the30th envelope tick away from sample20672. */
    {SsaAudio b={0};int16_t *whole=malloc(40000*sizeof(*whole)),*chunks=malloc(40000*sizeof(*chunks));size_t n;
        CHECK(whole && chunks);memcpy(copy+tone,pulse,sizeof(pulse));memset(&a,0,sizeof(a));
        CHECK(ssa_audio_init(&a,copy,0x800000,44100)==1 && ssa_audio_init(&b,copy,0x800000,44100)==1);
        CHECK(ssa_audio_play(&a,0x14)==1 && ssa_audio_play(&b,0x14)==1);
        CHECK(ssa_audio_render(&a,whole,20000)==1);
        for(n=0;n<20000;){size_t count=20000-n;if(count>197)count=197;
            CHECK(ssa_audio_render(&b,chunks+2*n,count)==1);n+=count;}
        CHECK(!memcmp(whole,chunks,40000*sizeof(*whole)));
        CHECK(v->envelope_level==14 && v->stage==0);
        mix_frames(&a,671);CHECK(v->envelope_level==14);
        mix_frames(&a,1);CHECK(v->envelope_level==15 && v->stage==1);
        free(whole);free(chunks);
    }
    free(copy);
}
static void echo_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0},b={0};uint32_t h,tone,wave,track;unsigned type,i;
    int16_t whole[6400],chunks[6400];
    /* Change the track settings after note-on: the old voice must retain its
     * own volume/length. EOT is three ticks after note-on, at sample1200. */
    static const uint8_t sequence[]={0xcd,8,64,0xcd,9,3,0xbd,20,0xcf,60,127,0x83,
        0xcd,8,128,0xcd,9,8,0xce,60,0xb0,0xb1};
    CHECK(copy);memcpy(copy,rom,0x800000);
    h=read32(copy+0x3e99d0+0x14*8)-0x08000000;tone=read32(copy+h+4)-0x08000000+20*12;
    wave=read32(copy+tone+4)-0x08000000;track=read32(copy+h+8)-0x08000000;
    put32(copy+wave,0x40000000);put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,4);
    track=0x7ff000;put32(copy+h+8,0x08000000+track);
    memset(copy+wave+16,64,4);memcpy(copy+track,sequence,sizeof(sequence));
    for(type=0;type<=1;++type){SsaAudioVoice *v=&a.voices[SSA_AUDIO_MUSIC_VOICES];size_t n;
        copy[tone]=(uint8_t)type;copy[tone+8]=type?0:255;copy[tone+9]=0;copy[tone+10]=type?15:255;copy[tone+11]=0;
        memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
        CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_init(&b,copy,0x800000,24000)==1);
        CHECK(ssa_audio_play(&a,0x14)==1 && ssa_audio_play(&b,0x14)==1);
        CHECK(v->echo_volume==64 && v->echo_length==3 && v->active);
        mix_frames(&a,1200);CHECK(a.players[2].tracks[0].echo_volume==128 && a.players[2].tracks[0].echo_length==8);
        CHECK(v->echo_volume==64 && v->echo_length==3 && v->stage==(type?4u:3u));
        mix_frames(&a,1);CHECK(v->active && v->stage==4 && v->envelope_level==(type?4u:64u));
        CHECK(v->echo_length==(type?2u:3u));
        mix_frames(&a,type?799:1199);CHECK(v->active);mix_frames(&a,1);CHECK(!v->active);
        /* The entire tail and its cutoff must be invariant under buffer size. */
        ssa_audio_reset(&a);CHECK(ssa_audio_play(&a,0x14)==1 && ssa_audio_render(&a,whole,3200)==1);
        for(n=0;n<3200;){size_t count=3200-n;if(count>37)count=37;
            CHECK(ssa_audio_render(&b,chunks+2*n,count)==1);n+=count;}
        CHECK(!memcmp(whole,chunks,sizeof(whole)));
        /* Independent constant-PCM / pulse amplitudes at sample1201 prove
         * that the tail reaches the output, rather than only changing state. */
        CHECK(abs(abs(whole[2402])-(type?1536:2592))<=1 &&
              abs(abs(whole[2403])-(type?1536:2592))<=1);
        for(i=0;i<1600;++i)CHECK(!whole[4800+i]);
        CHECK(ssa_audio_play(&a,0x14)==1);mix_frames(&a,1201);CHECK(v->active && v->stage==4);
        CHECK(ssa_audio_stop(&a,0x14)==1 && !v->active);
        CHECK(ssa_audio_play(&a,0x14)==1);mix_frames(&a,1201);ssa_audio_reset(&a);CHECK(!v->active);
        CHECK(ssa_audio_play(&a,0x14)==1);a.muted=1;
        CHECK(ssa_audio_render(&a,whole,3200)==1 && !v->active);
        for(i=0;i<6400;++i)CHECK(!whole[i]);
    }
    /* A PSG release slope must reach the rounded echo goal, then hold for
     * sound frames; ADSR64Hz ticks must not keep decrementing the echo. */
    copy[tone+11]=1;memset(&a,0,sizeof(a));CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1);
    CHECK(ssa_audio_play(&a,0x14)==1);mix_frames(&a,6825);
    CHECK(a.voices[SSA_AUDIO_MUSIC_VOICES].stage==4 && a.voices[SSA_AUDIO_MUSIC_VOICES].envelope_level==4 && a.voices[SSA_AUDIO_MUSIC_VOICES].active);
    mix_frames(&a,776);CHECK(!a.voices[SSA_AUDIO_MUSIC_VOICES].active);
    free(copy);
}
static void wave_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0};int16_t out[16];uint32_t h,tone,wave,track;unsigned i;
    CHECK(copy);memcpy(copy,rom,0x800000);
    h=read32(copy+0x3e99d0+0x14*8)-0x08000000;
    tone=read32(copy+h+4)-0x08000000+20*12;wave=read32(copy+tone+4)-0x08000000;
    put32(copy+wave,0);put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,4);
    copy[wave+16]=128;copy[wave+17]=192;copy[wave+18]=0;copy[wave+19]=127;
    CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1);CHECK(ssa_audio_play(&a,0x14)==1);
    a.players[2].tracks[0].volume=127;
    CHECK(ssa_audio_render(&a,out,8)==1);
    /* Envelope255 ->223 at master13, note bytes L125/R126 ->108/109. */
    CHECK(out[0]==-20736 && out[2]==-15552 && out[4]==-10368 && abs(out[8])<=1);
    CHECK(out[1]==-20928 && out[3]==-15696 && out[5]==-10464);
    ssa_audio_reset(&a);
    CHECK(ssa_audio_play(&a,0x14)==1);a.players[2].tracks[0].volume=127;a.players[2].tracks[0].pan=0;
    CHECK(ssa_audio_render(&a,out,8)==1 && out[0]!=0);
    for(i=0;i<8;++i)CHECK(out[2*i+1]==0);
    copy[0x3e99a0+2*12+10]=1; /* PC instances bypass the old shared-player priority gate. */
    ssa_audio_reset(&a);CHECK(ssa_audio_play(&a,0x12)==1);
    CHECK(ssa_audio_play(&a,0xf)==1 && instances(&a,0x12)==1 && instances(&a,0xf)==1);
    CHECK(ssa_audio_stop(&a,0x12)==1 && ssa_audio_play(&a,0xf)==1);
    /* Rhythm instruments replace pitch with the instrument key. EOT must
     * still match the original sequence note, rather than the remapped pitch. */
    track=read32(copy+h+8)-0x08000000;put32(copy+h+4,0x083e8fdc);
    {static const uint8_t sequence[]={0xbd,127,0xcf,30,127,0x86,0xce,30,0x8c,0xb1};
        int16_t pcm[6800];unsigned voice=0;
        memcpy(copy+track,sequence,sizeof(sequence));ssa_audio_reset(&a);
        CHECK(ssa_audio_play(&a,0x14)==1);
        while(voice<SSA_AUDIO_VOICES && !a.voices[voice].active)++voice;
        CHECK(voice<SSA_AUDIO_VOICES && a.voices[voice].note==30);
        CHECK(ssa_audio_render(&a,pcm,3400)==1 && a.players[2].tracks[0].active);
        CHECK(!a.voices[voice].active || a.voices[voice].stage==3);
    }
    /* Private malformed stream: bounded command work rejects a zero-time loop. */
    track=read32(copy+h+8)-0x08000000;copy[track]=0xb2;put32(copy+track+1,0x08000000+track);
    CHECK(ssa_audio_play(&a,0x14)==-1 && a.errors==1);
    memset(&a,0,sizeof(a));put32(copy+0x3e99d0,0x02000000);
    CHECK(ssa_audio_init(&a,copy,0x800000,24000)==-1);
    free(copy);
}
static void play(void *p,unsigned id){++play_events;CHECK(ssa_audio_play(p,id)>=0);}
static void stop(void *p,unsigned id){++stop_events;CHECK(ssa_audio_stop(p,id)==1);}
static void game_events(SsaAudio *a,const uint8_t *rom){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaNativeAudioOps ops={play,stop,a,NULL,NULL,NULL,NULL,NULL};int16_t block[800];unsigned i;
    CHECK(g);CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(ssa_native_game_bind_audio(g,&ops)==1);ssa_audio_reset(a);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    g->players[0].inventory.weapons|=2;g->players[0].inventory.ammo[1]=99;
    CHECK(ssa_native_game_select_weapon(g,2)==1);
    ssa_pc_input_runtime_button(&g->input_runtime,1,1);
    for(i=0;i<120 && !g->weapon_game.shots[1];++i){CHECK(ssa_native_game_tick(g)==1);CHECK(ssa_audio_render(a,block,400)==1);}
    CHECK(i<120 && play_events && g->sound_serial==play_events);
    {SsaWeaponEvent event={0};event.kind=SSA_WEAPON_SOUND_STOP;event.args[0]=0x14;
        CHECK(g->weapon_ops.event(g->weapon_ops.context,&g->weapon,&event)==0 && stop_events==1);}
    CHECK(ssa_native_game_bind_audio(g,NULL)==1);
    {unsigned before=play_events;SsaWeaponEvent event={0};event.kind=SSA_WEAPON_SOUND_PLAY;event.args[0]=0x14;
        CHECK(g->weapon_ops.event(g->weapon_ops.context,&g->weapon,&event)==0 && event.result>0 && play_events==before);}
    ssa_native_game_free(g);free(g);
}
static unsigned jump_events,landing_events;
typedef struct WalkingAudio {SsaNativeGame *game;SsaAudio *mixer;unsigned steps[2],jumps,landings;} WalkingAudio;
static void walking_event(WalkingAudio *w,unsigned id){
    SsaNativeGame *g=w->game;const SsaBody *before=&g->audio_player_before,*body=&g->players[0].entity.body;
    if(id==14 && !g->audio_jump_held && g->player.jump.was_down && body->z>before->z){
        CHECK(g->players[0].inventory.health>0 && before->z==(int32_t)before->floor*65536 && body->z>(int32_t)body->floor*65536);
        ++w->jumps;++jump_events;return;
    }
    if(id==13 || id==14){SsaPlayerSpawn *player=&w->game->players[0];SsaBody *b=&player->entity.body;
        CHECK(b->z==(int32_t)b->floor*65536 && player->inventory.health>0);
        if(before->z>(int32_t)before->floor*65536){
            CHECK(id==13 && before->velocity_z<0);++w->landings;++landing_events;
        }else{
            CHECK(b->x!=w->game->footstep_x || b->y!=w->game->footstep_y);
            ++w->steps[id-13];++walking_events;
        }
    }
}
static void walking_play(void *p,unsigned id){WalkingAudio *w=p;walking_event(w,id);
    CHECK(ssa_audio_play(w->mixer,id)>=0);
}
static void walking_source(void *p,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point){
    WalkingAudio *w=p;
    if(command==SSA_AUDIO_SOURCE_PLAY && source==1){CHECK(!point);walking_event(w,id);}
    CHECK(ssa_audio_source(w->mixer,command,id,source,point)>=0);
}
static void walking_stop(void *p,unsigned id){WalkingAudio *w=p;CHECK(ssa_audio_stop(w->mixer,id)==1);}
static void walking_tick(WalkingAudio *w){
    SsaBody before=w->game->players[0].entity.body;unsigned steps=walking_events;
    CHECK(ssa_native_game_tick(w->game)==1);mix_frames(w->mixer,400);
    {SsaBody *b=&w->game->players[0].entity.body;
        if(b->z>(int32_t)b->floor*65536){++airborne_frames;CHECK(walking_events==steps);}
        if(b->x==before.x && b->y==before.y){++blocked_frames;CHECK(walking_events==steps);}
    }
    CHECK(w->mixer->players[0].active && w->mixer->players[0].song==0x73 && !w->mixer->errors);
}
static void footsteps(SsaAudio *a,const uint8_t *rom){
    static const int directions[]={'W','S','A','D'};unsigned mode,i;
    for(mode=0;mode<4;++mode){
        SsaNativeGame *g=calloc(1,sizeof(*g));WalkingAudio w={g,a,{0,0},0,0};SsaNativeAudioOps ops={walking_play,walking_stop,&w,NULL,NULL,NULL,NULL,NULL};unsigned before;
        if(mode&2)ops.source=walking_source;
        CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);ssa_audio_reset(a);
        CHECK(ssa_audio_level_music(a,0)==1 && ssa_native_game_bind_audio(g,&ops)==1);
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        for(i=0;i<60;++i)walking_tick(&w);
        CHECK(!w.steps[0] && !w.steps[1]);
        if(mode&1){CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);
            CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);}
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,directions[mode],1,0)==1);
        for(i=0;i<120;++i)walking_tick(&w);
        CHECK(w.steps[0] && w.steps[1]);
        CHECK(g->player.view.third_person==(mode&1));
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0)==1);
        for(i=0;i<60;++i)walking_tick(&w);
        CHECK(w.jumps==1 && w.landings==1); /* Held Space does not retrigger. */
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0)==1);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,directions[mode],0,0)==1);before=walking_events;
        for(i=0;i<40;++i)walking_tick(&w);
        CHECK(walking_events==before);
        /* No horizontal movement is required for takeoff/landing cues. A
         * fresh airborne press must neither play another takeoff cue nor queue it. */
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0)==1);walking_tick(&w);
        CHECK(w.jumps==2 && w.landings==1);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0)==1);walking_tick(&w);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0)==1);
        for(i=0;i<100;++i)walking_tick(&w);
        CHECK(w.jumps==2 && w.landings==2 && walking_events==before);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',0,0)==1);walking_tick(&w);
        ssa_pc_input_runtime_enable(&g->input_runtime,0);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0)==1);
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,directions[mode],1,0)==1);
        for(i=0;i<40;++i)walking_tick(&w);
        CHECK(walking_events==before && w.jumps==2 && w.landings==2);
        ssa_native_game_free(g);free(g);
    }
    CHECK(airborne_frames>40 && blocked_frames>120);
}
typedef struct SceneAudio {
    SsaAudio *mixer;SsaAudioPoint point;SsaAudioListener ear;
    unsigned local,world,moves,listeners,last_song,music_events,last_music;
} SceneAudio;
static void scene_play(void *p,unsigned id){SceneAudio *s=p;++s->local;s->last_song=id;CHECK(ssa_audio_play(s->mixer,id)>=0);}
static void scene_stop(void *p,unsigned id){SceneAudio *s=p;CHECK(ssa_audio_stop(s->mixer,id)==1);}
static void scene_world(void *p,unsigned id,const SsaAudioPoint *point){SceneAudio *s=p;
    ++s->world;s->last_song=id;s->point=*point;CHECK(ssa_audio_play_at(s->mixer,id,point)>=0);}
static void scene_listen(void *p,const SsaAudioListener *ear){SceneAudio *s=p;
    ++s->listeners;s->ear=*ear;CHECK(ssa_audio_listen(s->mixer,ear)==1);}
static void scene_move(void *p,unsigned id,const SsaAudioPoint *point){SceneAudio *s=p;
    ++s->moves;CHECK(ssa_audio_move(s->mixer,id,point)>=0);}
static void scene_music(void *p,unsigned command,unsigned level){SceneAudio *s=p;
    ++s->music_events;s->last_music=command;CHECK(ssa_audio_music(s->mixer,command,level)>=0);}
static void game_scene(SsaAudio *a,const uint8_t *rom){
    SsaNativeGame *g=calloc(1,sizeof(*g));SceneAudio scene={0};SsaNativeAudioOps ops={scene_play,scene_stop,&scene,scene_world,scene_listen,scene_move,scene_music,NULL};
    SsaEntity *source;SsaEnemyActivation activation={0};SsaEnemyControl control={0};SsaEnemyControlEvent event={0};unsigned before,slot;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);scene.mixer=a;ssa_audio_reset(a);
    CHECK(ssa_native_game_bind_audio(g,&ops)==1 && scene.listeners==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_native_game_tick(g)==1);
    {SsaAudioListener ear=scene.ear;
        CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1 && ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0)==1);
        CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,40)==1 && ssa_native_game_tick(g)==1);
        CHECK(g->player.view.third_person && g->player.pitch && !memcmp(&ear,&scene.ear,sizeof(ear)));
        CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,256,0)==1 && ssa_native_game_tick(g)==1);
        CHECK(scene.ear.right_x==ear.right_y && scene.ear.right_y==-ear.right_x);
    }
    /* Actual particle creation must emit a world-space impact from the
     * particle, rather than a centered sound at the player's weapon. */
    source=&g->players[0].entity;before=scene.world;
    CHECK(ssa_projectile_runtime_particle(&g->projectiles,0,source->body.x,source->body.y,source->body.z,&g->combat_ops)==0);
    CHECK(scene.world==before+1 && scene.last_song==0x22 && scene.point.x==source->body.x && scene.point.y==source->body.y);
    for(slot=0;slot<100 && !g->spawn.actors[slot].owned;++slot){}
    CHECK(slot<100);source=&g->spawn.actors[slot].entity;activation.entity=source;control.activation=&activation;
    event.kind=SSA_ENEMY_CONTROL_SOUND;event.args[0]=0x43;before=scene.world;
    CHECK(g->enemy_ops.presentation(g->enemy_ops.context,&control,&event)==0 && scene.world==before+1);
    CHECK(scene.point.x==source->body.x && scene.point.y==source->body.y && scene.point.z==source->body.z);
    {SsaEnemyCd cd={0};cd.activation=&activation;before=scene.world;
        CHECK(g->cd_ops.presentation(g->cd_ops.context,&cd,SSA_ENEMY_CD_SOUND,NULL,0x3a,0,0)==0 && scene.world==before+1);}
    before=scene.world;CHECK(g->script_ops.event(g->script_ops.context,&g->spawn,slot,SSA_ENEMY_ACTIVATION_SOUND,0x43,0,0,0)==0 && scene.world==before+1);
    {SsaWeaponEvent shot={0};shot.kind=SSA_WEAPON_SOUND_PLAY;shot.args[0]=0x14;before=scene.local;
        CHECK(g->weapon_ops.event(g->weapon_ops.context,&g->weapon,&shot)==0 && scene.local==before+1 && shot.result>0);
        CHECK(!a->players[latest(a,0x14)].positioned);}
    CHECK(ssa_native_game_bind_audio(g,NULL)==1);before=scene.listeners;
    CHECK(ssa_native_game_tick(g)==1 && scene.listeners==before);
    ssa_native_game_free(g);free(g);
}
static void music_transitions(SsaAudio *a,const uint8_t *rom){
    SsaNativeGame *g=calloc(1,sizeof(*g));SceneAudio scene={0};unsigned i;
    SsaNativeAudioOps ops={scene_play,scene_stop,&scene,scene_world,scene_listen,scene_move,scene_music,NULL};
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);scene.mixer=a;ssa_audio_reset(a);
    CHECK(ssa_audio_level_music(a,0)==1 && ssa_native_game_bind_audio(g,&ops)==1 && !scene.music_events);
    /* Game Over music belongs to the final life; ordinary deaths now retry. */
    g->game.life_game.continue_flag=g->gameplay.life_game.continue_flag=1;
    CHECK(ssa_combat_runtime_damage(&g->combat,&g->players[0].entity,&g->players[0].entity,&g->players[0].entity,1000,&g->combat_ops)==0);
    CHECK(!g->players[0].inventory.health && ssa_native_game_tick(g)==1);
    /* The final-life lifecycle requests Game Over during the fall/message
     * phase, before the eventual terminal handoff. Damage itself is silent. */
    CHECK(!scene.music_events && g->music_mode==2);
    for(i=0;i<640 && g->game.life_game.input_state!=4;++i){
        CHECK(scene.music_events<=1 && g->music_mode==(scene.music_events?10u:2u));
        CHECK(ssa_native_game_tick(g)==1);mix_frames(a,400);
    }
    CHECK(i>0 && i<640 && g->game.life_game.input_state==4);
    CHECK(ssa_native_game_tick(g)==1);
    CHECK(scene.music_events==1 && scene.last_music==10 && g->music_mode==10 && a->players[0].song==0x81);
    mix_frames(a,400);CHECK(a->players[0].song==0x81);
    for(i=0;i<320;++i){CHECK(ssa_native_game_tick(g)==1);mix_frames(a,400);}
    CHECK(g->game.life_game.input_state==4 && scene.music_events==1 && g->music_serial==1);
    /* Rebuild the real game composition, as the Windows restart does. */
    ssa_native_game_free(g);CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    ssa_audio_reset(a);scene.music_events=0;
    CHECK(ssa_audio_level_music(a,0)==1 && ssa_native_game_bind_audio(g,&ops)==1);
    CHECK(ssa_native_game_tick(g)==1 && a->players[0].song==0x73 && !g->music_serial && !scene.music_events);
    /* Fixture for terminal-state latching; the normal-input full-level test
     * separately reaches the real scripted exit without state grants. */
    g->game.life_game.input_state=3;CHECK(ssa_native_game_tick(g)==1 && g->finished);
    CHECK(scene.music_events==1 && scene.last_music==9 && g->music_serial==1 && a->music.pending_song==0x78);
    mix_frames(a,400);CHECK(a->players[0].song==0x78);
    for(i=0;i<30;++i){CHECK(ssa_native_game_tick(g)==1);mix_frames(a,400);}
    CHECK(scene.music_events==1 && g->music_serial==1 && a->players[0].song==0x78);
    CHECK(ssa_native_game_bind_audio(g,NULL)==1);ssa_audio_reset(a);CHECK(ssa_audio_level_music(a,0)==1);
    CHECK(ssa_native_game_bind_audio(g,&ops)==1 && scene.music_events==2 && a->music.pending_song==0x78);
    mix_frames(a,400);CHECK(a->players[0].song==0x78);
    CHECK(ssa_audio_play(a,0x14)==1 && ssa_audio_play(a,13)==1);
    CHECK(ssa_audio_music(a,21,0)==1);mix_frames(a,400);
    CHECK(!a->players[0].active && a->players[2].active && a->players[4].active);
    for(i=0;i<2;++i){unsigned voice,lane;
        ssa_audio_reset(a);a->music_enabled=i;
        CHECK(ssa_audio_play(a,0x12)==1 && ssa_audio_play(a,0x3a)==1 && ssa_audio_play(a,13)==1);
        CHECK(ssa_audio_music(a,9,0)==(int)i);
        for(lane=1;lane<SSA_AUDIO_PLAYERS;++lane)CHECK(!a->players[lane].active);
        for(voice=0;voice<SSA_AUDIO_VOICES;++voice)CHECK(!a->voices[voice].active || a->voices[voice].player==0);
    }
    ssa_audio_reset(a);a->music_enabled=0;
    CHECK(ssa_audio_level_music(a,0)==0 && ssa_audio_music(a,9,0)==0 && ssa_audio_music(a,10,0)==0 && !a->players[0].active);
    CHECK(ssa_audio_play(a,0x14)==1 && a->players[2].active);
    a->muted=1;{int16_t out[800];CHECK(ssa_audio_render(a,out,400)==1);for(i=0;i<800;++i)CHECK(out[i]==0);}
    a->muted=0;a->music_enabled=1;
    CHECK(ssa_audio_music(a,10,0)==1 && a->players[0].song==0x81);
    CHECK(ssa_audio_music(a,2,14)==0 && ssa_audio_music(a,2,15)==0 && ssa_audio_music(a,2,16)==-1);
    ssa_native_game_free(g);free(g);
}
static void music_policy_fixture(const uint8_t *rom){
    uint8_t *copy=malloc(0x800000);SsaAudio a={0},b={0};uint32_t h,tone,wave;unsigned i,which;
    int16_t whole[40000],chunks[40000];
    static const uint8_t effect[]={0xbd,20,0xcf,60,127,0xb0,0xb2,0x05,0xf0,0x7f,0x08};
    static const uint8_t short_song[]={0xbd,20,0xcf,60,127,0x81,0xce,60,0xb1};
    CHECK(copy);memcpy(copy,rom,0x800000);h=read32(copy+0x3e99d0+0x14*8)-0x08000000;
    tone=read32(copy+h+4)-0x08000000+20*12;wave=read32(copy+tone+4)-0x08000000;
    put32(copy+wave,0x40000000);put32(copy+wave+4,24000*1024);put32(copy+wave+8,0);put32(copy+wave+12,4);
    memset(copy+wave+16,64,4);copy[tone+8]=255;copy[tone+9]=0;copy[tone+10]=255;copy[tone+11]=0;
    put32(copy+h+8,0x087ff000);memcpy(copy+0x7ff000,effect,sizeof(effect));
    CHECK(ssa_audio_init(&a,copy,0x800000,24000)==1 && ssa_audio_init(&b,copy,0x800000,24000)==1);
    CHECK(ssa_audio_play(&a,0x14)==1 && ssa_audio_render(&a,whole,1)==1 && whole[0]==10368);
    CHECK(ssa_audio_fade(&a,0)==1);mix_frames(&a,399);
    CHECK(a.master_gain==1 && ssa_audio_render(&a,whole,1)==1 && whole[0]==9720);
    for(i=1;i<17;++i){double before=a.master_gain;mix_frames(&a,400);CHECK(a.master_gain<=before);}
    CHECK(a.master_gain==0 && a.players[2].active);
    CHECK(ssa_audio_render(&a,whole,400)==1);for(i=0;i<800;++i)CHECK(!whole[i]);
    CHECK(ssa_audio_fade(&a,256)==-1 && ssa_audio_fade(NULL,0)==-1 && ssa_audio_fade(&a,0)==0);
    /* Original rounded decrement65500>>4 leaves12 after16 updates; the
     *17th update reaches zero, and the next sound frame commits the song. */
    ssa_audio_reset(&a);CHECK(ssa_audio_level_music(&a,0)==1 && ssa_audio_fade(&a,1)==1);
    CHECK(ssa_audio_music(&a,7,0)==1 && ssa_audio_music(&a,8,0)==1 && a.music.pending_song==0x6f);
    for(i=0;i<34;++i){unsigned rate=a.music.rate;CHECK(ssa_audio_fade(&a,15)==0 && a.music.rate==rate);
        mix_frames(&a,400);CHECK(a.players[0].song==0x73);}
    CHECK(a.master_gain==0 && a.music.volume==0 && a.music.flags&8);
    CHECK(ssa_audio_render(&a,whole,1)==1 && a.players[0].song==0x6f && a.master_gain==1 && !(a.music.flags&12));
    /* Block boundaries cannot move the fade or queued replacement. */
    ssa_audio_reset(&a);ssa_audio_reset(&b);
    CHECK(ssa_audio_level_music(&a,0)==1 && ssa_audio_level_music(&b,0)==1);
    CHECK(ssa_audio_fade(&a,1)==1 && ssa_audio_fade(&b,1)==1);
    CHECK(ssa_audio_music(&a,8,0)==1 && ssa_audio_music(&b,8,0)==1);
    CHECK(ssa_audio_render(&a,whole,20000)==1);
    for(i=0;i<20000;){unsigned count=20000-i;if(count>197)count=197;
        CHECK(ssa_audio_render(&b,chunks+2*i,count)==1);i+=count;}
    CHECK(!memcmp(whole,chunks,sizeof(whole)) && a.players[0].song==0x6f && !a.errors && !b.errors);
    ssa_audio_reset(&a);CHECK(a.master_gain==1 && !a.music.flags && !a.music_command && a.music.pending_song==65535);
    CHECK(ssa_audio_level_music(&a,0)==1 && ssa_audio_fade(&a,0)==1 && ssa_audio_music(&a,8,0)==1);
    CHECK(ssa_audio_stop(&a,0x6f)==1 && !(a.music.flags&8));mix_frames(&a,8000);CHECK(a.players[0].song==0x73);
    ssa_audio_reset(&a);a.muted=1;CHECK(ssa_audio_level_music(&a,0)==1 && ssa_audio_fade(&a,0)==1 && ssa_audio_music(&a,8,0)==1);
    CHECK(ssa_audio_render(&a,whole,8000)==1 && a.players[0].song==0x6f);
    for(i=0;i<16000;++i)CHECK(!whole[i]);
    a.muted=0;
    /* Real command semantics on short fixture songs: intro1/post-boss20
     * return to the level, while terminal9/10 stay at their end screen. */
    for(which=0;which<4;++which){unsigned command[]={1,20,9,10},song[]={0x84,0x7f,0x78,0x81};
        uint32_t header=read32(copy+0x3e99d0+song[which]*8)-0x08000000;
        copy[header]=1;put32(copy+header+4,read32(copy+h+4));put32(copy+header+8,0x087ff100);
        memcpy(copy+0x7ff100,short_song,sizeof(short_song));
        ssa_audio_reset(&a);CHECK(ssa_audio_music(&a,command[which],0)==1 && a.players[0].song==song[which]);
        mix_frames(&a,1200);
        if(which<2)CHECK(a.players[0].song==0x73 && a.players[0].active && a.music_command==2);
        else CHECK(a.players[0].song==song[which] && !a.players[0].active && a.music_command==command[which]);
    }
    free(copy);
}
static void music_intros(SsaAudio *a){unsigned which;
    for(which=0;which<4;++which){unsigned level[]={0,6,0,5},command=which<2?1:20,tick;
        unsigned expected=a->rom[0x44a9dc+level[which]*2]|(unsigned)a->rom[0x44a9dd+level[which]*2]<<8;
        ssa_audio_reset(a);CHECK(ssa_audio_music(a,command,level[which])==1);
        for(tick=0;tick<18000 && a->music_command!=2;++tick){mix_frames(a,400);CHECK(!a->errors);}
        CHECK(tick>0 && tick<18000);
        mix_frames(a,800);CHECK(a->players[0].active && a->players[0].song==expected && a->music_command==2);++intro_returns;
    }
}
#include "audio_concurrency.inc"
#include "audio_world.inc"
#include "audio_priority.inc"
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);SsaAudio *a=calloc(1,sizeof(*a));FILE *f;
    CHECK(argc==2 && rom && a);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_audio_init(a,rom,0x800000,24000)==1);
    audio_priority_fixture(rom);world_distance_fixture(rom);player_door_fixture(rom);source_fixture(rom);world_fixture(rom);concurrency_fixture(rom);pcm_bus_fixture(rom);effects_bus_fixture(rom);effects_headroom_fixture(rom);channel_fixture(rom);music_policy_fixture(rom);music_intros(a);echo_fixture(rom);catalog(a);semantics(a,rom);wave_fixture(rom);psg_fixture(rom);spatial_fixture(rom);game_events(a,rom);footsteps(a,rom);game_scene(a,rom);music_transitions(a,rom);
    printf("{\"jump_events\":%u,\"landing_events\":%u,\"priority_cases\":%u,\"world_audio_cases\":%u,\"concurrency_cases\":%u,\"effect_instances\":%u,\"effect_voice_capacity\":%u,",jump_events,landing_events,priority_cases,world_audio_cases,concurrent_cases,SSA_AUDIO_EFFECT_PLAYERS,SSA_AUDIO_EFFECT_VOICES);
    printf("\"result\":\"pass\",\"checks\":%u,\"songs\":%u,\"audible_songs\":%u,\"catalog_frames\":%llu,\"catalog_peak\":%u,\"catalog_clipped_samples\":%llu,\"pcm_bus_gain\":2,\"effects_bus_gain\":1.5,\"music_bus_gain\":0.5,\"effects_scope_cases\":%u,\"max_pcm_voices\":%u,\"max_psg_voices\":%u,\"play_events\":%u,\"stop_events\":%u,\"walking_events\":%u,\"airborne_frames\":%u,\"blocked_or_idle_frames\":%u,\"intro_returns\":%u,\"decoder_errors\":%u}\n",
        checks,songs,audible,(unsigned long long)samples,catalog_peak,(unsigned long long)catalog_clipped,effects_scope_cases,max_pcm,max_psg,play_events,stop_events,walking_events,airborne_frames,blocked_frames,intro_returns,a->errors);
    free(a);free(rom);return 0;
}
