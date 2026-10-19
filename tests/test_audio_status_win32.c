#include "../platform/win32/audio_win32.h"
#include <stdio.h>
#include <stdlib.h>
static DWORD WINAPI change_status(void *context){
    SsaWin32Audio *a=context;unsigned i;
    for(i=0;i<50000;++i){EnterCriticalSection(&a->mutex);
        a->last_error=a->midi->error=i&1;a->mixer.players[0].active=i&1;
        LeaveCriticalSection(&a->mutex);}
    return 0;
}
int main(void){
    SsaWin32Audio *a=calloc(1,sizeof(*a));HANDLE worker;unsigned i,bad=0;
    if(!a)return 1;
    a->midi=calloc(1,sizeof(*a->midi));if(!a->midi){free(a);return 1;}
    InitializeCriticalSection(&a->mutex);a->synchronized=1;
    worker=CreateThread(NULL,0,change_status,a,0,NULL);if(!worker)return 1;
    for(i=0;i<50000;++i){SsaWin32AudioStatus s=ssa_win32_audio_status(a);
        bad+=s.last_error!=s.midi_error || s.music_active!=s.midi_error || s.midi_available!=(s.midi_error==0) || s.soundfont_available;}
    WaitForSingleObject(worker,INFINITE);CloseHandle(worker);DeleteCriticalSection(&a->mutex);
    free(a->midi);free(a);printf("{\"result\":\"%s\",\"snapshots\":50000}\n",bad?"fail":"pass");return bad?1:0;
}
