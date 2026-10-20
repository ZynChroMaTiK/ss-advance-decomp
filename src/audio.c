#include "ssa/audio.h"
#include <limits.h>
#include <string.h>

/* ROM08C3EAC loads this song table;08FBC indexes the per-level music table.
 * MP2K command/wave layouts were checked against the ROM driver. This is a
 * host implementation, not a binary-compatible GBA hardware mixer. */
#define SONG_TABLE 0x3e99d0u
#define PLAYER_TABLE 0x3e99a0u
#define LEVEL_SONGS 0x44a9dcu
/* PC balance requested after listening: Original-mode sampled music and
 * sampled effects/footsteps receive +6.02dB at the output bus.
 * Hybrid SoundFont music bypasses this fixed adjustment.
 * Keep the recovered per-note arithmetic and PSG output levels unchanged. */
#define PCM_BUS_GAIN 2.0
/* Additional gameplay-effect balance, including PCM and PSG effects.
 * Songs1..6 are menu cues;6E..85 are music. Footsteps13/14 are effects. */
#define EFFECTS_BUS_GAIN 1.5
/* Original-mode soundtrack balance; applies to samples and all PSG voices.
 * Menu cues and gameplay effects retain their existing output levels. */
#define MUSIC_BUS_GAIN 0.5
/* Requested overall SoundFont-mode lift: +3dB after balancing synth and ROM
 * samples together. Keep their relative levels and Original mode unchanged. */
#define HYBRID_MUSIC_BUS_GAIN 1.4125375446227544
/* Raise both balanced music modes to full output peaks. The shared music
 * limiter below catches overloads without altering the instrument balance. */
#define ORIGINAL_MUSIC_OUTPUT_GAIN 3.0
#define HYBRID_MUSIC_OUTPUT_GAIN 2.0
static const unsigned duration[49]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
    28,30,32,36,40,42,44,48,52,54,56,60,64,66,68,72,76,78,80,84,88,90,92,96};
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int span(const SsaAudio *a,uint32_t p,size_t n){return p<=a->rom_size && n<=a->rom_size-p;}
static uint32_t offset(uint32_t p){return p>=0x08000000u && p<0x08800000u?p-0x08000000u:UINT32_MAX;}
static void music_request(SsaAudioMusicState *m,unsigned song){
    if(m->base_volume>>8){m->flags|=8;m->pending_song=(uint16_t)song;}
}
static int music_fade(SsaAudioMusicState *m,unsigned interval){
    if(m->flags&4)return 0;
    if(interval>15)interval=15;
    m->rate=(uint8_t)(interval*17);m->volume=m->base_volume;m->flags|=4;return 1;
}
/* One original sound-update frame, with player calls represented by a song
 * action. A queued song starts only after a pending fade reaches zero. */
