#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <audioclientactivationparams.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
extern "C" {
#include "midi_win32.h"
}
/* Explicit audible diagnostic, never part of normal game startup. Windows
 * process loopback captures only this test's own output; samples stay in RAM. */
static void check(HRESULT hr){if(FAILED(hr)){std::fprintf(stderr,"HRESULT %08lx\n",(unsigned long)hr);std::exit(1);}}
static void msg(HMIDIOUT m,unsigned s,unsigned a,unsigned b){if(midiOutShortMsg(m,s|(a<<8)|(b<<16)))std::exit(2);}
struct Activation: IActivateAudioInterfaceCompletionHandler, IAgileObject {
    LONG references=1;HANDLE done=CreateEventW(NULL,TRUE,FALSE,NULL);IAudioClient *client=nullptr;HRESULT result=E_FAIL;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) override {
        if(id==__uuidof(IUnknown) || id==__uuidof(IActivateAudioInterfaceCompletionHandler))*out=(IActivateAudioInterfaceCompletionHandler *)this;
        else if(id==__uuidof(IAgileObject))*out=(IAgileObject *)this;else{*out=nullptr;return E_NOINTERFACE;}AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&references);}
    ULONG STDMETHODCALLTYPE Release() override{LONG n=InterlockedDecrement(&references);if(!n){CloseHandle(done);delete this;}return n;}
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation *op) override{
        IUnknown *object=nullptr;HRESULT activationResult=E_FAIL;result=op->GetActivateResult(&activationResult,&object);
        if(SUCCEEDED(result))result=activationResult;
        if(SUCCEEDED(result) && object){result=object->QueryInterface(__uuidof(IAudioClient),(void **)&client);object->Release();}
        SetEvent(done);return S_OK;}
};
int main(int argc,char **argv){
    unsigned delay=argc>1?(unsigned)atoi(argv[1]):SSA_MIDI_DELAY_MS;
    if(delay>SSA_MIDI_MAX_DELAY_MS)return 8;
    check(CoInitializeEx(NULL,COINIT_MULTITHREADED));
    IAudioClient *client=nullptr;IAudioCaptureClient *capture=nullptr;
    AUDIOCLIENT_ACTIVATION_PARAMS ac={};ac.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    ac.ProcessLoopbackParams.TargetProcessId=GetCurrentProcessId();ac.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT parameters={};parameters.vt=VT_BLOB;parameters.blob.cbSize=sizeof ac;parameters.blob.pBlobData=(BYTE *)&ac;
    Activation *activation=new Activation;IActivateAudioInterfaceAsyncOperation *operation=nullptr;
    check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&parameters,activation,&operation));
    if(WaitForSingleObject(activation->done,10000)!=WAIT_OBJECT_0)return 7;
    check(activation->result);client=activation->client;operation->Release();activation->Release();
    WAVEFORMATEX storage={WAVE_FORMAT_IEEE_FLOAT,2,48000,48000*8,8,32,0};WAVEFORMATEX *format=&storage;
    if(format->wBitsPerSample!=32 || format->nChannels<2){std::fprintf(stderr,"Need stereo float loopback\n");return 3;}
    check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,format,NULL));
    check(client->GetService(__uuidof(IAudioCaptureClient),(void **)&capture));
    SsaAudio mixer={};mixer.sample_rate=48000;mixer.master_gain=1;mixer.music_enabled=1;mixer.music_volume=100;
    SsaWin32Midi *backend=ssa_win32_midi_open(&mixer);if(!backend)return 4;
    if(!ssa_win32_midi_set_delay(backend,delay))return 8;
    SsaAudioSynth synth=ssa_win32_midi_synth(backend);HMIDIOUT midi=backend->device;
    msg(midi,0xb0,120,0);msg(midi,0xb0,7,100);msg(midi,0xb0,11,127);msg(midi,0xb0,10,127);msg(midi,0xb0,91,0);msg(midi,0xb0,93,0);msg(midi,0xc0,115,0);
    const unsigned rate=48000,seconds=5;
    std::vector<short> pcm(rate*seconds*2,0);
    for(unsigned n=1;n<=4;++n)for(unsigned j=0;j<480;++j)pcm[(n*rate+j)*2]=(short)(std::sin(j*0.29)*10000);
    WAVEFORMATEX wf={WAVE_FORMAT_PCM,2,rate,rate*4,4,16,0};HWAVEOUT wave=nullptr;
    if(waveOutOpen(&wave,WAVE_MAPPER,&wf,0,0,CALLBACK_NULL))return 5;
    WAVEHDR headers[3]={};short buffers[3][1600]={};bool queued[3]={};unsigned rendered=0;
    for(unsigned i=0;i<3;++i){headers[i].lpData=(LPSTR)buffers[i];headers[i].dwBufferLength=sizeof buffers[i];waveOutPrepareHeader(wave,&headers[i],sizeof(WAVEHDR));}
    auto pump=[&](){for(unsigned i=0;i<3;++i){
        if(queued[i] && !(headers[i].dwFlags&WHDR_DONE))continue;
        if(rendered && rendered%rate==0 && rendered/rate<=4){msg(midi,0x80,60,0);msg(midi,0x90,60,110);}
        for(unsigned j=0;j<800;++j){double stereo[2]={rendered*2+j*2<pcm.size()?(double)pcm[rendered*2+j*2]:0,0};
            synth.mix_music(backend,stereo);buffers[i][j*2]=(short)stereo[0];buffers[i][j*2+1]=(short)stereo[1];}
        rendered+=800;waveOutWrite(wave,&headers[i],sizeof(WAVEHDR));queued[i]=true;
    }};
    check(client->Start());pump();
    std::vector<double> l,r;std::vector<UINT64> time;LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    ULONGLONG until=GetTickCount64()+5500;
    while(GetTickCount64()<until){
        pump();
        UINT32 available=0;check(capture->GetNextPacketSize(&available));
        while(available){BYTE *data;UINT32 count;DWORD flags;UINT64 device,qpc;
            check(capture->GetBuffer(&data,&count,&flags,&device,&qpc));
            const float *values=(float *)data;
            for(UINT32 i=0;i<count;++i){bool silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
                l.push_back(silent?0:std::fabs(values[i*format->nChannels]));r.push_back(silent?0:std::fabs(values[i*format->nChannels+1]));
                time.push_back(qpc+(UINT64)i*10000000/format->nSamplesPerSec);}
            check(capture->ReleaseBuffer(count));check(capture->GetNextPacketSize(&available));
        }
        Sleep(1);
    }
    ssa_win32_midi_close(backend);
    waveOutReset(wave);for(unsigned i=0;i<3;++i)waveOutUnprepareHeader(wave,&headers[i],sizeof(WAVEHDR));waveOutClose(wave);client->Stop();
    std::vector<double> left,right;
    UINT64 left_last=0,right_last=0;
    for(size_t i=0;i<time.size();++i){
        if(l[i]>.01 && time[i]>left_last+5000000){left.push_back(time[i]/10000.0);left_last=time[i];}
        if(r[i]>.003 && time[i]>right_last+5000000){right.push_back(time[i]/10000.0);right_last=time[i];}
    }
    bool valid=left.size()==4 && right.size()==4;double max_error=0;
    std::printf("{\"pcm_onsets\":%zu,\"midi_onsets\":%zu,\"compensation_ms\":%u,\"offset_ms\":[",left.size(),right.size(),delay);
    for(size_t i=0;i<std::min(left.size(),right.size());++i){double difference=right[i]-left[i];
        std::printf("%s%.3f",i?",":"",difference);max_error=std::max(max_error,std::fabs(difference));}
    if(delay && max_error>15)valid=false;
    std::printf("],\"max_error_ms\":%.3f,\"process_loopback_only\":true,\"result\":\"%s\"}\n",max_error,valid?"pass":"fail");
    capture->Release();client->Release();CoUninitialize();return valid?0:9;
}
