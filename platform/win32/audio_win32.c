#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "audio_win32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void ssa_win32_audio_quiesce(SsaWin32Audio *a)
{
    if(!a)return;
    if(a->stop_event)SetEvent(a->stop_event);
    if(a->thread)WaitForSingleObject(a->thread,INFINITE);
}
void ssa_win32_audio_close(SsaWin32Audio *a)
{
    unsigned i;
    if(!a)return;
    ssa_win32_audio_quiesce(a);
    if(a->mixer.loaded)(void)ssa_audio_set_synth(&a->mixer,NULL);
    ssa_soundfont_free(a->soundfont);a->soundfont=NULL;
    free(a->soundfont_path);a->soundfont_path=NULL;
    ssa_win32_midi_close(a->midi);a->midi=NULL;a->music_style=SSA_MUSIC_ORIGINAL;
    if(a->thread){CloseHandle(a->thread);a->thread=NULL;}
    if(a->stop_event){CloseHandle(a->stop_event);a->stop_event=NULL;}
    if(a->device)waveOutReset(a->device);
    for(i=0;i<SSA_AUDIO_BUFFERS;++i){
        if(a->device && a->prepared[i])waveOutUnprepareHeader(a->device,&a->headers[i],sizeof(WAVEHDR));
        a->prepared[i]=a->queued[i]=0;
    }
    if(a->device)waveOutClose(a->device);
    a->device=NULL;
    /* The driver may signal buffer_event through reset/close. Keep it alive
     * until waveOutClose has returned, after the worker has joined. */
    if(a->buffer_event){CloseHandle(a->buffer_event);a->buffer_event=NULL;}
    if(a->synchronized){DeleteCriticalSection(&a->mutex);a->synchronized=0;}
}
static int pump(SsaWin32Audio *a)
{
    unsigned i,j;MMRESULT result;
    if(!a->device || a->last_error)return 0;
    /* Clear before scanning all headers: a completion during this scan stays
     * signaled for the next wake. Clearing afterward could lose that wake. */
    ResetEvent(a->buffer_event);
    for(i=0;i<SSA_AUDIO_BUFFERS;++i){
        if(a->queued[i] && !(a->headers[i].dwFlags&WHDR_DONE))continue;
        if(a->queued[i])++a->recycled;
        if(ssa_audio_render(&a->mixer,a->pcm[i],SSA_AUDIO_BLOCK)<0){a->last_error=MMSYSERR_ERROR;return 0;}
        for(j=0;j<SSA_AUDIO_BLOCK*2;++j){int value=a->pcm[i][j];unsigned magnitude=(unsigned)(value<0?-value:value);
            if(magnitude>a->peak)a->peak=magnitude;}
        result=waveOutWrite(a->device,&a->headers[i],sizeof(WAVEHDR));
        if(result!=MMSYSERR_NOERROR){a->last_error=result;return 0;}
        a->queued[i]=1;++a->submitted;
    }
    return 1;
}
int ssa_win32_audio_soundfont_available(const SsaWin32Audio *a)
{
    DWORD attributes;
    if(!a || !a->soundfont || !a->soundfont_path)return 0;
    attributes=GetFileAttributesW(a->soundfont_path);
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
int ssa_win32_audio_load_soundfont(SsaWin32Audio *a,const wchar_t *path)
{
    FILE *f=NULL;void *data;long size;SsaSoundFont *s;wchar_t *saved_path;
    if(!a || !a->synchronized || !path || _wfopen_s(&f,path,L"rb") || !f)return 0;
    if(fseek(f,0,SEEK_END) || (size=ftell(f))<12 || size>64*1024*1024){fclose(f);return 0;}
    rewind(f);data=malloc((size_t)size);
    if(!data){fclose(f);return 0;}
    if(fread(data,1,(size_t)size,f)!=(size_t)size){free(data);fclose(f);return 0;}
    fclose(f);s=ssa_soundfont_load(data,(size_t)size,a->mixer.sample_rate);free(data);
    if(!s)return 0;
    saved_path=_wcsdup(path);if(!saved_path){ssa_soundfont_free(s);return 0;}
    EnterCriticalSection(&a->mutex);
    if(a->music_style==SSA_MUSIC_SOUNDFONT){(void)ssa_audio_set_synth(&a->mixer,NULL);a->music_style=SSA_MUSIC_ORIGINAL;}
    ssa_soundfont_free(a->soundfont);a->soundfont=s;
    free(a->soundfont_path);a->soundfont_path=saved_path;
    LeaveCriticalSection(&a->mutex);return 1;
}
SsaWin32AudioStatus ssa_win32_audio_status(SsaWin32Audio *a)
{
    SsaWin32AudioStatus s={0};
    if(!a)return s;
    if(a->synchronized)EnterCriticalSection(&a->mutex);
    s.soundfont_available=ssa_win32_audio_soundfont_available(a);
    s.midi_error=a->midi?a->midi->error:0;
    s.midi_available=a->midi && !s.midi_error;
    s.last_error=a->last_error;s.music_active=a->mixer.players[0].active;
    if(a->synchronized)LeaveCriticalSection(&a->mutex);
    return s;
}
int ssa_win32_audio_settings(SsaWin32Audio *a,const SsaSettings *settings)
{
    int result=1;SsaAudioSynth synth={0};
    if(!a || !settings || settings->music_style>=SSA_MUSIC_STYLES || settings->music_volume>100 || settings->sound_volume>100)return 0;
    if(!a->synchronized)return settings->music_style==0;
    EnterCriticalSection(&a->mutex);
    if((settings->music_style==SSA_MUSIC_SOUNDFONT && !ssa_win32_audio_soundfont_available(a)) ||
       (settings->music_style==SSA_MUSIC_MIDI && (!a->midi || a->midi->error)))result=0;
    else{
        unsigned previous_music=a->mixer.music_volume,previous_sounds=a->mixer.sound_volume;
        /* A newly attached MIDI backend may emit held notes immediately.
         * Establish the requested volume before attachment, including zero. */
        (void)ssa_audio_set_volume(&a->mixer,settings->music_volume,settings->sound_volume);
        if(settings->music_style!=a->music_style){SsaAudioSynth previous=a->mixer.synth;
            if(settings->music_style==SSA_MUSIC_SOUNDFONT)synth=ssa_soundfont_synth(a->soundfont);
            else if(settings->music_style==SSA_MUSIC_MIDI)synth=ssa_win32_midi_synth(a->midi);
            result=ssa_audio_set_synth(&a->mixer,settings->music_style?&synth:NULL)==1;
            if(result)a->music_style=settings->music_style;
            else{
                (void)ssa_audio_set_volume(&a->mixer,previous_music,previous_sounds);
                (void)ssa_audio_set_synth(&a->mixer,previous.context?&previous:NULL);
            }
        }
        if(a->music_style==SSA_MUSIC_MIDI)ssa_win32_midi_volume(a->midi);
    }
    LeaveCriticalSection(&a->mutex);return result;
}
int ssa_win32_audio_pump(SsaWin32Audio *a)
{
    int result;
    if(!a || !a->synchronized)return 0;
    EnterCriticalSection(&a->mutex);result=pump(a);LeaveCriticalSection(&a->mutex);return result;
}
static DWORD WINAPI worker(void *context)
{
    SsaWin32Audio *a=context;HANDLE events[2]={a->stop_event,a->buffer_event};DWORD result;
    for(;;){
        result=WaitForMultipleObjects(2,events,FALSE,INFINITE);
        if(result==WAIT_OBJECT_0)break;
        if(result!=WAIT_OBJECT_0+1){
            EnterCriticalSection(&a->mutex);a->last_error=MMSYSERR_ERROR;LeaveCriticalSection(&a->mutex);break;
        }
        if(!ssa_win32_audio_pump(a))break;
    }
    return 0;
}
int ssa_win32_audio_open(SsaWin32Audio *a,const uint8_t *rom,size_t size,unsigned level,unsigned mute,unsigned no_music)
{
    WAVEFORMATEX format={0};MMRESULT result;unsigned i;
    if(!a || a->device || a->mixer.loaded || ssa_audio_init(&a->mixer,rom,size,48000)<0)return 0;
    a->mixer.muted=mute;a->mixer.music_enabled=!no_music;
    (void)level; /* Menu begins silent; its original callback requests music. */
    format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
    format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
    a->buffer_event=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!a->buffer_event){a->last_error=MMSYSERR_NOMEM;return 0;}
    result=waveOutOpen(&a->device,WAVE_MAPPER,&format,(DWORD_PTR)a->buffer_event,0,CALLBACK_EVENT);
    if(result!=MMSYSERR_NOERROR){a->last_error=result;a->device=NULL;ssa_win32_audio_close(a);return 0;}
    InitializeCriticalSection(&a->mutex);a->synchronized=1;
    a->midi=ssa_win32_midi_open(&a->mixer);
    a->stop_event=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!a->stop_event){a->last_error=MMSYSERR_NOMEM;ssa_win32_audio_close(a);return 0;}
    for(i=0;i<SSA_AUDIO_BUFFERS;++i){
        a->headers[i].lpData=(LPSTR)a->pcm[i];a->headers[i].dwBufferLength=sizeof(a->pcm[i]);
        result=waveOutPrepareHeader(a->device,&a->headers[i],sizeof(WAVEHDR));
        if(result!=MMSYSERR_NOERROR){a->last_error=result;ssa_win32_audio_close(a);return 0;}
        a->prepared[i]=1;
    }
    if(!ssa_win32_audio_pump(a)){ssa_win32_audio_close(a);return 0;}
    a->thread=CreateThread(NULL,0,worker,a,0,NULL);
    if(!a->thread){a->last_error=MMSYSERR_NOMEM;ssa_win32_audio_close(a);return 0;}
    return 1;
}
static int restart_music(SsaWin32Audio *a,unsigned level,unsigned command)
{
    unsigned i;MMRESULT result;int ok=0;
    if(!a || !a->device)return 0;
    EnterCriticalSection(&a->mutex);result=waveOutReset(a->device);
    if(result!=MMSYSERR_NOERROR){a->last_error=result;goto done;}
    for(i=0;i<SSA_AUDIO_BUFFERS;++i)a->queued[i]=0;
    ssa_audio_reset(&a->mixer);
    if(ssa_audio_music(&a->mixer,command,level)<0)goto done;
    ok=pump(a);
done:LeaveCriticalSection(&a->mutex);return ok;
}
int ssa_win32_audio_restart(SsaWin32Audio *a,unsigned level)
{return restart_music(a,level,3);}
int ssa_win32_audio_begin_level(SsaWin32Audio *a,unsigned level)
{return restart_music(a,level,2);}
int ssa_win32_audio_begin_story(SsaWin32Audio *a,unsigned level)
{return restart_music(a,level,11);}
int ssa_win32_audio_screen(SsaWin32Audio *a,unsigned level,unsigned command)
{return restart_music(a,level,command);}
void ssa_win32_audio_map_skip(SsaWin32Audio *a)
{
    if(!a || !a->synchronized)return;
    EnterCriticalSection(&a->mutex);
    (void)ssa_audio_play(&a->mixer,2); /* Original music command13: menu confirm. */
    if(a->mixer.music_enabled)(void)ssa_audio_fade(&a->mixer,5);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_menu_command(SsaWin32Audio *a,unsigned c,unsigned level,unsigned music_enabled)
{
    if(!a || !a->synchronized)return;
    EnterCriticalSection(&a->mutex);a->mixer.music_enabled=music_enabled;
    if(c==12 || c==13 || c==16 || c==17)(void)ssa_audio_play(&a->mixer,c-11);
    /* Main-page entry repeats command8 after Press Start (and Back). Keep
     * the existing title/menu sequence and voices; re-enable still starts it. */
    else if(!(c==8 && music_enabled && a->mixer.music_command==8 &&
              a->mixer.players[0].active && a->mixer.players[0].song==0x6f &&
              !(a->mixer.music.flags&6)))
        (void)ssa_audio_music(&a->mixer,c,level);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_fade(SsaWin32Audio *a,unsigned interval)
{
    if(!a || !a->synchronized)return;
    EnterCriticalSection(&a->mutex);(void)ssa_audio_fade(&a->mixer,interval);LeaveCriticalSection(&a->mutex);
}
unsigned ssa_win32_audio_position(SsaWin32Audio *a)
{
    MMTIME position={0};position.wType=TIME_SAMPLES;
    if(!a || !a->device || waveOutGetPosition(a->device,&position,sizeof(position))!=MMSYSERR_NOERROR || position.wType!=TIME_SAMPLES)return 0;
    return position.u.sample;
}
void ssa_win32_audio_play(void *context,unsigned song)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);
    if(ssa_audio_play(&a->mixer,song)<0)fprintf(stderr,"Audio data rejected sound %u (command %u).\n",song,a->mixer.last_error_command);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_stop(void *context,unsigned song)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);(void)ssa_audio_stop(&a->mixer,song);LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_play_at(void *context,unsigned song,const SsaAudioPoint *position)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);
    if(ssa_audio_play_at(&a->mixer,song,position)<0)fprintf(stderr,"Audio data rejected world sound %u.\n",song);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_listen(void *context,const SsaAudioListener *listener)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);(void)ssa_audio_listen(&a->mixer,listener);LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_move(void *context,unsigned song,const SsaAudioPoint *position)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);(void)ssa_audio_move(&a->mixer,song,position);LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_music(void *context,unsigned command,unsigned level)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);
    if(ssa_audio_music(&a->mixer,command,level)<0)fprintf(stderr,"Audio rejected music command %u at level %u.\n",command,level);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_source(void *context,SsaAudioSourceCommand command,unsigned song,uint64_t source,const SsaAudioPoint *position)
{
    SsaWin32Audio *a=context;
    if(!a->synchronized)return;
    EnterCriticalSection(&a->mutex);
    if(ssa_audio_source(&a->mixer,command,song,source,position)<0)fprintf(stderr,"Audio rejected source sound %u (action %u).\n",song,(unsigned)command);
    LeaveCriticalSection(&a->mutex);
}
void ssa_win32_audio_toggle_mute(SsaWin32Audio *a)
{
    if(!a)return;
    if(a->synchronized)EnterCriticalSection(&a->mutex);
    a->mixer.muted=!a->mixer.muted;
    if(a->music_style==SSA_MUSIC_MIDI)ssa_win32_midi_volume(a->midi);
    if(a->synchronized)LeaveCriticalSection(&a->mutex);
}
