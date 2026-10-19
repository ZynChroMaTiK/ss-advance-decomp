#ifndef SSA_AUDIO_WIN32_H
#define SSA_AUDIO_WIN32_H
#include <windows.h>
#include <mmsystem.h>
#include "ssa/audio.h"
#include "ssa/soundfont.h"
#include "ssa/settings.h"
#include "midi_win32.h"
enum { SSA_AUDIO_BUFFERS=3,SSA_AUDIO_BLOCK=800 };
typedef struct SsaWin32Audio {
    SsaAudio mixer;HWAVEOUT device;
    HANDLE thread,stop_event,buffer_event;CRITICAL_SECTION mutex;unsigned synchronized;
    WAVEHDR headers[SSA_AUDIO_BUFFERS];
    int16_t pcm[SSA_AUDIO_BUFFERS][SSA_AUDIO_BLOCK*2];
    unsigned prepared[SSA_AUDIO_BUFFERS],queued[SSA_AUDIO_BUFFERS];
    unsigned last_error,peak;
    uint64_t submitted,recycled;
    SsaSoundFont *soundfont;
    wchar_t *soundfont_path;SsaWin32Midi *midi;unsigned music_style;
} SsaWin32Audio;
typedef struct SsaWin32AudioStatus {
    unsigned soundfont_available,midi_available,last_error,midi_error,music_active;
} SsaWin32AudioStatus;
/* UI-thread snapshot; all worker-owned fields are read under the mixer lock.
 * Open/close and backend replacement remain UI-thread lifecycle operations. */
SsaWin32AudioStatus ssa_win32_audio_status(SsaWin32Audio *);
int ssa_win32_audio_open(SsaWin32Audio *,const uint8_t *,size_t,unsigned level,unsigned mute,unsigned no_music);
int ssa_win32_audio_load_soundfont(SsaWin32Audio *,const wchar_t *);
int ssa_win32_audio_soundfont_available(const SsaWin32Audio *);
int ssa_win32_audio_settings(SsaWin32Audio *,const SsaSettings *);
int ssa_win32_audio_pump(SsaWin32Audio *);
int ssa_win32_audio_restart(SsaWin32Audio *,unsigned level);
int ssa_win32_audio_begin_level(SsaWin32Audio *,unsigned level);
int ssa_win32_audio_begin_story(SsaWin32Audio *,unsigned next_level);
int ssa_win32_audio_screen(SsaWin32Audio *,unsigned level,unsigned music_command);
void ssa_win32_audio_map_skip(SsaWin32Audio *);
void ssa_win32_audio_menu_command(SsaWin32Audio *,unsigned command,unsigned level,unsigned music_enabled);
void ssa_win32_audio_fade(SsaWin32Audio *,unsigned interval);
unsigned ssa_win32_audio_position(SsaWin32Audio *);
void ssa_win32_audio_close(SsaWin32Audio *);
void ssa_win32_audio_quiesce(SsaWin32Audio *);
void ssa_win32_audio_toggle_mute(SsaWin32Audio *);
void ssa_win32_audio_play(void *,unsigned song);
void ssa_win32_audio_stop(void *,unsigned song);
void ssa_win32_audio_play_at(void *,unsigned,const SsaAudioPoint *);
void ssa_win32_audio_listen(void *,const SsaAudioListener *);
void ssa_win32_audio_move(void *,unsigned,const SsaAudioPoint *);
void ssa_win32_audio_music(void *,unsigned command,unsigned level);
void ssa_win32_audio_source(void *,SsaAudioSourceCommand,unsigned,uint64_t,const SsaAudioPoint *);
#endif
