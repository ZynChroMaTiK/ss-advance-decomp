#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "profile_win32.h"
int ssa_win32_settings_read(SsaSettings *s,const wchar_t *path){
 uint8_t b[SSA_SETTINGS_BYTES+1];DWORD n;HANDLE f;
 if(!s || !path)return 0;
 f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
 if(!ReadFile(f,b,sizeof b,&n,NULL)){CloseHandle(f);return 0;}
 CloseHandle(f);return ssa_settings_decode(s,b,n)==1;
}
int ssa_win32_settings_write(const SsaSettings *s,const wchar_t *path){
 wchar_t temp[32768];uint8_t b[SSA_SETTINGS_BYTES];HANDLE f=INVALID_HANDLE_VALUE;DWORD n;unsigned i;int count,ok;
 if(!path || ssa_settings_encode(s,b,sizeof b)!=1)return 0;
 for(i=0;i<32;++i){
  count=_snwprintf(temp,32768,L"%ls.tmp-%lu-%u",path,(unsigned long)GetCurrentProcessId(),i);
  if(count<0 || count>=32768)return 0;
  f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(f!=INVALID_HANDLE_VALUE)break;
  if(GetLastError()!=ERROR_FILE_EXISTS)return 0;
 }
 if(f==INVALID_HANDLE_VALUE)return 0;
 ok=WriteFile(f,b,sizeof b,&n,NULL) && n==sizeof b && FlushFileBuffers(f);
 CloseHandle(f);
 if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
 if(!ok)DeleteFileW(temp);
 return ok;
}
static int read_profile(const wchar_t *path,SsaProfile *p,unsigned *error){
 uint8_t b[SSA_PROFILE_BYTES+1];DWORD n;HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE){*error=GetLastError();return *error==ERROR_FILE_NOT_FOUND?0:-1;}
 if(!ReadFile(f,b,sizeof b,&n,NULL)){*error=GetLastError();CloseHandle(f);return -1;}
 CloseHandle(f);if(ssa_profile_decode(p,b,n)!=1){*error=ERROR_INVALID_DATA;return -1;}return 1;
}
int ssa_win32_profile_open(SsaWin32Profile *p,const wchar_t *path,unsigned enabled){
 DWORD n;int result;
 if(!p || !path || enabled>1)return 0;
 memset(p,0,sizeof(*p));ssa_profile_reset(&p->data);p->enabled=enabled;
 n=GetFullPathNameW(path,32768,p->path,NULL);
 if(!n || n>=32768){p->last_error=n?ERROR_FILENAME_EXCED_RANGE:GetLastError();return 0;}
 if(!enabled)return 1;
 result=read_profile(p->path,&p->data,&p->last_error);
 if(result>=0){p->last_error=0;return 1;}return 0;
}
static int apply(SsaProfile *p,unsigned action,unsigned value){
 if(action==SSA_SAVE_CLEAR){ssa_profile_reset(p);return 1;}
 if(action==SSA_SAVE_LANGUAGE){if(value>=3)return 0;p->language=value;return 1;}
 return action==SSA_SAVE_COMPLETE && ssa_profile_complete(p,value)>=0;
}
int ssa_win32_profile_store(SsaWin32Profile *p,unsigned action,unsigned value){
 wchar_t lock_path[32768],temporary[32768];uint8_t b[SSA_PROFILE_BYTES];DWORD written;
 HANDLE lock=INVALID_HANDLE_VALUE,f=INVALID_HANDLE_VALUE;SsaProfile next;unsigned attempt,error=0;int ok=0,created=0,n;
 if(!p)return 0;
 next=p->data;if(!apply(&next,action,value))return 0;
 if(!p->enabled){p->data=next;return 1;}
 n=_snwprintf(lock_path,32768,L"%ls.lock",p->path);
 if(n<0 || n>=32768){p->last_error=ERROR_FILENAME_EXCED_RANGE;return 0;}
 for(attempt=0;attempt<25;++attempt){
  lock=CreateFileW(lock_path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_DELETE_ON_CLOSE,NULL);
  if(lock!=INVALID_HANDLE_VALUE)break;
  error=GetLastError();if(error!=ERROR_SHARING_VIOLATION && error!=ERROR_ACCESS_DENIED)break;
  Sleep(10);
 }
 if(lock==INVALID_HANDLE_VALUE){p->last_error=error;p->data=next;return 0;}
 ssa_profile_reset(&next);
 (void)read_profile(p->path,&next,&error); /* Bad/missing old files can be replaced by an explicit change. */
 if(!apply(&next,action,value) || ssa_profile_encode(&next,b,sizeof b)!=1)goto done;
 for(attempt=0;attempt<32;++attempt){
  n=_snwprintf(temporary,32768,L"%ls.tmp-%lu-%u",p->path,(unsigned long)GetCurrentProcessId(),attempt);
  if(n<0 || n>=32768){error=ERROR_FILENAME_EXCED_RANGE;goto done;}
  f=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(f!=INVALID_HANDLE_VALUE){created=1;break;}
  error=GetLastError();if(error!=ERROR_FILE_EXISTS)goto done;
 }
 if(f==INVALID_HANDLE_VALUE)goto done;
 if(!WriteFile(f,b,sizeof b,&written,NULL) || written!=sizeof b || !FlushFileBuffers(f)){error=GetLastError();goto done;}
 CloseHandle(f);f=INVALID_HANDLE_VALUE;
 if(!MoveFileExW(temporary,p->path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=GetLastError();goto done;}
 created=0;ok=1;
done:
 if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 if(created)DeleteFileW(temporary);
 CloseHandle(lock);p->data=next;p->last_error=ok?0:error;return ok;
}
