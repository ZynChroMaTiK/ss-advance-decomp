/* Private checks of the host renderer; never linked into the game. */
#include "../src/audio.c"
unsigned ssa_test_psg_register(const uint8_t *rom,unsigned key,unsigned fraction,unsigned rounded){
    SsaAudio a={0};a.rom=rom;return psg_register(&a,key+fraction/256.0,rounded);
}
unsigned ssa_test_wave_dac(const uint8_t *rom,unsigned level,unsigned nibble){
    SsaAudio a={0};a.rom=rom;return wave_dac(&a,level,nibble);
}
/* Exercise real note setup and output, including half-rate wave pitch and
 * source waveform nibbles. Also return the raw DAC levels for all32 steps. */
int ssa_test_wave_note(const uint8_t *rom,unsigned voice,unsigned key,unsigned velocity,unsigned volume,double *out){
    SsaAudio a={0};SsaAudioVoice *v;SsaAudioTrack *t;unsigned i,l,r;double left,right,before,value;
    if(ssa_audio_init(&a,rom,0x800000,24000)<0)return -1;
    a.players[0].priority=128;t=&a.players[0].tracks[0];t->bank=0x3e8fdc;
    t->voice=voice;t->key=key;t->velocity=velocity;t->volume=volume;t->pan=64;
    if(start_note(&a,0,0,96,0)!=1)return -1;
    v=&a.voices[0];if(v->type!=3)return -1;
    voice_volume(&a,v,0,&l,&r);before=v->position;value=sample(&a,v,&left,&right);
    out[0]=v->volume_goal;out[1]=v->envelope_level;out[2]=l;out[3]=r;
    out[4]=(v->position-before)*a.sample_rate;out[5]=value*left*128;out[6]=value*right*128;
    for(i=0;i<32;++i){unsigned raw=a.rom[v->wave+i/2];raw=i&1?raw&15:raw>>4;
        out[7+i]=wave_dac(&a,v->envelope_level,raw);}
    return 1;
}
