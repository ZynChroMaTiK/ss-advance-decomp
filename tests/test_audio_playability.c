/* Run the same normal-input level completion with actual native music/SFX
 * decoding and mixing on every frame. No audio device is required. */
#include "ssa/native_game.h"
#include "ssa/audio.h"
#include <stdio.h>
static SsaAudio mixer;
static unsigned audio_failures,audio_frames,footstep_events,spatial_events,listener_updates,source_updates,music_events;
static unsigned level_seen;
static unsigned max_effect_instances,max_effect_voices,max_music_voices;
static unsigned door_moving_frames,door_stopped_loop_frames,door_unowned_loop_frames;
static uint64_t nonzero;
static void play_audio(void *context,unsigned id)
{if(id==13 || id==14)++footstep_events;if(ssa_audio_play(context,id)<0)++audio_failures;}
static void stop_audio(void *context,unsigned id)
{if(ssa_audio_stop(context,id)<0)++audio_failures;}
static void play_world(void *context,unsigned id,const SsaAudioPoint *point)
{++spatial_events;if(ssa_audio_play_at(context,id,point)<0)++audio_failures;}
static void listen_audio(void *context,const SsaAudioListener *listener)
{++listener_updates;if(ssa_audio_listen(context,listener)<0)++audio_failures;}
static void move_audio(void *context,unsigned id,const SsaAudioPoint *point)
{++source_updates;if(ssa_audio_move(context,id,point)<0)++audio_failures;}
static void music_audio(void *context,unsigned command,unsigned level)
{++music_events;if(ssa_audio_music(context,command,level)<0)++audio_failures;}
static void source_audio(void *context,SsaAudioSourceCommand command,unsigned id,uint64_t source,const SsaAudioPoint *point)
{
    if(command==SSA_AUDIO_SOURCE_PLAY){if(point)++spatial_events;else if(id==13 || id==14)++footstep_events;}
    if(command==SSA_AUDIO_SOURCE_LOOP){unsigned i,found=0;SsaAudio *a=context;
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].source==source && a->players[i].song==id && a->players[i].active)found=1;
        if(!found)++spatial_events;}
    if(command==SSA_AUDIO_SOURCE_MOVE)++source_updates;
    if(ssa_audio_source(context,command,id,source,point)<0)++audio_failures;
}
static int checked_step(SsaNativeGame *g)
{
    int16_t pcm[1600];unsigned i;int result;
    if(!mixer.loaded){SsaNativeAudioOps ops={play_audio,stop_audio,&mixer,play_world,listen_audio,move_audio,music_audio,source_audio};
        if(ssa_audio_init(&mixer,g->rom,g->rom_size,48000)<0 ||
           ssa_audio_level_music(&mixer,g->level)<0 || ssa_native_game_bind_audio(g,&ops)<0)return -1;
    }
    result=ssa_native_game_step(g);
    if(result==1){
        for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(mixer.players[i].active && mixer.players[i].song==0x67 &&
            (mixer.players[i].source<2 || mixer.players[i].source>=2+g->level_runtime.movers.count))++door_unowned_loop_frames;
        for(i=0;i<g->level_runtime.movers.count;++i){const uint8_t *r=g->level_runtime.movers.records[i];
            unsigned flags=r[14]|(unsigned)r[15]<<8,j;
            if(flags&8)++door_moving_frames;
            else for(j=1;j<SSA_AUDIO_PLAYERS;++j)
                if(mixer.players[j].active && mixer.players[j].source==2+i && mixer.players[j].song==0x67)++door_stopped_loop_frames;
        }
        if(ssa_audio_render(&mixer,pcm,800)<0)return -1;
        {unsigned effects=0,music=0,players=0;
            for(i=1;i<SSA_AUDIO_PLAYERS;++i)players+=mixer.players[i].active;
            for(i=0;i<SSA_AUDIO_VOICES;++i)if(mixer.voices[i].active){
                if(mixer.voices[i].player){++effects;if(i<SSA_AUDIO_MUSIC_VOICES)++audio_failures;}
                else{++music;if(i!=mixer.voices[i].track)++audio_failures;}}
            if(players>max_effect_instances)max_effect_instances=players;
            if(effects>max_effect_voices)max_effect_voices=effects;
            if(music>max_music_voices)max_music_voices=music;
        }
        if(g->finished){if(!mixer.players[0].active || mixer.players[0].song!=0x78)++audio_failures;}
        else if(mixer.players[0].active && mixer.players[0].song==0x73 && mixer.music_command==2)level_seen=1;
        else ++audio_failures;
        for(i=0;i<1600;++i)nonzero+=(unsigned)(pcm[i]!=0);
        ++audio_frames;
    }
    return result;
}
#define ssa_native_game_step checked_step
#define main bot_main
#include "test_native_playability.c"
#undef main
#undef ssa_native_game_step
int main(int argc,char **argv)
{
    int result=bot_main(argc,argv);
    printf("{\"door_moving_frames\":%u,\"door_stopped_loop_frames\":%u,\"door_unowned_loop_frames\":%u,",door_moving_frames,door_stopped_loop_frames,door_unowned_loop_frames);
    printf("\"max_effect_instances\":%u,\"max_effect_voices\":%u,\"max_music_voices\":%u,\"effect_recycles\":%llu,\"voice_recycles\":%llu,",
        max_effect_instances,max_effect_voices,max_music_voices,(unsigned long long)mixer.effect_recycles,(unsigned long long)mixer.voice_recycles);
    printf("\"audio_frames\":%u,\"mixed_frames\":%llu,\"notes\":%llu,\"loops\":%llu,\"nonzero_samples\":%llu,\"clipped_samples\":%llu,\"effects_limited_frames\":%llu,\"footstep_events\":%u,\"spatial_events\":%u,\"listener_updates\":%u,\"source_updates\":%u,\"music_preserved\":%u,\"music_events\":%u,\"completion_song\":%u,\"level_seen\":%u,\"errors\":%u}\n",
        audio_frames,(unsigned long long)mixer.frames,(unsigned long long)mixer.notes,
        (unsigned long long)mixer.loops,(unsigned long long)nonzero,(unsigned long long)mixer.clipped,(unsigned long long)mixer.effects_limited_frames,
        footstep_events,spatial_events,listener_updates,source_updates,!audio_failures,music_events,mixer.players[0].song,level_seen,mixer.errors+audio_failures);
    return result || door_unowned_loop_frames || door_stopped_loop_frames || !door_moving_frames || max_effect_instances<2 || max_effect_voices<2 || !max_music_voices || mixer.effect_recycles || mixer.voice_recycles || !nonzero || !footstep_events || !spatial_events || !listener_updates || !level_seen || music_events!=1 || mixer.players[0].song!=0x78 || mixer.errors || mixer.clipped || audio_failures;
}
