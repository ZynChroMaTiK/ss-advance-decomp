#ifndef SSA_SOUNDFONT_H
#define SSA_SOUNDFONT_H
#include "ssa/audio.h"
typedef struct SsaSoundFont SsaSoundFont;
/* Loads an external SF2, owns its sample data. Free after detaching the mixer.
 * All use is serialized with the mixer; loading itself can happen off-thread. */
SsaSoundFont *ssa_soundfont_load(const void *,size_t,unsigned sample_rate);
void ssa_soundfont_free(SsaSoundFont *);
SsaAudioSynth ssa_soundfont_synth(SsaSoundFont *);
#endif
