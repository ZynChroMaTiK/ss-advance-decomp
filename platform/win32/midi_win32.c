#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "midi_win32.h"
#include "ssa/music_instrument.h"
#include <stdlib.h>
#include <string.h>

/* MIDI channel volume saturates at 127; balance the retained music separately
 * rather than continuing to raise an already saturated controller. */
#define MIDI_INSTRUMENT_VOLUME_GAIN 1.8
#define MIDI_PCM_MUSIC_GAIN 0.5
static void send(SsaWin32Midi *m,unsigned status,unsigned first,unsigned second){
    MMRESULT result=midiOutShortMsg(m->device,(DWORD)(status|(first<<8)|(second<<16)));
    ++m->messages;if(result!=MMSYSERR_NOERROR)m->error=result;
}
static int clamp(int n,int max){return n<0?0:n>max?max:n;}
int ssa_win32_midi_set_delay(SsaWin32Midi *m,unsigned milliseconds){
    if(!m || !m->music_delay || milliseconds>SSA_MIDI_MAX_DELAY_MS)return 0;
    m->delay_frames=m->mixer->sample_rate*milliseconds/1000;m->delay_cursor=0;
    memset(m->music_delay,0,(size_t)m->delay_frames*2*sizeof(double));return 1;
}
static void mix_music(void *context,double stereo[2]){
    SsaWin32Midi *m=context;unsigned at=m->delay_cursor*2;double left,right;
    /* Only retained music reaches this hook, before sound effects are mixed.
     * Apply the balance even when latency compensation is disabled. */
    stereo[0]*=MIDI_PCM_MUSIC_GAIN;stereo[1]*=MIDI_PCM_MUSIC_GAIN;
    left=stereo[0];right=stereo[1];
    if(!m->delay_frames)return;
    stereo[0]=m->music_delay[at];stereo[1]=m->music_delay[at+1];
    m->music_delay[at]=left;m->music_delay[at+1]=right;
    if(++m->delay_cursor==m->delay_frames)m->delay_cursor=0;
}
void ssa_win32_midi_volume(SsaWin32Midi *m){
    unsigned ch;int expression;
    if(!m)return;
    expression=m->mixer->muted || !m->mixer->music_enabled?0:
        clamp((int)(127*m->mixer->master_gain*m->mixer->music_volume/100.0+0.5),127);
    if(expression==m->expression)return;
    for(ch=0;ch<16;++ch)send(m,0xb0|ch,11,(unsigned)expression);
    m->expression=expression;
}
static void reset(void *context){
    SsaWin32Midi *m=context;unsigned ch;
    for(ch=0;ch<16;++ch){
        send(m,0xb0|ch,120,0);send(m,0xb0|ch,121,0);send(m,0xb0|ch,64,0);
        /* Per-channel pitch range24 semitones, then deselect the RPN. */
        send(m,0xb0|ch,101,0);send(m,0xb0|ch,100,0);
        send(m,0xb0|ch,6,24);send(m,0xb0|ch,38,0);
        send(m,0xb0|ch,101,127);send(m,0xb0|ch,100,127);
        /* Disable effects so reset/mute do not leave long reverb tails. */
        send(m,0xb0|ch,91,0);send(m,0xb0|ch,93,0);
        m->volumes[ch]=m->pans[ch]=m->pitches[ch]=-1;
    }
    for(ch=0;ch<SSA_AUDIO_TRACKS;++ch)m->keys[ch]=m->channels[ch]=-1;
    if(m->music_delay)memset(m->music_delay,0,(size_t)m->delay_frames*2*sizeof(double));
    m->delay_cursor=0;
    m->expression=-1;ssa_win32_midi_volume(m);++m->resets;
}
static void release(void *context,unsigned track){
    SsaWin32Midi *m=context;unsigned i;int key=m->keys[track],ch=m->channels[track];
    m->keys[track]=-1;
    if(key<0 || ch<0)return;
    /* Several rhythm tracks may share a GM percussion key. */
    if(ch==9)for(i=0;i<SSA_AUDIO_TRACKS;++i)if(m->channels[i]==9 && m->keys[i]==key)return;
    send(m,0x80|(unsigned)ch,(unsigned)key,0);
}
static void control(void *context,unsigned track,const SsaAudioTrack *t){
    SsaWin32Midi *m=context;int ch=m->channels[track],volume,pan,pitch;double mod=0,bend;
    if(ch<0)return;
    if(t->mod_depth && !t->lfo_wait){int p=(int)t->lfo_phase;
        mod=(p<64?p:p<192?128-p:p-256)*(double)t->mod_depth/4096.0;}
    volume=clamp((int)(t->volume*(t->mod_type==1?1+mod:1)*MIDI_INSTRUMENT_VOLUME_GAIN),127);
    pan=clamp((int)(t->pan+(t->mod_type==2?mod*127:0)),127);
    bend=t->bend*(double)t->bend_range/64.0+t->tune/64.0+(t->mod_type==0?mod:0);
    pitch=clamp((int)(8192+bend*8192/24),16383);
    if(volume!=m->volumes[ch]){send(m,0xb0|(unsigned)ch,7,(unsigned)volume);m->volumes[ch]=volume;}
    if(pan!=m->pans[ch]){send(m,0xb0|(unsigned)ch,10,(unsigned)pan);m->pans[ch]=pan;}
    if(pitch!=m->pitches[ch]){send(m,0xe0|(unsigned)ch,(unsigned)pitch&127,(unsigned)pitch>>7);m->pitches[ch]=pitch;}
}
static int note(void *context,unsigned track,const SsaAudioTrack *t,const SsaAudioVoice *v){
    SsaWin32Midi *m=context;SsaMusicInstrument map;int ch=m->channels[track];unsigned i,c;
    release(m,track);
    if(ch>=0 && ch!=9)send(m,0xb0|(unsigned)ch,120,0);
    if(!v->type){m->channels[track]=-1;return m->error?-1:1;}
    map=ssa_music_instrument(t,v);
    if(map.drum)ch=9;
    else if(ch<0 || ch==9){
        ch=-1;
        for(c=0;c<16;++c){if(c==9)continue;
            for(i=0;i<SSA_AUDIO_TRACKS;++i)if(i!=track && m->channels[i]==(int)c)break;
            if(i==SSA_AUDIO_TRACKS){ch=(int)c;break;}
        }
        if(ch<0){m->error=MMSYSERR_ALLOCATED;return -1;}
    }
    m->channels[track]=ch;
    send(m,0xc0|(unsigned)ch,map.drum?0:map.program,0);
    control(m,track,t);ssa_win32_midi_volume(m);
    if(v->velocity){m->keys[track]=map.key;send(m,0x90|(unsigned)ch,(unsigned)map.key,v->velocity);++m->notes;}
    return m->error?-1:1;
}
static void sample(void *context,float stereo[2]){
    /* Windows renders MIDI separately; keep the retained PCM in our mixer.
     * Check the sequencer's fade/mute here, sending only changed expression. */
    ssa_win32_midi_volume(context);stereo[0]=stereo[1]=0;
}
SsaWin32Midi *ssa_win32_midi_open(SsaAudio *mixer){
    UINT i,count=midiOutGetNumDevs();SsaWin32Midi *m;
    if(!mixer || mixer->sample_rate<8000 || mixer->sample_rate>96000)return NULL;
    for(i=0;i<count;++i){MIDIOUTCAPSW caps={0};
        if(midiOutGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR ||
           wcscmp(caps.szPname,L"Microsoft GS Wavetable Synth"))continue;
        m=calloc(1,sizeof(*m));if(!m)return NULL;
        if(midiOutOpen(&m->device,i,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){free(m);continue;}
        m->mixer=mixer;memcpy(m->name,caps.szPname,sizeof(m->name));
        m->music_delay=calloc((size_t)mixer->sample_rate*SSA_MIDI_MAX_DELAY_MS/1000*2,sizeof(double));
        if(!m->music_delay){midiOutClose(m->device);free(m);return NULL;}
        (void)ssa_win32_midi_set_delay(m,SSA_MIDI_DELAY_MS);reset(m);
        if(m->error){ssa_win32_midi_close(m);return NULL;}return m;
    }
    return NULL;
}
void ssa_win32_midi_close(SsaWin32Midi *m){
    if(!m)return;
    reset(m);midiOutReset(m->device);midiOutClose(m->device);free(m->music_delay);free(m);
}
SsaAudioSynth ssa_win32_midi_synth(SsaWin32Midi *m){
    SsaAudioSynth synth={m,reset,note,release,control,sample,mix_music};return synth;
}
