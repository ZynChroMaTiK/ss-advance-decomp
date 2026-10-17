#include "input_win32.h"
#include <stdlib.h>
#include <string.h>

/* RegisterRawInputDevices has one target per process/device class. This is an
 * application-owned backend, not an automatically registering shared library. */
static SsaWin32Input *owner;
static int fail(SsaWin32Input *b,DWORD error) {
    if(b)b->last_error=error?error:ERROR_INVALID_PARAMETER;
    return -1;
}
static int ready(const SsaWin32Input *b) {
    return b && owner==b && b->attached && b->window && b->runtime &&
        b->runtime->input && b->thread_id==GetCurrentThreadId();
}
static int mouse_registration(RAWINPUTDEVICE *mouse,int *found) {
    UINT count=0,result,i;RAWINPUTDEVICE *list;
    *found=0;SetLastError(ERROR_SUCCESS);
    result=GetRegisteredRawInputDevices(NULL,&count,sizeof(RAWINPUTDEVICE));
    if(result==(UINT)-1 && GetLastError()!=ERROR_INSUFFICIENT_BUFFER)return -1;
    if(!count)return 0;
    if((size_t)count>SIZE_MAX/sizeof(*list)) {SetLastError(ERROR_NOT_ENOUGH_MEMORY);return -1;}
    list=malloc((size_t)count*sizeof(*list));
    if(!list) {SetLastError(ERROR_NOT_ENOUGH_MEMORY);return -1;}
    result=GetRegisteredRawInputDevices(list,&count,sizeof(*list));
    if(result==(UINT)-1) {DWORD error=GetLastError();free(list);SetLastError(error);return -1;}
    for(i=0;i<result;++i)if(list[i].usUsagePage==1 && list[i].usUsage==2) {
        *mouse=list[i];*found=1;break;
    }
    free(list);return 0;
}
int ssa_win32_input_attach(SsaWin32Input *b,HWND window,SsaPcInputRuntime *runtime) {
    SsaWin32Input next;RAWINPUTDEVICE mouse={1,2,0,NULL};DWORD process=0;
    int found;
    if(!b || b->attached || owner || !window || !IsWindow(window) || !runtime ||
       !runtime->input || runtime->input->yaw_per_count<=0 || !runtime->game || runtime->game->updating ||
       GetWindowThreadProcessId(window,&process)!=GetCurrentThreadId() ||
       process!=GetCurrentProcessId())return fail(b,ERROR_INVALID_PARAMETER);
    memset(&next,0,sizeof(next));
    if(mouse_registration(&next.previous_mouse,&found)<0)return fail(b,GetLastError());
    mouse.hwndTarget=window;
    if(!RegisterRawInputDevices(&mouse,1,sizeof(mouse)))return fail(b,GetLastError());
    next.window=window;next.runtime=runtime;next.thread_id=GetCurrentThreadId();
    next.attached=1;next.had_previous=(unsigned)found;next.focused=GetFocus()==window;
    *b=next;owner=b;ssa_pc_input_runtime_focus(runtime,(int)b->focused);return 0;
}
int ssa_win32_input_detach(SsaWin32Input *b) {
    RAWINPUTDEVICE current={0},restore={1,2,RIDEV_REMOVE,NULL};int found,replaced;
    if(!ready(b))return fail(b,ERROR_INVALID_PARAMETER);
    if(b->runtime->game->updating)return fail(b,ERROR_BUSY);
    if(mouse_registration(&current,&found)<0)return fail(b,GetLastError());
    replaced=!found || current.dwFlags!=0 || current.hwndTarget!=b->window;
    if(!replaced) {
        if(b->had_previous)restore=b->previous_mouse;
        if(!RegisterRawInputDevices(&restore,1,sizeof(restore)))return fail(b,GetLastError());
    }
    ssa_pc_input_runtime_focus(b->runtime,0);owner=NULL;memset(b,0,sizeof(*b));return replaced?1:0;
}
int ssa_win32_input_raw(SsaWin32Input *b,const void *data,size_t bytes) {
    RAWINPUTHEADER header;RAWMOUSE mouse;int result;
    const size_t needed=offsetof(RAWINPUT,data)+sizeof(mouse);
    if(!ready(b) || !data || bytes<sizeof(header))return fail(b,ERROR_INVALID_PARAMETER);
    memcpy(&header,data,sizeof(header));
    if(header.dwSize<sizeof(header) || header.dwSize>bytes)return fail(b,ERROR_INVALID_DATA);
    if(header.dwType!=RIM_TYPEMOUSE || GET_RAWINPUT_CODE_WPARAM(header.wParam)!=RIM_INPUT || !b->focused)return 0;
    if(header.dwSize<needed || bytes<needed)return fail(b,ERROR_INVALID_DATA);
    memcpy(&mouse,(const unsigned char *)data+offsetof(RAWINPUT,data),sizeof(mouse));
    if(!(mouse.usFlags&MOUSE_MOVE_ABSOLUTE)) {
        result=ssa_pc_input_runtime_relative(b->runtime,(int32_t)mouse.lLastX,(int32_t)mouse.lLastY);
        if(result<0)return fail(b,ERROR_INVALID_PARAMETER);
    }
    if(mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_DOWN)
        if(ssa_pc_input_runtime_button(b->runtime,SSA_PC_MOUSE_LEFT,1)<0)return fail(b,ERROR_INVALID_PARAMETER);
    if(mouse.usButtonFlags&RI_MOUSE_LEFT_BUTTON_UP)
        if(ssa_pc_input_runtime_button(b->runtime,SSA_PC_MOUSE_LEFT,0)<0)return fail(b,ERROR_INVALID_PARAMETER);
    return 1;
}
int ssa_win32_input_message(SsaWin32Input *b,HWND window,UINT message,
    WPARAM wparam,LPARAM lparam,LRESULT *out) {
    int result=0;
    if(!ready(b) || window!=b->window || !out)return fail(b,ERROR_INVALID_PARAMETER);
    *out=0;
    switch(message) {
    case WM_SETFOCUS:
        if(b->runtime->game->updating && (b->runtime->game->stage!=SSA_FRAME_POLL ||
           !b->runtime->game->external_active))return fail(b,ERROR_BUSY);
        b->focused=1;ssa_pc_input_runtime_focus(b->runtime,1);return 1;
    case WM_KILLFOCUS:
        if(b->runtime->game->updating && (b->runtime->game->stage!=SSA_FRAME_POLL ||
           !b->runtime->game->external_active))return fail(b,ERROR_BUSY);
        b->focused=0;ssa_pc_input_runtime_focus(b->runtime,0);return 1;
    case WM_KEYDOWN:case WM_KEYUP:
        if(wparam!='W' && wparam!='A' && wparam!='S' && wparam!='D' &&
           wparam!=VK_SPACE && wparam!='H' && wparam!='E')return 0;
        if(!b->focused)return 1;
        result=ssa_pc_input_runtime_key(b->runtime,(int)wparam,message==WM_KEYDOWN,
            message==WM_KEYDOWN && (((ULONG_PTR)lparam>>30)&1u)!=0);
        return result<0?fail(b,ERROR_INVALID_PARAMETER):1;
    case WM_INPUT:
        {
            UINT size=0,copied;void *data=NULL;
            copied=GetRawInputData((HRAWINPUT)lparam,RID_INPUT,NULL,&size,sizeof(RAWINPUTHEADER));
            if(copied==(UINT)-1)result=fail(b,GetLastError());
            else if(size<sizeof(RAWINPUTHEADER))result=fail(b,ERROR_INVALID_DATA);
            else if(!(data=malloc(size)))result=fail(b,ERROR_NOT_ENOUGH_MEMORY);
            else {
                UINT capacity=size;
                copied=GetRawInputData((HRAWINPUT)lparam,RID_INPUT,data,&capacity,sizeof(RAWINPUTHEADER));
                if(copied==(UINT)-1)result=fail(b,GetLastError());
                else if(copied!=size || capacity>size)result=fail(b,ERROR_INVALID_DATA);
                else if(GET_RAWINPUT_CODE_WPARAM(wparam)==RIM_INPUT)
                    result=ssa_win32_input_raw(b,data,size);
            }
            free(data);*out=DefWindowProcW(window,message,wparam,lparam);
            return result<0?-1:1;
        }
    default:return 0;
    }
}
