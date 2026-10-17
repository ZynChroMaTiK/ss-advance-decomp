#include "input_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static unsigned assertions,messages,raw_packets;
#define CHECK(x) do {++assertions;if(!(x)){fprintf(stderr,"check failed line%d: %s (Win32=%lu)\n",__LINE__,#x,(unsigned long)GetLastError());exit(1);}} while(0)
static SsaWin32Input backend;
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if(backend.attached && window==backend.window) {
        LRESULT result=0;int handled=ssa_win32_input_message(&backend,window,message,wparam,lparam,&result);
        ++messages;if(handled)return result;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static int get_mouse(RAWINPUTDEVICE *out) {
    RAWINPUTDEVICE list[16];UINT count=16,n=GetRegisteredRawInputDevices(list,&count,sizeof(*list)),i;
    CHECK(n!=(UINT)-1);
    for(i=0;i<n;++i)if(list[i].usUsagePage==1 && list[i].usUsage==2) {*out=list[i];return 1;}
    return 0;
}
static SsaPcInputSample sample(SsaPcInput *input) {
    SsaPcInputSample result;CHECK(ssa_pc_input_sample(input,&result)==1);return result;
}
static void key(HWND w,int code,int down,int repeat) {
    LPARAM bits=1;if(repeat)bits|=(LPARAM)((ULONG_PTR)1<<30);
    CHECK(PostMessageW(w,down?WM_KEYDOWN:WM_KEYUP,(WPARAM)code,bits));
    {
        MSG message;CHECK(PeekMessageW(&message,w,down?WM_KEYDOWN:WM_KEYUP,
            down?WM_KEYDOWN:WM_KEYUP,PM_REMOVE));DispatchMessageW(&message);
    }
}
static RAWINPUT mouse(LONG x,LONG y,USHORT buttons,USHORT flags) {
    RAWINPUT r;memset(&r,0,sizeof(r));r.header.dwType=RIM_TYPEMOUSE;
    r.header.dwSize=sizeof(r);r.header.wParam=RIM_INPUT;
    r.data.mouse.lLastX=x;r.data.mouse.lLastY=y;r.data.mouse.usButtonFlags=buttons;r.data.mouse.usFlags=flags;
    return r;
}
static int raw(RAWINPUT *r) {++raw_packets;return ssa_win32_input_raw(&backend,r,sizeof(*r));}
static DWORD WINAPI wrong_thread(void *unused) {
    LRESULT result;int status;(void)unused;
    status=ssa_win32_input_message(&backend,backend.window,WM_SETFOCUS,0,0,&result);
    return status==-1?0:1;
}
int main(void) {
    WNDCLASSW wc;HWND window,previous_window,capture;RECT clip_before,clip_after;
    RAWINPUTDEVICE original,prior,current,remove_mouse={1,2,RIDEV_REMOVE,NULL};int had_original;
    SsaPcInput input;SsaPcInputRuntime runtime;SsaPcInputSample s;SsaWin32Input second={0};
    SsaFrameGame frame_game={0};SsaFrame frame={0};SsaGameRuntime game={0};
    SsaEntityRuntime entities={0};SsaCombatRuntime combat={0};SsaPlayerGameplay gameplay={0};
    SsaPlayerRuntime player={0};SsaPlayerSpawn player_state={0};
    RAWINPUT r;LRESULT result;unsigned i;HANDLE thread;DWORD thread_result;
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(NULL);
    wc.lpszClassName=L"SSA native input backend test";CHECK(RegisterClassW(&wc));
    previous_window=CreateWindowExW(0,wc.lpszClassName,L"prior",0,0,0,0,0,HWND_MESSAGE,NULL,wc.hInstance,NULL);
    window=CreateWindowExW(0,wc.lpszClassName,L"test",0,0,0,0,0,HWND_MESSAGE,NULL,wc.hInstance,NULL);
    CHECK(window && previous_window && !IsWindowVisible(window));capture=GetCapture();CHECK(GetClipCursor(&clip_before));
    had_original=get_mouse(&original);prior=(RAWINPUTDEVICE){1,2,0,previous_window};
    CHECK(RegisterRawInputDevices(&prior,1,sizeof(prior)));
    CHECK(ssa_pc_input_init(&input,64)==1);memset(&runtime,0,sizeof(runtime));
    /* Minimal valid event-routing projection; no simulation step or renderer
     * is fabricated by this Windows-only fixture. Use real adapter init/events. */
    frame.game=&frame_game;game.frame=&frame;game.entities=&entities;
    entities.combat=&combat;combat.player_count=1;entities.players[0]=&gameplay;
    gameplay.runtime=&player;player.player=&player_state;player_state.motion.movement.flags=4;
    CHECK(ssa_pc_input_runtime_init(&runtime,&input,&game,0)==0);
    CHECK(ssa_win32_input_attach(NULL,window,&runtime)==-1);
    CHECK(ssa_win32_input_attach(&backend,NULL,&runtime)==-1);
    CHECK(ssa_win32_input_attach(&backend,window,&runtime)==0);
    CHECK(get_mouse(&current) && current.hwndTarget==window && current.dwFlags==0);
    CHECK(ssa_win32_input_attach(&second,previous_window,&runtime)==-1);
    CHECK(ssa_win32_input_attach(&backend,window,&runtime)==-1);
    CHECK(ssa_win32_input_message(&backend,previous_window,WM_SETFOCUS,0,0,&result)==-1);
    SendMessageW(window,WM_SETFOCUS,0,0);CHECK(input.focused && backend.focused);
    key(window,'W',1,0);key(window,'A',1,0);s=sample(&input);CHECK(s.keys==(0x40|0x200));
    key(window,'S',1,0);key(window,'D',1,0);s=sample(&input);CHECK(s.keys==0);
    key(window,'W',0,0);key(window,'A',0,0);s=sample(&input);CHECK(s.keys==(0x80|0x100));
    key(window,'S',0,0);key(window,'D',0,0);
    key(window,VK_SPACE,1,0);key(window,VK_SPACE,0,0);s=sample(&input);CHECK(s.jump_down);
    s=sample(&input);CHECK(!s.jump_down);
    key(window,'H',1,0);key(window,'H',1,1);s=sample(&input);CHECK(s.view_toggle);
    s=sample(&input);CHECK(!s.view_toggle);key(window,'H',0,0);
    key(window,'H',1,1);s=sample(&input);CHECK(!s.view_toggle);key(window,'H',0,0);
    key(window,'E',1,0);key(window,'E',0,0);s=sample(&input);CHECK(s.keys==0x10000);
    CHECK(ssa_win32_input_message(&backend,window,WM_KEYDOWN,VK_F1,0,&result)==0);
    CHECK(ssa_win32_input_message(&backend,window,WM_SYSKEYDOWN,'H',0,&result)==0);
    game.updating=1;game.stage=SSA_FRAME_RENDER_WORLD;
    CHECK(ssa_win32_input_message(&backend,window,WM_KILLFOCUS,0,0,&result)==-1);
    CHECK(backend.last_error==ERROR_BUSY && backend.focused && input.focused);
    CHECK(ssa_win32_input_detach(&backend)==-1 && backend.attached);
    game.updating=0;
    CHECK(ssa_win32_input_message(&backend,window,WM_LBUTTONDOWN,MK_LBUTTON,0,&result)==0);
    s=sample(&input);CHECK(!s.held && !s.pressed);
    r=mouse(10,-100,RI_MOUSE_LEFT_BUTTON_DOWN,0);CHECK(raw(&r)==1);s=sample(&input);
    CHECK(s.yaw_delta==640 && s.held==1 && s.pressed==1);
    CHECK(ssa_win32_input_message(&backend,window,WM_LBUTTONDOWN,MK_LBUTTON,0,&result)==0);
    s=sample(&input);CHECK(s.held==1 && !s.pressed && !s.yaw_delta);
    r=mouse(-5,1,RI_MOUSE_LEFT_BUTTON_UP,0);CHECK(raw(&r)==1);s=sample(&input);
    CHECK(s.yaw_delta==-320 && !s.held && !s.pressed);
    r=mouse(1000,2000,RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_LEFT_BUTTON_UP,MOUSE_MOVE_ABSOLUTE);
    CHECK(raw(&r)==1);s=sample(&input);CHECK(!s.yaw_delta && s.held==1 && s.pressed==1);
    s=sample(&input);CHECK(!s.held && !s.pressed);
    r=mouse(1,0,RI_MOUSE_WHEEL,0);r.data.mouse.usButtonData=120;CHECK(raw(&r)==1);
    s=sample(&input);CHECK(s.yaw_delta==64 && !s.held);
    r=mouse(999,999,RI_MOUSE_LEFT_BUTTON_DOWN,0);r.header.wParam=RIM_INPUTSINK;
    CHECK(raw(&r)==0);s=sample(&input);CHECK(!s.yaw_delta && !s.held);
    r.header.dwType=RIM_TYPEKEYBOARD;CHECK(raw(&r)==0);
    CHECK(ssa_win32_input_raw(&backend,NULL,0)==-1);
    r=mouse(123,0,0,0);CHECK(ssa_win32_input_raw(&backend,&r,sizeof(RAWINPUTHEADER)-1)==-1);
    r.header.dwSize=sizeof(r)+1;CHECK(raw(&r)==-1);
    r.header.dwSize=sizeof(RAWINPUTHEADER);CHECK(raw(&r)==-1);
    s=sample(&input);CHECK(!s.yaw_delta && !s.held);
    /* Invalid HRAWINPUT exercises GetRawInputData failure and DefWindowProc
     * cleanup routing, not a forged successful OS device event. */
    CHECK(ssa_win32_input_message(&backend,window,WM_INPUT,RIM_INPUT,0,&result)==-1);
    CHECK(backend.last_error!=0);
    key(window,'W',1,0);key(window,VK_SPACE,1,0);r=mouse(9,0,RI_MOUSE_LEFT_BUTTON_DOWN,0);CHECK(raw(&r)==1);
    SendMessageW(window,WM_KILLFOCUS,0,0);s=sample(&input);
    CHECK(!s.enabled && !s.keys && !s.jump_down && !s.held && !s.yaw_delta && !input.down);
    key(window,'W',1,0);CHECK(raw(&r)==0);SendMessageW(window,WM_SETFOCUS,0,0);
    s=sample(&input);CHECK(s.enabled && !s.keys && !s.held);
    for(i=0;i<1024;++i) {
        LONG dx=(LONG)i-512;r=mouse(dx,(LONG)i,0,0);CHECK(raw(&r)==1);
        s=sample(&input);CHECK(s.yaw_delta==dx*64 && !s.held);
    }
    r=mouse(LONG_MIN,LONG_MAX,0,0);CHECK(raw(&r)==1);s=sample(&input);CHECK(s.yaw_delta==0);
    thread=CreateThread(NULL,0,wrong_thread,NULL,0,NULL);CHECK(thread!=NULL);
    CHECK(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);CHECK(GetExitCodeThread(thread,&thread_result));
    CHECK(thread_result==0);CHECK(CloseHandle(thread));
    CHECK(ssa_win32_input_detach(&backend)==0);CHECK(!backend.attached && !input.focused);
    CHECK(get_mouse(&current) && current.hwndTarget==prior.hwndTarget && current.dwFlags==prior.dwFlags);
    CHECK(ssa_win32_input_detach(&backend)==-1);
    CHECK(ssa_win32_input_attach(&backend,window,&runtime)==0);
    CHECK(RegisterRawInputDevices(&prior,1,sizeof(prior)));
    CHECK(ssa_win32_input_detach(&backend)==1);CHECK(get_mouse(&current) && current.hwndTarget==previous_window);
    CHECK(RegisterRawInputDevices(had_original?&original:&remove_mouse,1,sizeof(original)));
    CHECK(GetCapture()==capture);CHECK(GetClipCursor(&clip_after));CHECK(EqualRect(&clip_before,&clip_after));
    CHECK(ssa_pc_input_runtime_free(&runtime)==0);
    CHECK(DestroyWindow(window));CHECK(DestroyWindow(previous_window));CHECK(UnregisterClassW(wc.lpszClassName,wc.hInstance));
    printf("{\"assertions\":%u,\"window_messages\":%u,\"raw_payloads\":%u,\"relative_cases\":1025,\"physical_device_test\":false,\"visible_window\":false}\n",assertions,messages,raw_packets);
    return 0;
}
