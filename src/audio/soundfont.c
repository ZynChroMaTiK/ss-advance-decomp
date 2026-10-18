#include "ssa/soundfont.h"
#include "ssa/music_instrument.h"
#include <stdlib.h>
#include <limits.h>
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "../../third_party/TinySoundFont/tsf.h"

struct SsaSoundFont {
    tsf *font;
    int presets[128],drums,keys[SSA_AUDIO_TRACKS];
    float buffer[64];unsigned cursor;
};
/* Authored GM arrangement for PSG instruments, NOT GM program identity.
 * Wave basses are transposed down an octave, matching their GBA pitch.
 * Program127 is the ROM's rhythm split, with keys24..39. Its original note
 * identity selects percussion, NOT the split entry's remapped sample pitch.
 * Its sampled entries (and every other resolved PCM instrument) stay native.
 * Unused/unknown programs use square lead rather than indexing past this map. */
static void reset(void *context){
    SsaSoundFont *s=context;int i;
    /* tsf_reset uses a short release and frees channels. At a scene reset we
     * need actual silence, and keep the preallocated channels for the worker. */
    for(i=0;i<s->font->voiceNum;++i)s->font->voices[i].playingPreset=-1;
    for(i=0;i<SSA_AUDIO_TRACKS;++i)s->keys[i]=-1;
    s->cursor=32;
}
static void control(void *context,unsigned track,const SsaAudioTrack *t){
    SsaSoundFont *s=context;float mod=0,volume=t->volume/127.0f,pan=t->pan/127.0f;
    if(t->mod_depth && !t->lfo_wait){int p=(int)t->lfo_phase;
        mod=(p<64?p:p<192?128-p:p-256)*(float)t->mod_depth/4096.0f;}
    if(t->mod_type==1)volume*=1+mod;
    if(t->mod_type==2)pan+=mod;
    if(volume<0)volume=0;
    if(pan<0)pan=0;
    if(pan>1)pan=1;
    tsf_channel_set_volume(s->font,(int)track,volume);
    tsf_channel_set_pan(s->font,(int)track,pan);
    tsf_channel_set_tuning(s->font,(int)track,t->bend*(float)t->bend_range/64.0f+t->tune/64.0f+(t->mod_type==0?mod:0));
}
static void release(void *context,unsigned track){
    SsaSoundFont *s=context;
    if(s->keys[track]>=0)tsf_channel_note_off(s->font,(int)track,s->keys[track]);
    s->keys[track]=-1;
}
static int note(void *context,unsigned track,const SsaAudioTrack *t,const SsaAudioVoice *v){
    SsaSoundFont *s=context;SsaMusicInstrument map;int key,preset;
    release(s,track);tsf_channel_sounds_off_all(s->font,(int)track);
    if(!v->type)return 1; /* Retain the actual ROM sample, including split drums. */
    map=ssa_music_instrument(t,v);key=map.key;
    preset=map.drum?s->drums:s->presets[map.program];
    tsf_channel_set_presetindex(s->font,(int)track,preset);
    control(s,track,t);s->keys[track]=key;
    return tsf_channel_note_on(s->font,(int)track,key,v->velocity/127.0f)?1:-1;
}
static void sample(void *context,float stereo[2]){
    SsaSoundFont *s=context;
    /* Bounded synthesis blocks: <=31 sample event latency (0.65ms at48kHz),
     * independent of device buffer boundaries. No game-frame dependency. */
    if(s->cursor==32){tsf_render_float(s->font,s->buffer,32,0);s->cursor=0;}
    stereo[0]=s->buffer[s->cursor*2];stereo[1]=s->buffer[s->cursor*2+1];++s->cursor;
}
SsaSoundFont *ssa_soundfont_load(const void *data,size_t size,unsigned rate){
    SsaSoundFont *s;unsigned i;
    if(!data || size<12 || size>INT_MAX || rate<8000 || rate>96000 ||
       memcmp(data,"RIFF",4) || memcmp((const char *)data+8,"sfbk",4))return NULL;
    s=calloc(1,sizeof(*s));if(!s)return NULL;
    s->font=tsf_load_memory(data,(int)size);
    if(!s->font || !tsf_get_presetcount(s->font))goto fail;
    /* Balance the replacement layer against retained ROM samples. The selected
     * SC-55 bank measured +11.83dB RMS over the PCM stem across the catalogue.
     * The user's listening adjustment lowers it another 3dB, for -15dB total.
     * Use a stable trim, preserving dynamics instead of chasing quiet drums.
     * This affects only the synth, not ROM PCM or Original-mode bus gains. */
    tsf_set_output(s->font,TSF_STEREO_INTERLEAVED,(int)rate,-15.0f);
    if(!tsf_set_max_voices(s->font,128))goto fail;
    for(i=0;i<128;++i){s->presets[i]=tsf_get_presetindex(s->font,0,(int)i);
        if(s->presets[i]<0)s->presets[i]=0;}
    s->drums=tsf_get_presetindex(s->font,128,0);
    if(s->drums<0)goto fail; /* GM drums are required by this arrangement. */
    for(i=0;i<SSA_AUDIO_TRACKS;++i)if(!tsf_channel_set_presetindex(s->font,(int)i,0))goto fail;
    reset(s);return s;
fail:ssa_soundfont_free(s);return NULL;
}
void ssa_soundfont_free(SsaSoundFont *s){if(s){if(s->font)tsf_close(s->font);free(s);}}
SsaAudioSynth ssa_soundfont_synth(SsaSoundFont *s){
    SsaAudioSynth ops={s,reset,note,release,control,sample,NULL};return ops;
}
