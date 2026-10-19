#ifndef SSA_AUDIO_H
#define SSA_AUDIO_H
#include <stddef.h>
#include <stdint.h>

enum { SSA_AUDIO_SONGS=136,SSA_AUDIO_ROM_PLAYERS=4,SSA_AUDIO_FOOTSTEP_PLAYER=4,
    SSA_AUDIO_EFFECT_PLAYERS=32,SSA_AUDIO_PLAYERS=1+SSA_AUDIO_EFFECT_PLAYERS,
    SSA_AUDIO_TRACKS=16,SSA_AUDIO_MUSIC_VOICES=SSA_AUDIO_TRACKS,
    SSA_AUDIO_EFFECT_VOICES=64,SSA_AUDIO_VOICES=SSA_AUDIO_MUSIC_VOICES+SSA_AUDIO_EFFECT_VOICES };
/* PC spatial extension. Coordinates use16.16; right is a unit XY vector in
 *16.16. Values are copied, never borrowed from gameplay or the audio worker. */
typedef struct SsaAudioPoint {int32_t x,y,z;} SsaAudioPoint;
typedef struct SsaAudioListener {SsaAudioPoint position;int32_t right_x,right_y;} SsaAudioListener;
typedef enum SsaAudioSourceCommand {
    SSA_AUDIO_SOURCE_PLAY,SSA_AUDIO_SOURCE_LOOP,SSA_AUDIO_SOURCE_MOVE,
    SSA_AUDIO_SOURCE_STOP,SSA_AUDIO_SOURCE_RETIRE,
    SSA_AUDIO_SOURCE_CUE_TWICE,SSA_AUDIO_SOURCE_CANCEL_REPEAT
} SsaAudioSourceCommand;
typedef struct SsaAudioTrack {
    uint32_t pc,bank,stack[3];
    unsigned wait,depth,repeat,active,running,voice,key,velocity;
    unsigned volume,pan,bend_range,lfo_speed,lfo_delay,mod_depth,mod_type,lfo_phase,lfo_wait;
    unsigned echo_volume,echo_length,priority;
    int transpose,bend,tune;
    unsigned synth_gate,synth_tied,synth_note,synth_on;
} SsaAudioTrack;
typedef struct SsaAudioPlayer {
    SsaAudioTrack tracks[SSA_AUDIO_TRACKS];
    unsigned song,active,priority,count,tempo;
    uint64_t clock,serial,source;
    SsaAudioPoint position;
    unsigned positioned,source_loop,local_priority,paused;
    unsigned cue_repeats,cue_started;
    uint64_t cue_frames,cue_gap;
    double left,right,target_left,target_right;
} SsaAudioPlayer;
typedef struct SsaAudioVoice {
    uint64_t serial;
    uint32_t wave,length,loop,lfsr;
    unsigned active,player,track,type,key,note,velocity,gate,tied,looped,priority;
    unsigned attack,decay,sustain,release,stage,duty,psg_round_pitch;
    unsigned envelope_level,envelope_wait,volume_goal;
    unsigned echo_volume,echo_length; /* Copied at note start; stage4 holds the tail. */
    unsigned released; /* Release ownership survives entry into the echo tail. */
    int instrument_pan;
    double position,step,envelope;
} SsaAudioVoice;
/* Optional music renderer. Borrowed context, called only on the mixer thread.
 * Sequence timing stays in audio.c. Resolved PCM music voices and all effects
 * keep the ROM renderer. note receives PCM too, to retire the track's previous
 * synth note, but must not synthesize it. Only PSG music is replaced. */
typedef struct SsaAudioSynth {
    void *context;
    void (*reset)(void *);
    int (*note)(void *,unsigned,const SsaAudioTrack *,const SsaAudioVoice *);
    void (*release)(void *,unsigned);
    void (*control)(void *,unsigned,const SsaAudioTrack *);
    void (*sample)(void *,float stereo[2]);
    /* Optional music-only output processing before peak protection/effects.
     * A hardware MIDI renderer uses this to align retained PCM with its sink. */
    void (*mix_music)(void *,double stereo[2]);
} SsaAudioSynth;
/* Recovered08C04/08D30/08E74 policy. Volume uses the original16-bit scale. */
typedef struct SsaAudioMusicState {
    uint8_t flags,rate;
    uint16_t volume,base_volume,pending_song;
} SsaAudioMusicState;
/* Native ROM-data sequencer/synthesizer. No GBA instructions are executed.
 * Single-threaded: play/stop/render on the same thread. ROM remains borrowed
 * and immutable until the audio object and output device are closed. */