static int music_frame(SsaAudioMusicState *m){int song=-1;unsigned volume;
    if((m->flags&10)==8 && (!(m->flags&4) || !m->volume)){
        song=m->pending_song;m->flags=(uint8_t)((m->flags&0xf3)|1);
    }
    if(m->flags&4){unsigned low=m->rate&15;
        if(low)m->rate=(uint8_t)((m->rate&0xf0)|(low-1));
        else{m->rate|=(uint8_t)(m->rate>>4);volume=m->base_volume>>4;
            m->volume=(uint16_t)(m->volume>volume?m->volume-volume:0);}
        m->flags|=1;
    }
    if(!(m->flags&2))m->flags&=0xfe;
    return song;
}
int ssa_audio_fade(SsaAudio *a,unsigned interval){
    if(!a || !a->loaded || interval>255)return -1;
    return music_fade(&a->music,interval);
}
static unsigned song_player(const SsaAudio *a,unsigned song){
    return song==13 || song==14?SSA_AUDIO_FOOTSTEP_PLAYER:u16(a->rom+SONG_TABLE+song*8+4);
}
static uint64_t root(uint64_t value){uint64_t result=0,bit=UINT64_C(1)<<62;
    while(bit>value)bit>>=2;
    while(bit){if(value>=result+bit){value-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}
    return result;
}
static void spatialize(SsaAudio *a,SsaAudioPlayer *p){
    int64_t dx,dy,dz;uint64_t horizontal,distance;double pan=0,gain,falloff;
    p->target_left=p->target_right=1;
    if(!p->positioned || !a->listener_ready)return;
    /*24.8 deltas keep the three squared full-int32 coordinate differences
     * inside64 bits, including opposite world bounds. No libm dependency. */
    dx=((int64_t)p->position.x-a->listener.position.x)/256;
    dy=((int64_t)p->position.y-a->listener.position.y)/256;
    dz=((int64_t)p->position.z-a->listener.position.z)/256;
    horizontal=(uint64_t)(dx*dx)+(uint64_t)(dy*dy);
    distance=root(horizontal+(uint64_t)(dz*dz));
    horizontal=root(horizontal);
    if(horizontal)pan=((double)dx*a->listener.right_x+(double)dy*a->listener.right_y)/(65536.0*horizontal);
    if(pan>1)pan=1;
    if(pan< -1)pan=-1;
    /* Unity at the source, half at256 world units for ordinary emitters
     * and1024 for enemies. Keep directional pan independent of falloff.
     * Local/UI/music playback remains unpositioned. */
    falloff=p->enemy_range?262144.0:65536.0;
    gain=falloff/(falloff+distance);
    p->target_left=gain*(pan>0?1-0.75*pan:1);
    p->target_right=gain*(pan<0?1+0.75*pan:1);
}
int ssa_audio_listen(SsaAudio *a,const SsaAudioListener *listener){unsigned i;
    if(!a || !a->loaded || !listener || listener->right_x< -65536 || listener->right_x>65536 ||
       listener->right_y< -65536 || listener->right_y>65536 || (!listener->right_x && !listener->right_y))return -1;
    a->listener=*listener;a->listener_ready=1;
    for(i=0;i<SSA_AUDIO_PLAYERS;++i)spatialize(a,&a->players[i]);
    return 1;
}
static int signed8(unsigned n){return n<128?(int)n:(int)n-256;}
static double ratio(const SsaAudio *a,double key){unsigned n;
    if(key<0)key=0;
    if(key>127)key=127;
    n=(unsigned)key;
    return n==127?a->ratios[n]:a->ratios[n]+(a->ratios[n+1]-a->ratios[n])*(key-n);
}
static void echo_voice(SsaAudioVoice *v){
    /* MP2K pseudo-echo holds the oscillator at a low envelope level. It is
     * not a delayed repeat. PCM has an8-bit envelope; PSG rounds its goal. */
    v->envelope_level=v->type?(v->volume_goal*v->echo_volume+255)/256:v->echo_volume;
    v->envelope=v->envelope_level/(v->type?15.0:255.0);v->stage=4;
    if(!v->envelope_level)v->active=0;
}
static void release_voice(SsaAudioVoice *v){
    v->released=1;
    if(v->stage>=3)return;
    v->stage=3;v->envelope_wait=v->release;
    if(v->type && !v->release)echo_voice(v);
}
static void stop_track(SsaAudio *a,unsigned player,unsigned track){unsigned i;
    if(!player){a->players[0].tracks[track].synth_on=0;
        if(a->synth.context)a->synth.release(a->synth.context,track);}
    a->players[player].tracks[track].active=0;
    for(i=0;i<SSA_AUDIO_VOICES;++i)if(a->voices[i].active && a->voices[i].player==player && a->voices[i].track==track)release_voice(&a->voices[i]);
}
static void stop_player(SsaAudio *a,unsigned player){unsigned i;
    if(!player && a->synth.context)a->synth.reset(a->synth.context);
    memset(&a->players[player],0,sizeof(a->players[player]));
    for(i=0;i<SSA_AUDIO_VOICES;++i)if(a->voices[i].player==player)a->voices[i].active=0;
}
void ssa_audio_reset(SsaAudio *a){
    if(!a)return;
    memset(a->players,0,sizeof(a->players));memset(a->voices,0,sizeof(a->voices));a->frame_samples=0;a->psg_clock=0;
    memset(&a->listener,0,sizeof(a->listener));a->listener_ready=0;
    a->music=(SsaAudioMusicState){0,0,0,0xffdc,0xffff};
    a->music_command=a->music_level=0;a->master_gain=1;a->effects_headroom_gain=a->world_headroom_gain=1;
    a->music_headroom_gain=1;
    /* A hardware synth reads these gains while resetting its controllers.
     * Restore the post-fade state first, so reset cannot reapply zero gain. */
    if(a->synth.context)a->synth.reset(a->synth.context);
}
int ssa_audio_init(SsaAudio *a,const uint8_t *rom,size_t size,unsigned rate){
    SsaAudio next={0};unsigned i;
    if(!a || a->loaded || !rom || size!=0x800000 || rate<8000 || rate>96000)return -1;
    next.rom=rom;next.rom_size=size;next.sample_rate=rate;next.music_enabled=1;
    next.music_volume=next.sound_volume=100;
    for(i=0;i<SSA_AUDIO_SONGS;++i){uint32_t h=offset(u32(rom+SONG_TABLE+i*8));
        if(!span(&next,h,4) || rom[h]>SSA_AUDIO_TRACKS || u16(rom+SONG_TABLE+i*8+4)>=SSA_AUDIO_ROM_PLAYERS)return -1;
        if(rom[h]){unsigned j;
            if(!span(&next,h,8+rom[h]*4) || !span(&next,offset(u32(rom+h+4)),12))return -1;
            for(j=0;j<rom[h];++j)if(!span(&next,offset(u32(rom+h+8+j*4)),1))return -1;
        }
    }
    for(i=0;i<14;++i)if(u16(rom+LEVEL_SONGS+i*2)>=SSA_AUDIO_SONGS)return -1;
    next.ratios[60]=1;
    for(i=61;i<128;++i)next.ratios[i]=next.ratios[i-1]*1.0594630943592953;
    for(i=60;i>0;--i)next.ratios[i-1]=next.ratios[i]/1.0594630943592953;
    next.loaded=1;ssa_audio_reset(&next);*a=next;return 1;
}
int ssa_audio_set_volume(SsaAudio *a,unsigned music,unsigned sounds){
    if(!a || !a->loaded || music>100 || sounds>100)return -1;
    a->music_volume=music;a->sound_volume=sounds;return 1;
}
int ssa_audio_set_synth(SsaAudio *a,const SsaAudioSynth *s){unsigned i;
    if(!a || !a->loaded || (s && (!s->context || !s->reset || !s->note || !s->release || !s->control || !s->sample)))return -1;
    if(a->synth.context)a->synth.reset(a->synth.context);
    if(s)a->synth=*s;else memset(&a->synth,0,sizeof(a->synth));
    a->music_headroom_gain=1;
    if(s){s->reset(s->context);
        for(i=0;i<a->players[0].count;++i)if(a->players[0].tracks[i].synth_on &&
            s->note(s->context,i,&a->players[0].tracks[i],&a->voices[i])!=1){
                s->reset(s->context);memset(&a->synth,0,sizeof(a->synth));return -1;}}
    return 1;
}
static int error(SsaAudio *a,unsigned p,unsigned cmd){
    ++a->errors;a->last_error_song=a->players[p].song;a->last_error_command=cmd;return -1;
}
/* The host has no shared GBA hardware channels. A new music note replaces
 * only its own track's old note, including release/echo. Effect oscillators
 * are independent even when they use the same PSG type or sequence track. */
static unsigned select_voice(SsaAudio *a,unsigned player,unsigned track){
    unsigned i,best=SSA_AUDIO_VOICES,priority=a->players[player].local_priority;
    if(!player)return track;
    for(i=SSA_AUDIO_MUSIC_VOICES;i<SSA_AUDIO_VOICES;++i){
        const SsaAudioVoice *v=&a->voices[i],*old;
        unsigned rank=a->players[v->player].local_priority,old_rank;
        if(!v->active)return i;
        if(rank>priority)continue;
        if(best==SSA_AUDIO_VOICES){best=i;continue;}
        old=&a->voices[best];old_rank=a->players[old->player].local_priority;
        if(rank<old_rank || (rank==old_rank && ((v->released && !old->released) ||
           (v->released==old->released && v->serial<old->serial))))best=i;
    }
    if(best==SSA_AUDIO_VOICES){++a->voice_drops;return best;}
    ++a->voice_recycles;return best;
}
static int player_busy(const SsaAudio *a,unsigned player){unsigned i;
    if(a->players[player].active || a->players[player].cue_repeats)return 1;
    for(i=SSA_AUDIO_MUSIC_VOICES;i<SSA_AUDIO_VOICES;++i)
        if(a->voices[i].active && a->voices[i].player==player)return 1;
    return 0;
}
static unsigned effect_player(SsaAudio *a,unsigned song,unsigned priority){
    unsigned i,best=0,preferred=song_player(a,song);
    /* Keep the ROM route as a free-slot hint, never as exclusive ownership. */
    if(preferred && !player_busy(a,preferred))return preferred;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i){
        if(!player_busy(a,i))return i;
        if(a->players[i].local_priority>priority)continue;
        if(!best || a->players[i].local_priority<a->players[best].local_priority ||
           (a->players[i].local_priority==a->players[best].local_priority &&
            a->players[i].serial<a->players[best].serial))best=i;
    }
    if(!best){++a->effect_drops;return 0;}
    ++a->effect_recycles;return best;
}
/* C47CC and C3AC4: track volume/pan, then note velocity/rhythm pan.
 * Keep the driver's integer truncations and distinct left/right bytes. */
