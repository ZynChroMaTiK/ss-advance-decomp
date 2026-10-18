#ifndef SSA_MIDI_WIN32_H
#define SSA_MIDI_WIN32_H
#include <windows.h>
#include <mmsystem.h>
#include "ssa/audio.h"
enum {SSA_MIDI_DELAY_MS=185,SSA_MIDI_MAX_DELAY_MS=500};
typedef struct SsaWin32Midi {
    HMIDIOUT device;SsaAudio *mixer;
    int keys[SSA_AUDIO_TRACKS],channels[SSA_AUDIO_TRACKS];
    int volumes[16],pans[16],pitches[16],expression;
    unsigned error;uint64_t messages,notes,resets;
    wchar_t name[MAXPNAMELEN];
    double *music_delay;unsigned delay_frames,delay_cursor;
} SsaWin32Midi;
SsaWin32Midi *ssa_win32_midi_open(SsaAudio *);
void ssa_win32_midi_close(SsaWin32Midi *);
void ssa_win32_midi_volume(SsaWin32Midi *);
/* Set outside rendering, under the audio mutex. Clears buffered music. */
int ssa_win32_midi_set_delay(SsaWin32Midi *,unsigned milliseconds);
SsaAudioSynth ssa_win32_midi_synth(SsaWin32Midi *);
#endif
