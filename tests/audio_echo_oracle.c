/* Test-only access to envelope/command internals, without a new game API. */
#include "../src/audio.c"
void ssa_test_pcm_envelope(unsigned *state){
    SsaAudio a={0};SsaAudioVoice *v=&a.voices[0];
    v->active=1;v->stage=state[0];v->envelope_level=state[1];
    v->attack=state[2];v->decay=state[3];v->sustain=state[4];v->release=state[5];
    v->echo_volume=state[6];v->echo_length=state[7];
    envelope_frame(&a);
    state[0]=v->stage;state[1]=v->envelope_level;state[7]=v->echo_length;state[8]=v->active;
}
int ssa_test_echo_command(unsigned extension,unsigned value,unsigned *out){
    uint8_t sequence[]={0xcd,0,0,0xb1};SsaAudio a={0};SsaAudioTrack *t=&a.players[0].tracks[0];int result;
    sequence[1]=(uint8_t)extension;sequence[2]=(uint8_t)value;
    a.rom=sequence;a.rom_size=sizeof(sequence);t->active=1;
    result=track_tick(&a,0,0);out[0]=t->echo_volume;out[1]=t->echo_length;out[2]=t->pc;
    return result;
}