static void voice_volume(const SsaAudio *a,const SsaAudioVoice *v,double mod,unsigned *left,unsigned *right){
    const SsaAudioTrack *t=&a->players[v->player].tracks[v->track];
    unsigned volume=t->volume*64/32,l,r;int pan=((int)t->pan-64)*2;
    if(t->mod_type==1){double scaled=volume*(1+mod);volume=scaled>65535?65535:scaled<0?0:(unsigned)scaled;}
    if(t->mod_type==2)pan+=(int)(mod*128);
    if(pan< -128)pan=-128;
    if(pan>127)pan=127;
    l=((unsigned)(pan+128)*volume/256)&255;r=((unsigned)(127-pan)*volume/256)&255;
    l=l*v->velocity*(unsigned)(128+v->instrument_pan)/16384;
    r=r*v->velocity*(unsigned)(127-v->instrument_pan)/16384;
    /* Driver byte2 is right/FIFO A; byte3 is left/FIFO B. */
    *right=l>255?255:l;*left=r>255?255:r;
}
static unsigned psg_goal(const SsaAudio *a,const SsaAudioVoice *v){unsigned l,r;
    voice_volume(a,v,0,&l,&r);return (l+r)/16>15?15:(l+r)/16;
}
static int floor_shift(int value,unsigned shift){
    int divisor=1<<shift;return value<0?-((-value+divisor-1)/divisor):value/divisor;
}
/* C4880 uses the same period register for pulses and the32-sample wave.
 * The wave oscillator completes a cycle at HALF the pulse frequency.
 * Bit8 instruments round the register to an even value at this ROM's
 * SOUNDBIAS resolution (40), as at C4D18..C4D4C. */
static unsigned psg_register(const SsaAudio *a,double key,unsigned rounded){
    unsigned note,fraction,encoded;int first,second,result;
    if(key<36)key=36;
    if(key>=167)key=166+255.0/256;
    note=(unsigned)key-36;fraction=(unsigned)((key-(unsigned)key)*256);
    encoded=a->rom[0x3e8e90u+note];first=(int)u16(a->rom+0x3e8f14u+(encoded&15)*2);
    if(first>=32768)first-=65536;
    first=floor_shift(first,encoded>>4);
    encoded=a->rom[0x3e8e90u+note+1];second=(int)u16(a->rom+0x3e8f14u+(encoded&15)*2);
    if(second>=32768)second-=65536;
    second=floor_shift(second,encoded>>4);
    result=2048+first+floor_shift((second-first)*(int)fraction,8);
    if(rounded)result=(result+1)&0x7fe;
    return (unsigned)result;
}
/* Apply the hardware DAC operation BEFORE centering. At25%, nibble15
 * becomes3, not3.75. Smooth gain overstates quieter wave instruments. */