typedef struct SsaAudio {
    const uint8_t *rom;size_t rom_size;
    unsigned sample_rate,loaded,muted,music_enabled,frame_samples,psg_clock;
    SsaAudioListener listener;unsigned listener_ready;
    SsaAudioPlayer players[SSA_AUDIO_PLAYERS];
    SsaAudioVoice voices[SSA_AUDIO_VOICES];
    SsaAudioMusicState music;
    unsigned music_command,music_level;
    double master_gain,effects_headroom_gain,world_headroom_gain,music_headroom_gain;
    double ratios[128];
    unsigned music_volume,sound_volume; /* PC bus levels, 0..100. */
    SsaAudioSynth synth;
    uint64_t frames,notes,loops,clipped,effects_limited_frames;
    uint64_t music_limited_frames;
    uint64_t starts,effect_recycles,voice_recycles,effect_drops,voice_drops;
    unsigned errors,last_error_song,last_error_command;
} SsaAudio;
int ssa_audio_init(SsaAudio *,const uint8_t *,size_t,unsigned sample_rate);
void ssa_audio_reset(SsaAudio *);
int ssa_audio_set_synth(SsaAudio *,const SsaAudioSynth *); /* NULL: original */
int ssa_audio_set_volume(SsaAudio *,unsigned music,unsigned sounds);
/* PC concurrency: music owns player0 and one voice per sequence track.
 * Each effect/menu cue gets an independent instance, including repeat sounds
 * and footsteps. Effects never borrow music voices. At capacity only the
 * world effects yield to local centered player effects at both instance and
 * voice capacity. Enemies cannot steal a local sound or its release tail.
 * Equal-class sounds recycle released/old voices and old instances; incoming
 * world sounds are dropped if every slot is local. Music is always separate.
 * 1 started,0 disabled/empty or lower-priority effect dropped,-1 invalid data. */
int ssa_audio_play(SsaAudio *,unsigned song);
/* One-shot world position. Music is rejected. Ordinary play resets spatial
 * gain to unity. Move updates the newest matching positional instance without
 * restarting it. Stop(song) retires every instance of that sound ID.
 * Listener/move gain changes settle smoothly (10ms time constant). */
int ssa_audio_play_at(SsaAudio *,unsigned song,const SsaAudioPoint *);
int ssa_audio_move(SsaAudio *,unsigned song,const SsaAudioPoint *);
int ssa_audio_listen(SsaAudio *,const SsaAudioListener *);
int ssa_audio_stop(SsaAudio *,unsigned song);
/* Copied numeric emitter identity; never an entity pointer. PLAY overlaps;
 * LOOP ensures one instance per(source,song), updating it without restarting.
 * MOVE affects every positional instance of source. STOP affects only that
 * source/song (SSA_AUDIO_SONGS means all its sounds). RETIRE stops its loops
 * and detaches one-shots at their last position, allowing death/impact tails.
 * PLAY/LOOP accept NULL position for local centered effects. Source must be
 * nonzero. CUE_TWICE plays a single-burst UI effect twice, with a silent gap
 * half the first burst's duration (rounded up to an output frame), discarding
 * trailing sequence rests (not for music or
 * multi-burst effects). CANCEL_REPEAT cancels only its queued second play,
 * preserving the current sound. STOP/reset cancel the entire pair.
 * Returns1/0 for started/empty, count for updates/stops, -1 invalid. */
int ssa_audio_source(SsaAudio *,SsaAudioSourceCommand,unsigned song,
    uint64_t source,const SsaAudioPoint *);
int ssa_audio_level_music(SsaAudio *,unsigned level);
/* Music-request portion of original08EB0. Returns1 with a song (FFFF=stop),
 *0 for no request, -1 invalid arguments/data. Other UI/SFX/pause commands are
 * outside this selector. The settings byte enables requests only when==1. */
int ssa_audio_music_select(const uint8_t *,size_t,unsigned command,unsigned level,
    unsigned enabled,unsigned *song);
/* Queue a selection at a sound frame, waiting for an explicit fade if active.
 * Empty music players start immediately. Intro1/post-boss20 return to level2
 * after completion. PC terminal9/10 also retire old SFX, even without music. */
int ssa_audio_music(SsaAudio *,unsigned command,unsigned level);
/* Begin the original global fade. Interval0..255 is clamped to15; an active
 * fade is left running. Music requests wait for its completion. Affects SFX
 * too, as in the ROM. Returns1 started,0 already fading,-1 invalid. */
int ssa_audio_fade(SsaAudio *,unsigned interval);
/* Signed interleaved stereo16 output. Muting advances playback silently. */
int ssa_audio_render(SsaAudio *,int16_t *output,size_t frames);
#endif
