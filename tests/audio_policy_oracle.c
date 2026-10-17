/* Private policy access for ROM differential tests; never linked into game. */
#include "../src/audio.c"
void ssa_test_audio_policy(unsigned *state,unsigned operation,unsigned arg,int *action,unsigned *gain){
    SsaAudioMusicState m={(uint8_t)state[0],(uint8_t)state[1],(uint16_t)state[2],(uint16_t)state[3],(uint16_t)state[4]};
    *action=-1;*gain=UINT_MAX;
    if(operation==0){*action=music_frame(&m);if(!(m.flags&2))*gain=((m.flags&4?m.volume:m.base_volume)+255u)>>8;}
    else if(operation==1)music_request(&m,arg);
    else (void)music_fade(&m,arg);
    state[0]=m.flags;state[1]=m.rate;state[2]=m.volume;state[3]=m.base_volume;state[4]=m.pending_song;
}
unsigned ssa_test_audio_return(const uint8_t *rom,unsigned command,unsigned level,unsigned enabled,unsigned active){
    SsaAudio a={0};a.rom=rom;a.rom_size=0x800000;a.music.base_volume=0xffdc;
    a.music_command=command;a.music_level=level;a.music_enabled=enabled;a.players[0].active=active;
    music_update(&a);return a.music_command;
}