static unsigned wave_dac(const SsaAudio *a,unsigned level,unsigned nibble){
    unsigned reg=a->rom[0x3e8f68u+(level>15?15:level)];
    if(reg&0x80)return nibble*3/4;
    switch(reg&0x60){case 0x20:return nibble;case 0x40:return nibble/2;case 0x60:return nibble/4;default:return 0;}
}
static int start_note(SsaAudio *a,unsigned player,unsigned track,unsigned gate,unsigned tied){
    SsaAudioTrack *t=&a->players[player].tracks[track];SsaAudioVoice v={0};
    uint32_t tone=t->bank+t->voice*12,wave;unsigned depth;int best;
    int key=(int)t->key+t->transpose;
    if(key<0)key=0;
    if(key>127)key=127;
    v.note=(unsigned)key;
    for(depth=0;depth<3;++depth){unsigned type,index;
        if(!span(a,tone,12))return -1;
        type=a->rom[tone];
        if(!(type&0xc0))break;
        index=(unsigned)key;
        if(type&0x40){uint32_t map=offset(u32(a->rom+tone+8));
            if(!span(a,map,128))return -1;
            index=a->rom[map+index];}
        tone=offset(u32(a->rom+tone+4));
        if(tone==UINT32_MAX || !span(a,tone,(size_t)(index+1)*12))return -1;
        tone+=index*12;
        if(type&0x80){key=a->rom[tone+1];
            if(a->rom[tone+3]&0x80)v.instrument_pan=((int)a->rom[tone+3]-0xc0)*2;}
    }
    if(depth==3 || !span(a,tone,12))return -1;
    v.type=a->rom[tone]&7;v.duty=u32(a->rom+tone+4)&3;
    v.psg_round_pitch=(a->rom[tone]&8)!=0;
    v.attack=a->rom[tone+8];v.decay=a->rom[tone+9];v.sustain=a->rom[tone+10];v.release=a->rom[tone+11];
    if(v.type==0){
        wave=offset(u32(a->rom+tone+4));if(!span(a,wave,16) || u16(a->rom+wave))return -1;
        v.length=u32(a->rom+wave+12);v.loop=u32(a->rom+wave+8);v.wave=wave+16;
        v.looped=(a->rom[wave+3]&0x40)!=0;
        if(!v.length || !span(a,v.wave,v.length) || (v.looped && v.loop>=v.length))return -1;
        v.step=(double)u32(a->rom+wave+4)/1024/a->sample_rate;
        if(a->rom[tone]&8)key=60;
        if(!v.step || v.step>256)return -1;
    }else if(v.type==3){v.wave=offset(u32(a->rom+tone+4));if(!span(a,v.wave,16))return -1;}
    else if(v.type!=1 && v.type!=2 && v.type!=4)return -1;
    v.priority=a->players[player].priority+t->priority;if(v.priority>255)v.priority=255;
    best=(int)select_voice(a,player,track);
    if(best==SSA_AUDIO_VOICES)return 1; /* Lower-priority world note; keep its sequence clock. */
    v.serial=player?a->notes+1:0;
    v.key=(unsigned)key;v.velocity=t->velocity;v.gate=gate;v.tied=tied;v.player=player;v.track=track;
    v.echo_volume=t->echo_volume;v.echo_length=t->echo_length;
    v.active=1;v.lfsr=0x7fff;v.envelope=v.type?1:0;v.stage=0;
    if(v.type){
        /* PSG rates count1/64-second steps; unlike PCM, zero attack/decay/
         * release skips that phase. Sustain is a4-bit level. */
        v.attack&=7;v.decay&=7;v.sustain&=15;v.release&=7;
        v.volume_goal=psg_goal(a,&v);v.envelope_wait=v.attack;
        if(!v.attack){v.envelope_level=v.volume_goal;v.stage=1;v.envelope_wait=v.decay;
            if(!v.decay){v.stage=2;v.envelope_level=(v.volume_goal*v.sustain+15)/16;}}
        v.envelope=v.envelope_level/15.0;
        if(v.stage==2 && !v.envelope_level)echo_voice(&v);
    }
    a->voices[best]=v;t->lfo_phase=0;t->lfo_wait=t->lfo_delay;++a->notes;
    if(!player){t->synth_on=1;t->synth_gate=gate;t->synth_tied=tied;t->synth_note=v.note;
        if(a->synth.context && a->synth.note(a->synth.context,track,t,&v)!=1)return -1;}
    return 1;
}
static int byte(SsaAudio *a,SsaAudioTrack *t,unsigned *value){
    if(!span(a,t->pc,1))return -1;
    *value=a->rom[t->pc++];return 1;
}
static int jump(SsaAudio *a,SsaAudioTrack *t,uint32_t *target){
    if(!span(a,t->pc,4))return -1;
    *target=offset(u32(a->rom+t->pc));t->pc+=4;return span(a,*target,1)?1:-1;
}
static int track_tick(SsaAudio *a,unsigned player,unsigned track){
    SsaAudioPlayer *p=&a->players[player];SsaAudioTrack *t=&p->tracks[track];unsigned work,i;
    if(!t->active)return 1;
    if(!player && t->synth_on && !t->synth_tied && t->synth_gate && !--t->synth_gate){
        t->synth_on=0;if(a->synth.context)a->synth.release(a->synth.context,track);}
    for(i=0;i<SSA_AUDIO_VOICES;++i){SsaAudioVoice *v=&a->voices[i];
        if(v->active && v->player==player && v->track==track && !v->tied && v->gate && !--v->gate)release_voice(v);
    }
    if(t->wait && --t->wait)return 1;
    for(work=0;work<2048;++work){unsigned cmd,arg;uint32_t target;
        if(byte(a,t,&cmd)<0)return error(a,player,0x100);
        if(cmd<0x80){if(!t->running)return error(a,player,cmd);--t->pc;cmd=t->running;}
        else if(cmd>=0xbd)t->running=cmd;
        if(cmd<=0xb0){t->wait=duration[cmd-0x80];if(t->wait)return 1;continue;}
        if(cmd>=0xcf){unsigned gate=duration[cmd-0xcf];
            if(span(a,t->pc,1) && a->rom[t->pc]<0x80)t->key=a->rom[t->pc++];
            if(span(a,t->pc,1) && a->rom[t->pc]<0x80)t->velocity=a->rom[t->pc++];
            if(span(a,t->pc,1) && a->rom[t->pc]<0x80)gate+=a->rom[t->pc++];
            if(start_note(a,player,track,gate,cmd==0xcf)<0)return error(a,player,cmd);
            continue;
        }
        if(cmd==0xb1){stop_track(a,player,track);return 1;}
        if(cmd==0xb2 || cmd==0xb3){
            if(jump(a,t,&target)<0 || (cmd==0xb3 && t->depth>=3))return error(a,player,cmd);
            /* U.M.T. pickup1D holds three PSG notes behind an endless GOTO.
             * The original shared effect player eventually replaces them.
             * Independent PC pickup instances must finish after their first
             * pass; only an explicit LOOP request owns a persistent repeat. */
            if(cmd==0xb2 && p->song==0x1d && !p->source_loop){
                stop_track(a,player,track);return 1;
            }
            if(cmd==0xb3)t->stack[t->depth++]=t->pc;else ++a->loops;
            t->pc=target;continue;
        }
        if(cmd==0xb4){if(t->depth)t->pc=t->stack[--t->depth];continue;}
        if(cmd==0xb5){
            if(byte(a,t,&arg)<0 || jump(a,t,&target)<0)return error(a,player,cmd);
            if(!arg || ++t->repeat<arg)t->pc=target;else t->repeat=0;
            continue;
        }
        if(cmd==0xce){arg=t->key;
            if(span(a,t->pc,1) && a->rom[t->pc]<0x80)arg=a->rom[t->pc++];
            if(!player && t->synth_on && t->synth_tied && t->synth_note==(unsigned)((int)arg+t->transpose)){
                t->synth_on=0;if(a->synth.context)a->synth.release(a->synth.context,track);}
            for(i=0;i<SSA_AUDIO_VOICES;++i){SsaAudioVoice *v=&a->voices[i];
                if(v->active && v->player==player && v->track==track && v->tied && v->note==(unsigned)((int)arg+t->transpose))release_voice(v);
            }continue;
        }
        if(byte(a,t,&arg)<0)return error(a,player,cmd);
        switch(cmd){
        case 0xba:t->priority=arg;break;
        case 0xbb:p->tempo=arg?arg:1;break;
        case 0xbc:t->transpose=signed8(arg);break;
        case 0xbd:t->voice=arg;break;
        case 0xbe:t->volume=arg;break;
        case 0xbf:t->pan=arg;break;
        case 0xc0:t->bend=(int)arg-64;break;
        case 0xc1:t->bend_range=arg;break;
        case 0xc2:t->lfo_speed=arg;break;
        case 0xc3:t->lfo_delay=arg;break;
        case 0xc4:t->mod_depth=arg;break;
        case 0xc5:t->mod_type=arg;break;
        case 0xc8:t->tune=(int)arg-64;break;
        case 0xcd:{unsigned extension=arg;if(extension!=8 && extension!=9)return error(a,player,cmd);
            if(byte(a,t,&arg)<0)return error(a,player,cmd);
            if(extension==8)t->echo_volume=arg;else t->echo_length=arg;
            break;}
        default:return error(a,player,cmd);
        }
    }
    return error(a,player,0x101); /* Malformed zero-time loop, never hang audio. */
}
static int play_instance(SsaAudio *a,unsigned song,const SsaAudioPoint *position,unsigned reuse){
    SsaAudioPlayer *p;uint32_t h,bank;unsigned player,i;
    if(!a || !a->loaded || song>=SSA_AUDIO_SONGS)return -1;
    h=offset(u32(a->rom+SONG_TABLE+song*8));if(!span(a,h,4))return -1;
    if(!a->rom[h])return 0;
    if(song>=0x6e && !a->music_enabled)return 0;
    player=reuse?reuse:song>=0x6e?0:effect_player(a,song,position==NULL);
    if(song<0x6e && !player)return 0;
    p=&a->players[player];
    stop_player(a,player);p->song=song;p->count=a->rom[h];p->priority=a->rom[h+2];p->tempo=75;p->active=1;
    p->local_priority=player && !position;
    p->left=p->right=p->target_left=p->target_right=1;
    ++a->starts;p->serial=player?a->starts:0;
    if(position){p->position=*position;p->positioned=1;spatialize(a,p);
        p->left=p->target_left;p->right=p->target_right;}
    bank=offset(u32(a->rom+h+4));
    for(i=0;i<p->count;++i){SsaAudioTrack *t=&p->tracks[i];
        t->pc=offset(u32(a->rom+h+8+i*4));t->bank=bank;t->active=1;
        t->volume=127;t->pan=64;t->velocity=127;t->key=60;t->bend_range=2;t->lfo_speed=22;
        if(track_tick(a,player,i)<0){stop_player(a,player);return -1;}
    }
    return 1;
}
int ssa_audio_play(SsaAudio *a,unsigned song){return play_instance(a,song,NULL,0);}
int ssa_audio_play_at(SsaAudio *a,unsigned song,const SsaAudioPoint *position){
    if(!position || song>=0x6e)return -1;
    return play_instance(a,song,position,0);
}
int ssa_audio_move(SsaAudio *a,unsigned song,const SsaAudioPoint *position){SsaAudioPlayer *p=NULL;unsigned i;
    if(!a || !a->loaded || song>=SSA_AUDIO_SONGS || !position)return -1;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i){SsaAudioPlayer *next=&a->players[i];
        if(next->song==song && next->positioned && player_busy(a,i) && (!p || next->serial>p->serial))p=next;}
    if(!p)return 0;
    p->position=*position;spatialize(a,p);return 1;
}
int ssa_audio_stop(SsaAudio *a,unsigned song){unsigned player;
    if(!a || !a->loaded || song>=SSA_AUDIO_SONGS)return -1;
    if(song>=0x6e && a->music.flags&8 && a->music.pending_song==song)a->music.flags&=0xf7;
    for(player=0;player<SSA_AUDIO_PLAYERS;++player)
        if(a->players[player].song==song){stop_player(a,player);if(!player)a->music_command=0;}
    return 1;
}
int ssa_audio_source(SsaAudio *a,SsaAudioSourceCommand command,unsigned song,
    uint64_t source,const SsaAudioPoint *position){unsigned i,count=0,enemy=0;int result;
    if(!a || !a->loaded || !source || (unsigned)command>SSA_AUDIO_SOURCE_ENEMY_LOOP)return -1;
    if(command==SSA_AUDIO_SOURCE_ENEMY_PLAY || command==SSA_AUDIO_SOURCE_ENEMY_LOOP){
        if(!position)return -1;
        enemy=1;command=command==SSA_AUDIO_SOURCE_ENEMY_LOOP?SSA_AUDIO_SOURCE_LOOP:SSA_AUDIO_SOURCE_PLAY;
    }
    if(command==SSA_AUDIO_SOURCE_PLAY || command==SSA_AUDIO_SOURCE_LOOP || command==SSA_AUDIO_SOURCE_CUE_TWICE){
        if(song>=0x6e)return -1;
        if(command==SSA_AUDIO_SOURCE_LOOP)for(i=1;i<SSA_AUDIO_PLAYERS;++i){SsaAudioPlayer *p=&a->players[i];
            if(p->source==source && p->song==song && p->source_loop && player_busy(a,i)){
                if(position){p->position=*position;p->positioned=1;}else p->positioned=0;
                p->enemy_range=enemy;spatialize(a,p);return 1;
            }
        }
        result=play_instance(a,song,position,0);if(result!=1)return result;
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].serial==a->starts){
            SsaAudioPlayer *p=&a->players[i];
            p->source=source;p->source_loop=command==SSA_AUDIO_SOURCE_LOOP;
            p->enemy_range=enemy;spatialize(a,p);p->left=p->target_left;p->right=p->target_right;
            p->cue_repeats=command==SSA_AUDIO_SOURCE_CUE_TWICE;return 1;}
        return -1;
    }
    if(command==SSA_AUDIO_SOURCE_MOVE && !position)return -1;
    if((command==SSA_AUDIO_SOURCE_STOP || command==SSA_AUDIO_SOURCE_CANCEL_REPEAT) && song>SSA_AUDIO_SONGS)return -1;
    for(i=1;i<SSA_AUDIO_PLAYERS;++i){SsaAudioPlayer *p=&a->players[i];
        if(p->source!=source)continue;
        if(command==SSA_AUDIO_SOURCE_MOVE){if(!p->positioned || !player_busy(a,i))continue;
            p->position=*position;spatialize(a,p);++count;
        }else if(command==SSA_AUDIO_SOURCE_RETIRE){
            p->cue_repeats=0;
            if(p->source_loop)stop_player(a,i);else p->source=0;
            ++count;
        }else if(song==SSA_AUDIO_SONGS || p->song==song){
            if(command==SSA_AUDIO_SOURCE_CANCEL_REPEAT)p->cue_repeats=0;else stop_player(a,i);
            ++count;}
    }
    return (int)count;
}
int ssa_audio_level_music(SsaAudio *a,unsigned level){
    return ssa_audio_music(a,2,level);
}
int ssa_audio_music_select(const uint8_t *rom,size_t size,unsigned command,unsigned level,
    unsigned enabled,unsigned *song){unsigned selected;
    if(!rom || size!=0x800000 || !song || command>65535 || level>=16 || enabled>255)return -1;
    if(command==21){*song=65535;return 1;}
    if(enabled!=1)return 0;
    switch(command){
    case 1:case 3:selected=level<=5 || level==12 || level==13?0x84:0x85;break;
    case 2:if(level>=14)return 0; /* Diagnostic maps have no campaign entry. */
        selected=u16(rom+LEVEL_SONGS+level*2);if(selected>=SSA_AUDIO_SONGS)return -1;break;
    case 7:selected=0x70;break;
    case 8:selected=0x6f;break;
    case 9:selected=level<=5 || level==12 || level==13?0x78:0x7f;break;
    case 10:selected=0x81;break;
    case 11:selected=level==0?0x71:level==6?0x6e:0x82;break;
    case 19:selected=0x83;break;
    case 20:selected=level==5?0x78:0x7f;break;
    default:return 0;
    }
    *song=selected;return 1;
}
int ssa_audio_music(SsaAudio *a,unsigned command,unsigned level){unsigned song,i;int result;
    if(!a || !a->loaded)return -1;
    if(command==4 || command==5){
        unsigned pause=command==4;
        for(i=0;i<SSA_AUDIO_PLAYERS;++i)a->players[i].paused=pause;
        if(pause)a->music.flags|=2;else a->music.flags&=(uint8_t)~2u;
        if(a->synth.context)for(i=0;i<a->players[0].count;++i){SsaAudioTrack track=a->players[0].tracks[i];
            if(pause)track.volume=0;
            a->synth.control(a->synth.context,i,&track);
        }
        return 1;
    }
    if(command==6){for(i=0;i<SSA_AUDIO_PLAYERS;++i)stop_player(a,i);return 1;}
    result=ssa_audio_music_select(a->rom,a->rom_size,command,level,a->music_enabled,&song);
    /* PC terminal screens freeze entity updates. Retire old effects here so
     * loops cannot outlive their controller, including in effects-only mode.
     * A death voice emitted later in the same tick still plays normally. */
    if(result>=0 && (command==9 || command==10))for(i=1;i<SSA_AUDIO_PLAYERS;++i)stop_player(a,i);
    if(result!=1)return result;
    a->music_command=command;a->music_level=level;music_request(&a->music,song);
    /* Initial/late-bound playback has no old music to fade. Start it now;
     * replacements are committed at the next sound-update frame. */
    if(!a->players[0].active && !(a->music.flags&6)){
        int action=music_frame(&a->music);
        if(action==65535){stop_player(a,0);return 1;}
        if(action>=0)return ssa_audio_play(a,(unsigned)action);
    }
    return 1;
}
static void music_update(SsaAudio *a){int action;
    if(a->players[0].paused)return;
    action=music_frame(&a->music);
    if(action==65535)stop_player(a,0);
    else if(action>=0 && ssa_audio_play(a,(unsigned)action)<0){
        (void)error(a,0,0x102);stop_player(a,0);
    }
    if(!(a->music.flags&2)){
        unsigned volume=a->music.flags&4?a->music.volume:a->music.base_volume;
        a->master_gain=((volume+255u)>>8)/256.0;
    }
    /* Original09120 returns intro1 and post-boss20 to the level song after
     * the music player finishes. End-screen9/10 intentionally do not return. */
    if((a->music_command==1 || a->music_command==20) && a->music_enabled==1 &&
       !a->players[0].active && !(a->music.flags&8)){
        unsigned song;
        if(ssa_audio_music_select(a->rom,a->rom_size,2,a->music_level,1,&song)==1){
            a->music_command=2;music_request(&a->music,song);
        }
    }
}
static void envelope_frame(SsaAudio *a){unsigned i,p,t;
    for(p=0;p<SSA_AUDIO_PLAYERS;++p)if(!a->players[p].paused)for(t=0;t<a->players[p].count;++t){SsaAudioTrack *track=&a->players[p].tracks[t];
        if(track->lfo_wait)--track->lfo_wait;else track->lfo_phase=(track->lfo_phase+track->lfo_speed)&255;
        if(!p && a->synth.context)a->synth.control(a->synth.context,t,track);
    }
    for(i=0;i<SSA_AUDIO_VOICES;++i){SsaAudioVoice *v=&a->voices[i];
        if(!v->active || a->players[v->player].paused)continue;
        /* Both tail lengths count sound frames, independently of tempo and
         * the64Hz PSG ADSR clock. A zero length ends on the next frame too. */
        if(v->stage==4){unsigned length=v->echo_length;v->echo_length=(length-1)&255;
            if(length<=1 || (v->type && signed8(v->echo_length)<=0))v->active=0;
            continue;
        }
        if(v->type)continue;
        switch(v->stage){
        case 0:v->envelope_level+=v->attack;
            if(v->envelope_level>=255){v->envelope_level=255;v->stage=1;}break;
        case 1:v->envelope_level=v->envelope_level*v->decay/256;
            if(v->envelope_level<=v->sustain){v->envelope_level=v->sustain;v->stage=2;
                if(!v->sustain)echo_voice(v);}break;
        case 3:v->envelope_level=v->envelope_level*v->release/256;
            if(v->envelope_level<=v->echo_volume)echo_voice(v);
            break;
        default:break;
        }
        v->envelope=v->envelope_level/255.0;
    }
}
static void psg_envelope_tick(SsaAudio *a){unsigned i;
    for(i=0;i<SSA_AUDIO_VOICES;++i){SsaAudioVoice *v=&a->voices[i];unsigned target;
        if(!v->active || a->players[v->player].paused || !v->type || v->stage==4)continue;
        v->volume_goal=psg_goal(a,v);
        target=(v->volume_goal*v->sustain+15)/16;
        if(v->stage==2){v->envelope_level=target;v->envelope=target/15.0;continue;}
        if(v->envelope_wait && --v->envelope_wait)continue;
        if(v->stage==0){
            if(++v->envelope_level>=v->volume_goal){v->envelope_level=v->volume_goal;v->stage=v->decay?1:2;
                if(!v->decay)v->envelope_level=target;}
        }else{
            if(v->envelope_level)--v->envelope_level;
            if(v->stage==1 && v->envelope_level<=target){v->envelope_level=target;v->stage=2;}
        }
        v->envelope_wait=v->stage==0?v->attack:v->stage==1?v->decay:v->release;
        v->envelope=v->envelope_level/15.0;
        if(v->stage!=0 && !v->envelope_level)echo_voice(v);
    }
}
static double sample(SsaAudio *a,SsaAudioVoice *v,double *left,double *right){
    SsaAudioTrack *t=&a->players[v->player].tracks[v->track];double value,key,mod=0,step;
    unsigned n,next,l,r;
    if(t->mod_depth && !t->lfo_wait){int phase=(int)t->lfo_phase;
        int triangle=phase<64?phase:phase<192?128-phase:phase-256;
        mod=(double)triangle*t->mod_depth/4096;
    }
    key=v->key+(double)t->bend*t->bend_range/64.0+t->tune/64.0+(t->mod_type==0?mod:0);
    voice_volume(a,v,mod,&l,&r);
    if(v->type==0){
        /* SoundMode master13: C3356 scales the8-bit envelope by14/16,
         * then multiplies each note-volume byte. FIFO output is signed8. */
        unsigned level=v->envelope_level*14/16;
        *left=(level*l/256)/256.0;*right=(level*r/256)/256.0;
        if(v->position>=v->length){
            if(!v->looped){v->active=0;return 0;}
            {double length=v->length-v->loop;double pos=v->position-v->loop;
             v->position=v->loop+pos-(unsigned)(pos/length)*length;}
        }
        n=(unsigned)v->position;next=n+1;
        if(next>=v->length)next=v->looped?v->loop:n;
        value=signed8(a->rom[v->wave+n]);value+=(signed8(a->rom[v->wave+next])-value)*(v->position-n);
        v->position+=v->step*ratio(a,key);return value;
    }
    /* SOUNDCNT_L=77, H=A90E: PSG100%, FIFO100%. In signed8 FIFO
     * units, a pulse/noise step spans four units per envelope level.
     * PSG pan uses channel routing, not the PCM's continuous stereo gain. */
    *left=r>=l*2?0:1;*right=l>=r*2?0:1;
    if(!l && !r)*left=*right=0;
    if(v->type==3){*left*=4;*right*=4;}
    else{*left*=v->envelope_level*2;*right*=v->envelope_level*2;}
    if(v->type==4)step=261.6255653005986*ratio(a,key)*8/a->sample_rate;
    else step=(v->type==3?65536.0:131072.0)/(2048-psg_register(a,key,v->psg_round_pitch))/a->sample_rate;
    if(step>0.95)step=0.95;
    v->position+=step;
    if(v->position>=1){v->position-=1;if(v->type==4){unsigned bit=(v->lfsr^(v->lfsr>>1))&1;
        v->lfsr=(v->lfsr>>1)|(bit<<14);if(v->duty&1)v->lfsr=(v->lfsr&~64u)|(bit<<6);}}
    if(v->type==3){n=(unsigned)(v->position*32);value=a->rom[v->wave+n/2];
        n=(n&1)?((unsigned)value&15):((unsigned)value>>4);
        return wave_dac(a,v->envelope_level,n)-wave_dac(a,v->envelope_level,15)/2.0;
    }
    if(v->type==4)return (v->lfsr&1)?1:-1;
    {static const double duty[4]={0.125,0.25,0.5,0.75};return v->position<duty[v->duty]?1:-1;}
}
/* Reserve headroom in order: music/menu, then local player effects, then world.
 * A crowded enemy bus cannot change the local bus's gain or recovery history. */
