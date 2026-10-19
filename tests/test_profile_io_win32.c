#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../platform/win32/profile_win32.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"profile IO line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int wmain(int argc,wchar_t **argv){
    SsaWin32Profile p={0},q={0};HANDLE blocker;DWORD n;uint8_t bad[3]={0};
    CHECK(argc==2);CHECK(ssa_win32_profile_open(&p,argv[1],1));
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_CLEAR,0));
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_COMPLETE,5));
    blocker=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(blocker!=INVALID_HANDLE_VALUE);
    CHECK(!ssa_win32_profile_store(&p,SSA_SAVE_LANGUAGE,2));
    CHECK(p.data.completed==32 && p.data.language==2 && p.last_error==ERROR_SHARING_VIOLATION);
    CloseHandle(blocker);
    CHECK(ssa_win32_profile_open(&q,argv[1],1) && q.data.completed==32 && !q.data.language);
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_LANGUAGE,2));
    CHECK(ssa_win32_profile_open(&q,argv[1],1) && q.data.completed==32 && q.data.language==2);
    /* Another writer's explicit clear must still win over stale session bits. */
    CHECK(ssa_win32_profile_store(&q,SSA_SAVE_CLEAR,0));
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_COMPLETE,1) && p.data.completed==2);
    blocker=CreateFileW(argv[1],GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(blocker!=INVALID_HANDLE_VALUE);CHECK(WriteFile(blocker,bad,sizeof bad,&n,NULL) && n==sizeof bad);CloseHandle(blocker);
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_LANGUAGE,1) && p.data.completed==2);
    CHECK(ssa_win32_profile_open(&q,argv[1],1) && q.data.completed==2 && q.data.language==1);
    CHECK(SetFileAttributesW(argv[1],FILE_ATTRIBUTE_READONLY));
    CHECK(!ssa_win32_profile_store(&p,SSA_SAVE_COMPLETE,3) && p.data.completed==10);
    CHECK(SetFileAttributesW(argv[1],FILE_ATTRIBUTE_NORMAL));
    CHECK(ssa_win32_profile_open(&q,argv[1],1) && q.data.completed==2);
    CHECK(ssa_win32_profile_store(&p,SSA_SAVE_CLEAR,0) && !p.data.completed);
    printf("{\"result\":\"pass\",\"checks\":%u}\n",checks);return 0;
}
