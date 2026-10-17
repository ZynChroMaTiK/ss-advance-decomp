#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "audio_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* No window, input messages or main-thread pumping during idle cases. Output
 * is muted, but the real device must consume music at wall-clock speed. */
static int measure(SsaWin32Audio *a,unsigned assisted,unsigned effects)
{
    LARGE_INTEGER frequency,start,end;unsigned first,last,i;uint64_t recycled;
    double elapsed,ratio;int ok;
    if(!ssa_win32_audio_begin_level(a,0))return 0;
    if(effects)for(i=0;i<16;++i){
        SsaAudioPoint p={(int32_t)i*32*65536,0,0};
        ssa_win32_audio_source(a,SSA_AUDIO_SOURCE_LOOP,0x12,128+i,&p);
    }
    Sleep(300); /* Settle the device start before measuring. */
    first=ssa_win32_audio_position(a);
    EnterCriticalSection(&a->mutex);recycled=a->recycled;LeaveCriticalSection(&a->mutex);
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&start))return 0;
    if(!assisted)Sleep(3000);
    else do {
        if(!ssa_win32_audio_pump(a))return 0;
        Sleep(1);if(!QueryPerformanceCounter(&end))return 0;
    }while((double)(end.QuadPart-start.QuadPart)/(double)frequency.QuadPart<3);
    if(!QueryPerformanceCounter(&end))return 0;
    last=ssa_win32_audio_position(a);
    elapsed=(double)(end.QuadPart-start.QuadPart)/(double)frequency.QuadPart;
    ratio=(double)(last-first)/(48000*elapsed);
    EnterCriticalSection(&a->mutex);
    ok=last>first && ratio>=0.98 && ratio<=1.04 && a->recycled>recycled && !a->last_error && !a->mixer.errors;
    printf("{\"case\":\"%s\",\"effects\":%u,\"seconds\":%.6f,\"played_frames\":%u,"
        "\"speed_ratio\":%.6f,\"recycled\":%llu,\"errors\":%u,\"result\":\"%s\"}\n",
        assisted?"assisted":"idle",effects,elapsed,last-first,ratio,
        (unsigned long long)(a->recycled-recycled),a->last_error+a->mixer.errors,ok?"pass":"fail");
    LeaveCriticalSection(&a->mutex);fflush(stdout);return ok;
}
int main(int argc,char **argv)
{
    SsaWin32Audio *a=calloc(1,sizeof(*a));uint8_t *rom=malloc(0x800000);FILE *file;int ok=1;
    if(argc!=2 || !a || !rom)return 2;
    file=fopen(argv[1],"rb");if(!file)return 2;
    if(fread(rom,1,0x800000,file)!=0x800000){fclose(file);return 2;}fclose(file);
    if(!ssa_win32_audio_open(a,rom,0x800000,0,1,0)){fprintf(stderr,"Audio open failed: %u\n",a->last_error);return 2;}
    ok&=measure(a,0,0);ok&=measure(a,1,0);ok&=measure(a,0,1);
    ssa_win32_audio_close(a);free(a);free(rom);return ok?0:1;
}