static void mix_effects(SsaAudio *a,double *left,double *right,double el,double er,double *gain){
    double target=1,needed;
    if(el>0 && *left+el>32760){needed=(32760-*left)/el;if(needed<target)target=needed;}
    if(el<0 && *left+el< -32760){needed=(-32760-*left)/el;if(needed<target)target=needed;}
    if(er>0 && *right+er>32760){needed=(32760-*right)/er;if(needed<target)target=needed;}
    if(er<0 && *right+er< -32760){needed=(-32760-*right)/er;if(needed<target)target=needed;}
    if(target<0)target=0;
    if(target<*gain)*gain=target;
    else *gain+=(target-*gain)*(10.0/a->sample_rate);
    if(target==1 && *gain>0.999999)*gain=1;
    *left+=el* *gain;*right+=er* *gain;
}
int ssa_audio_render(SsaAudio *a,int16_t *out,size_t frames){size_t n;unsigned p,i;
    if(!a || !a->loaded || (!out && frames) || frames>SIZE_MAX/2)return -1;
    for(n=0;n<frames;++n){double left=0,right=0,effects_left=0,effects_right=0,world_left=0,world_right=0;
        double music_left=0,music_right=0;
        if(!a->frame_samples){music_update(a);envelope_frame(a);}
        if(++a->frame_samples>=a->sample_rate/60)a->frame_samples=0;
        a->psg_clock+=64;
        if(a->psg_clock>=a->sample_rate){a->psg_clock-=a->sample_rate;psg_envelope_tick(a);}
        for(p=0;p<SSA_AUDIO_PLAYERS;++p){SsaAudioPlayer *player=&a->players[p];unsigned active=0;
            if(player->cue_repeats && !player->paused){unsigned voices=0;
                if(player->cue_started!=2){
                    for(i=SSA_AUDIO_MUSIC_VOICES;i<SSA_AUDIO_VOICES;++i)
                        voices|=a->voices[i].active && a->voices[i].player==p;
                    if(voices){player->cue_started=1;++player->cue_frames;}
                    else if(player->cue_started){player->cue_started=2;player->cue_gap=(player->cue_frames+1)/2;}
                    else if(!player->active)player->cue_repeats=0;
                }
                if(player->cue_started==2 && player->cue_gap)--player->cue_gap;
                else if(player->cue_started==2){unsigned song=player->song,repeats=player->cue_repeats-1;
                    uint64_t source=player->source;SsaAudioPoint position=player->position;
                    unsigned positioned=player->positioned;
                    if(play_instance(a,song,positioned?&position:NULL,p)<0)return -1;
                    player->source=source;player->cue_repeats=repeats;
                }
            }
            player->left+=(player->target_left-player->left)*(100.0/a->sample_rate);
            player->right+=(player->target_right-player->right)*(100.0/a->sample_rate);
            if(!player->active || player->paused)continue;
            player->clock+=(uint64_t)player->tempo*48;
            while(player->clock>=(uint64_t)a->sample_rate*60){
                player->clock-=(uint64_t)a->sample_rate*60;
                for(i=0;i<player->count;++i)if(track_tick(a,p,i)<0){stop_player(a,p);break;}
            }
            for(i=0;i<player->count;++i)active|=player->tracks[i].active;
            if(!active)player->active=0;
        }
        for(i=0;i<SSA_AUDIO_VOICES;++i)if(a->voices[i].active && !a->players[a->voices[i].player].paused){double l,r,value=sample(a,&a->voices[i],&l,&r)*128;
            SsaAudioPlayer *player=&a->players[a->voices[i].player];
            if(player->song>=0x6e){
                if(a->synth.context){
                    /* Hybrid music: retain resolved ROM PCM instruments at
                     * their native level; replace only PSG. Original-mode
                     * PCM boost and soundtrack attenuation do not apply. */
                    if(a->voices[i].type)value=0;
                }else{
                    if(!a->voices[i].type)value*=PCM_BUS_GAIN;
                    value*=MUSIC_BUS_GAIN;
                }
                value*=a->music_volume/100.0;
            }else{
                if(!a->voices[i].type)value*=PCM_BUS_GAIN;
                value*=a->sound_volume/100.0;
            }
            if(player->song>=7 && player->song<0x6e){value*=EFFECTS_BUS_GAIN;
                if(player->local_priority){effects_left+=value*l*player->left;effects_right+=value*r*player->right;}
                else{world_left+=value*l*player->left;world_right+=value*r*player->right;}
            }else if(player->song>=0x6e){
                music_left+=value*l*(a->synth.context?1:player->left);
                music_right+=value*r*(a->synth.context?1:player->right);}
            else{left+=value*l*player->left;right+=value*r*player->right;}
        }
        if(a->synth.context && !a->players[0].paused){float stereo[2];double gain=32768*a->music_volume/100.0;
            a->synth.sample(a->synth.context,stereo);music_left+=stereo[0]*gain;music_right+=stereo[1]*gain;}
        left*=a->master_gain;right*=a->master_gain;
        {
            /* Both modes share stereo-linked overload protection after their
             * individual balance, master fade and requested output lift. */
            double gain=a->master_gain*(a->synth.context?
                HYBRID_MUSIC_OUTPUT_GAIN*HYBRID_MUSIC_BUS_GAIN:ORIGINAL_MUSIC_OUTPUT_GAIN);
            double stereo[2]={music_left*gain,music_right*gain};
            if(a->synth.context && a->synth.mix_music)a->synth.mix_music(a->synth.context,stereo);
            mix_effects(a,&left,&right,stereo[0],stereo[1],&a->music_headroom_gain);
            if(a->music_headroom_gain<1)++a->music_limited_frames;
        }
        effects_left*=a->master_gain;effects_right*=a->master_gain;
        world_left*=a->master_gain;world_right*=a->master_gain;
        mix_effects(a,&left,&right,effects_left,effects_right,&a->effects_headroom_gain);
        mix_effects(a,&left,&right,world_left,world_right,&a->world_headroom_gain);
        if(a->effects_headroom_gain<1 || a->world_headroom_gain<1)++a->effects_limited_frames;
        if(left>32767){left=32767;++a->clipped;}if(left< -32768){left=-32768;++a->clipped;}
        if(right>32767){right=32767;++a->clipped;}if(right< -32768){right=-32768;++a->clipped;}
        out[2*n]=a->muted?0:(int16_t)left;out[2*n+1]=a->muted?0:(int16_t)right;
    }
    a->frames+=frames;return 1;
}
