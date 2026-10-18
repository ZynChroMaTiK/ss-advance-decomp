/* Process-only loopback of the real MIDI map/fade/level handoff. Native PCM
 * music is silenced only in this diagnostic, to measure Windows MIDI alone. */
#define main timing_diagnostic_entry
#include "test_midi_sync_win32.cpp"
#undef main
extern "C" {
#include "audio_win32.h"
}
static void discard_music(void *,double stereo[2]){stereo[0]=stereo[1]=0;}
struct Energy {unsigned long long frames=0;double peak=0,energy=0;};
static Energy collect(IAudioCaptureClient *capture,unsigned milliseconds){
    Energy e;ULONGLONG end=GetTickCount64()+milliseconds;
    do{UINT32 available=0;check(capture->GetNextPacketSize(&available));
        while(available){BYTE *data;UINT32 count;DWORD flags;UINT64 position,time;
            check(capture->GetBuffer(&data,&count,&flags,&position,&time));
            const float *samples=(float *)data;
            for(UINT32 i=0;i<count*2;++i){double value=flags&AUDCLNT_BUFFERFLAGS_SILENT?0:samples[i];
                e.peak=std::max(e.peak,std::fabs(value));e.energy+=value*value;}
            e.frames+=count;check(capture->ReleaseBuffer(count));check(capture->GetNextPacketSize(&available));
        }
        Sleep(2);
    }while(GetTickCount64()<end);
    return e;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    std::vector<unsigned char> rom(0x800000);FILE *file=fopen(argv[1],"rb");
    if(!file || fread(rom.data(),1,rom.size(),file)!=rom.size())return 3;fclose(file);
    check(CoInitializeEx(NULL,COINIT_MULTITHREADED));
    AUDIOCLIENT_ACTIVATION_PARAMS ac={};ac.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    ac.ProcessLoopbackParams.TargetProcessId=GetCurrentProcessId();ac.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT parameters={};parameters.vt=VT_BLOB;parameters.blob.cbSize=sizeof ac;parameters.blob.pBlobData=(BYTE *)&ac;
    Activation *activation=new Activation;IActivateAudioInterfaceAsyncOperation *operation=nullptr;
    check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&parameters,activation,&operation));
    if(WaitForSingleObject(activation->done,10000)!=WAIT_OBJECT_0)return 4;
    check(activation->result);IAudioClient *client=activation->client;operation->Release();activation->Release();
    WAVEFORMATEX format={WAVE_FORMAT_IEEE_FLOAT,2,48000,48000*8,8,32,0};IAudioCaptureClient *capture=nullptr;
    check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,&format,NULL));
    check(client->GetService(__uuidof(IAudioCaptureClient),(void **)&capture));check(client->Start());
    SsaWin32Audio *audio=new SsaWin32Audio{};SsaSettings settings=SSA_SETTINGS_DEFAULT;settings.music_style=SSA_MUSIC_MIDI;
    if(!ssa_win32_audio_open(audio,rom.data(),rom.size(),0,0,0) || !ssa_win32_audio_settings(audio,&settings))return 5;
    EnterCriticalSection(&audio->mutex);audio->mixer.synth.mix_music=discard_music;LeaveCriticalSection(&audio->mutex);
    bool valid=true;
    for(unsigned level: {0u,6u}){
        if(!ssa_win32_audio_restart(audio,level))return 6;
        collect(capture,300);Energy map=collect(capture,1700);
        ssa_win32_audio_map_skip(audio);collect(capture,2200);
        EnterCriticalSection(&audio->mutex);int faded=audio->midi->expression;LeaveCriticalSection(&audio->mutex);
        if(!ssa_win32_audio_begin_level(audio,level))return 7;
        collect(capture,300);Energy play=collect(capture,2000);
        EnterCriticalSection(&audio->mutex);
        int expression=audio->midi->expression;unsigned errors=audio->midi->error+audio->mixer.errors+audio->last_error;
        unsigned long long notes=audio->midi->notes;
        std::printf("{\"level\":%u,\"map_peak\":%.8f,\"level_peak\":%.8f,\"map_energy\":%.8f,\"level_energy\":%.8f,\"faded_expression\":%d,\"level_expression\":%d,\"notes\":%llu,\"errors\":%u}\n",level,map.peak,play.peak,map.energy,play.energy,faded,expression,notes,errors);
        LeaveCriticalSection(&audio->mutex);fflush(stdout);
        valid=valid && map.peak>.001 && play.peak>.001 && faded==0 && expression>0 && !errors;
    }
    ssa_win32_audio_close(audio);delete audio;client->Stop();capture->Release();client->Release();CoUninitialize();
    std::printf("{\"result\":\"%s\",\"transitions\":2,\"process_loopback_only\":true,\"pcm_music_excluded\":true,\"microphone_used\":false}\n",valid?"pass":"fail");
    return valid?0:8;
}
