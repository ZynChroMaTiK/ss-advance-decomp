#ifndef SSA_MUSIC_INSTRUMENT_H
#define SSA_MUSIC_INSTRUMENT_H
#include "ssa/audio.h"
/* Shared authored GM arrangement; resolved PCM stays with the ROM renderer. */
typedef struct SsaMusicInstrument {unsigned program;int key,drum;} SsaMusicInstrument;
static inline SsaMusicInstrument ssa_music_instrument(const SsaAudioTrack *t,const SsaAudioVoice *v){
    static const unsigned drums[16]={42,46,38,36,40,35,50,45,49,50,45,40,35,76,51,50};
    SsaMusicInstrument m={80,(int)v->note-(v->type==3?12:0),0};
    switch(t->voice){
    case 1:case 5:case 7:m.program=38;break;
    case 18:case 32:m.program=39;break;
    case 15:m.program=81;break;
    case 16:case 17:case 35:m.program=62;break;
    default:break;
    }
    if(t->voice==127){m.drum=1;m.key=v->note>=24 && v->note<40?(int)drums[v->note-24]:38;}
    if(m.key<0)m.key=0;
    if(m.key>127)m.key=127;
    return m;
}
#endif
