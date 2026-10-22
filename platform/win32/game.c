#include "ssa/weapon_order.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "vulkan_renderer.h"
#include "ssa/map_view.h"
#include "ssa/menu.h"
#include "ssa/campaign_screen.h"
#include "ssa/netricsa.h"
#include "ssa/native_statistics.h"
#include "ssa/intermission.h"
#include "ssa/run_exit.h"
#include "audio_win32.h"
#include "profile_win32.h"

typedef struct App {
    HWND window;SsaNativeGame *game;uint8_t *rom;size_t rom_size;
    unsigned level,captured,running,smoke,failed,reload,smoke_frames;
    unsigned advance;
    unsigned automap_keys;
    unsigned death_active,death_fade,death_black;
    int wheel_remainder;
    SsaNativeCarry entry;
    SsaCampaignScreen *story;
    SsaNetricsa *netricsa;
    SsaStatisticsTotals statistics;
    SsaStatisticsRows statistics_rows;
    unsigned net_active,net_pressed,net_held,net_counter,net_ticks,net_test,net_checks,net_data_checks,net_music_checks;
    unsigned net_prelevel,net_briefings,net_prelevel_ticks,net_overlay_checks,net_width,net_height;
    uint32_t *net_backdrop,*net_hud_frame,*net_pixels;
    unsigned net_hud_removed,net_weapon_pieces;
    unsigned story_active,story_pressed,story_held,story_requested,story_ticks,story_game_counter,story_music_checks;
    SsaMapView *map_view;
    unsigned map_active,map_fire,map_views,map_starts,map_wait_ticks,map_music_checks,level_music_checks;
    unsigned map_black_presents,map_fade_presents;
    unsigned map_black_hold,level_fade,level_revealing,transition_frame;
    unsigned map_hold_ticks,level_fade_ticks,level_fade_starts,level_fade_presents,reveal_weapon_checks;
    unsigned reveal_game_ticks,reveal_tick_base,reveal_move_checks,reveal_hud_checks,reveal_entry_checks;
    int32_t reveal_x,reveal_y;
    LARGE_INTEGER black_presented_at,reveal_presented_at;
    uint64_t black_hold_min_us,level_fade_min_us;
    unsigned transition_capture_mask;
    SsaMenu *menu;
    unsigned menu_active,menu_pressed,menu_wait_ticks,menu_starts,menu_last_page,menu_page_ticks,menu_game_counter;
    unsigned direct_level,language,difficulty,menu_notice_ticks;
    unsigned menu_test,menu_options_test,menu_toggle_count,menu_held_keys,menu_language_test;
    unsigned menu_credits_test,menu_credits_checks;
    unsigned menu_mouse_checks,menu_music_continuity;
    unsigned password_test,password_test_edges;
    unsigned level_select_test;
    unsigned level_starter_checks;
    SsaWin32Profile profile;
    SsaWin32Scores scores;
    SsaSettings settings;
    WINDOWPLACEMENT windowed_placement;
    LONG_PTR windowed_style;
    unsigned display_applied,window_mode,display_switching;
    wchar_t settings_path[32768];
    unsigned completion_recorded;
    SsaIntermission *intermission;
    SsaNativeCarry completed_carry;
    unsigned screen_active,screen_held,screen_pressed,screen_width,screen_height,tally_recorded,completion_setting;
    uint32_t *screen_backdrop,*screen_pixels;
    const char *menu_notice;
    unsigned render_width,render_height,original_renderer;
    unsigned smoke_resize,initial_width,initial_height;
    unsigned smoke_look;
    unsigned smoke_present_each;
    unsigned startup_test,startup_restarts;
    unsigned transition_test,transition_stage;
    SsaNativeCarry transition_expected;
    unsigned mute,no_music,audio_test,audio_test_sound;
    unsigned audio_test_worker,audio_test_m_key,audio_test_footsteps,audio_test_spatial,audio_test_music;
    unsigned audio_test_music_queued,audio_test_level_starts,audio_test_fade;
    SsaWin32Audio audio;
    SsaPcRenderer renderer;
    SsaVulkanRenderer *vulkan;
    unsigned vulkan_requested;
    HMODULE dwm;
    HRESULT (WINAPI *dwm_flush)(void);
    HRESULT (WINAPI *dwm_composition)(BOOL *);
    unsigned vsync_available;
    HDC back_dc;HBITMAP back_bitmap;HGDIOBJ back_original;
    uint32_t *back_pixels;BITMAPINFO back_info;
    int back_width,back_height;
    uint64_t presents;
    uint64_t presentation_samples;
    BITMAPINFO bitmap;
} App;
#include "game_vsync.inc"
static const wchar_t CLASS_NAME[]=L"SeriousSamAdvanceNativeGame";
static const wchar_t TITLE[]=L"Serious Sam Advance";
enum { MAP_BLACK_HOLD_TICKS=15, LEVEL_FADE_TICKS=60, PASSWORD_TEST_LEVELS=SSA_NATIVE_CAMPAIGN_LEVELS-1 };
static uint64_t elapsed_us(LARGE_INTEGER start)
{
    LARGE_INTEGER now,frequency;
    if(!start.QuadPart || !QueryPerformanceCounter(&now) || !QueryPerformanceFrequency(&frequency))return 0;
    return (uint64_t)(now.QuadPart-start.QuadPart)*1000000u/(uint64_t)frequency.QuadPart;
}
/* Audio diagnostics query independent instances rather than ROM route slots. */
static const SsaAudioPlayer *audio_test_latest(const SsaAudio *a,unsigned song)
{
    unsigned i;const SsaAudioPlayer *p=&a->players[0];
    for(i=1;i<SSA_AUDIO_PLAYERS;++i)if(a->players[i].song==song && a->players[i].serial>p->serial)p=&a->players[i];
    return p;
}
static int read_rom(App *a,const wchar_t *path)
{
    static const uint8_t expected[20]={0x4f,0xf4,0x8d,0x8c,0x18,0xb7,0x0a,0x51,0x84,0x75,
        0x8f,0x19,0x17,0x34,0x3a,0xe8,0x5a,0x70,0x85,0x8e};
    FILE *f;uint8_t digest[20];BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;int ok=0;
    f=_wfopen(path,L"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END) || ftell(f)!=0x800000){fclose(f);return 0;}
    rewind(f);a->rom=malloc(0x800000);a->rom_size=0x800000;
    if(!a->rom || fread(a->rom,1,a->rom_size,f)!=a->rom_size){fclose(f);return 0;}
    fclose(f);
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,NULL,0)<0)goto done;
    if(BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)<0)goto done;
    if(BCryptHashData(hash,a->rom,(ULONG)a->rom_size,0)<0)goto done;
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
    ok=memcmp(digest,expected,sizeof(expected))==0;
done:
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    return ok;
}
static void release(App *a)
{
    unsigned held=a->captured;a->captured=0;a->wheel_remainder=0;a->menu_held_keys=0;a->story_held=0;a->automap_keys=0;
    a->net_held=0;
    a->screen_held=0;
    ssa_pc_input_runtime_focus(&a->game->input_runtime,0);
    ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
    if(held){ClipCursor(NULL);SetCursor(LoadCursorW(NULL,MAKEINTRESOURCEW(32512)) /* IDC_ARROW, also in non-UNICODE test builds */);}
}
static int confine(App *a)
{
    RECT rect;POINT p[2];
    if(a->smoke || GetForegroundWindow()!=a->window || IsIconic(a->window) ||
       !GetClientRect(a->window,&rect) || rect.right<=0 || rect.bottom<=0)return 0;
    p[0].x=rect.left;p[0].y=rect.top;p[1].x=rect.right;p[1].y=rect.bottom;
    if(!ClientToScreen(a->window,&p[0]) || !ClientToScreen(a->window,&p[1]))return 0;
    rect.left=p[0].x;rect.top=p[0].y;rect.right=p[1].x;rect.bottom=p[1].y;
    return ClipCursor(&rect)!=0;
}
static void capture(App *a)
{
    if(a->captured)return;
    if(a->map_active || a->menu_active || a->story_active || a->net_active || a->screen_active || a->death_active ||
       a->game->players[0].inventory.health<=0)return;
    if(!confine(a))return;
    a->captured=1;
    SetCursor(NULL);ssa_pc_input_runtime_focus(&a->game->input_runtime,1);
    ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
}
static int fullscreen_bounds(App *a,HMONITOR monitor)
{
    MONITORINFO info={0};info.cbSize=sizeof(info);
    if(!monitor || !GetMonitorInfoW(monitor,&info))return 0;
    return SetWindowPos(a->window,HWND_NOTOPMOST,info.rcMonitor.left,info.rcMonitor.top,
        info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,
        SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOOWNERZORDER)!=0;
}
static int display_mode_apply(App *a,unsigned mode)
{
    LONG_PTR style,next;WINDOWPLACEMENT placement={0};unsigned ok;
    if(!a->window || mode>SSA_DISPLAY_WINDOWED)return 0;
    if(a->display_applied && a->window_mode==mode)return 1;
    if(!a->display_applied && mode==SSA_DISPLAY_WINDOWED){
        a->window_mode=mode;a->display_applied=1;return 1;}
    style=GetWindowLongPtrW(a->window,GWL_STYLE);
    if(mode==SSA_DISPLAY_FULLSCREEN){
        placement.length=sizeof(placement);
        if(!GetWindowPlacement(a->window,&placement))return 0;
        a->windowed_placement=placement;a->windowed_style=style;
        next=(style&~(LONG_PTR)WS_OVERLAPPEDWINDOW)|WS_POPUP;
    }else{
        placement=a->windowed_placement;
        if(placement.length!=sizeof(placement))return 0;
        /* Preserve current visibility, including hidden validation windows. */
        next=(a->windowed_style&~(LONG_PTR)WS_VISIBLE)|(style&WS_VISIBLE);
        if(!IsWindowVisible(a->window))placement.showCmd=SW_HIDE;
    }
    a->display_switching=1;SetLastError(0);
    if(!SetWindowLongPtrW(a->window,GWL_STYLE,next) && GetLastError()){
        a->display_switching=0;return 0;}
    if(mode==SSA_DISPLAY_FULLSCREEN)ok=fullscreen_bounds(a,MonitorFromWindow(a->window,MONITOR_DEFAULTTONEAREST));
    else ok=SetWindowPlacement(a->window,&placement) && SetWindowPos(a->window,NULL,0,0,0,0,
        SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    if(!ok){
        (void)SetWindowLongPtrW(a->window,GWL_STYLE,style);
        if(mode==SSA_DISPLAY_FULLSCREEN){placement=a->windowed_placement;
            if(!(style&WS_VISIBLE))placement.showCmd=SW_HIDE;
            (void)SetWindowPlacement(a->window,&placement);
        }else (void)fullscreen_bounds(a,MonitorFromWindow(a->window,MONITOR_DEFAULTTONEAREST));
    }else{a->window_mode=mode;a->display_applied=1;}
    a->display_switching=0;InvalidateRect(a->window,NULL,FALSE);return ok!=0;
}
static void display_toggle(App *a)
{
    unsigned mode=a->settings.display_mode^1u;
    if(!display_mode_apply(a,mode)){
        fprintf(stderr,"Could not toggle display mode (Windows error %lu).\n",(unsigned long)GetLastError());
        a->menu_notice="Display mode could not be changed.";a->menu_notice_ticks=300;return;
    }
    a->settings.display_mode=mode;
    if(a->menu)a->menu->settings.display_mode=mode;
    if(a->profile.enabled && !ssa_win32_settings_write(&a->settings,a->settings_path)){
        a->menu_notice="Could not save Settings. Changes apply for this session.";a->menu_notice_ticks=300;
    }
}
static int presentation_error(const char *stage)
{fprintf(stderr,"Presentation failed: %s (Windows error %lu).\n",stage,(unsigned long)GetLastError());return 0;}
static int snapshot_surface(const wchar_t *,const BITMAPINFO *,const uint32_t *);
static const uint32_t *display_pixels(const App *a)
{
    if(a->screen_active)return a->screen_pixels;
    if(a->net_active && !a->net_prelevel)return a->net_pixels;
    return !a->map_active && !a->menu_active && !a->story_active && !a->net_active &&
        !(a->game->players[0].motion.movement.flags&8) && a->renderer.pixels && !a->original_renderer?a->renderer.pixels:a->game->rgb888;
}
static int present(App *a,HDC window_dc)
{
    RECT rect;int w,h,ox,oy,ow,oh,n;HDC dc;
    int rw=a->bitmap.bmiHeader.biWidth,rh=-a->bitmap.bmiHeader.biHeight;
    const uint32_t *pixels=display_pixels(a);
    if(!GetClientRect(a->window,&rect))return presentation_error("client bounds");
    w=rect.right;h=rect.bottom;if(w<=0 || h<=0)return 1;
    if(!a->back_dc){a->back_dc=CreateCompatibleDC(window_dc);if(!a->back_dc)return presentation_error("memory DC");}
    if(a->back_width!=w || a->back_height!=h){
        HBITMAP bitmap;HGDIOBJ old;void *bits=NULL;BITMAPINFO info={0};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(window_dc,&info,DIB_RGB_COLORS,&bits,NULL,0);
        if(!bitmap)return presentation_error("back bitmap");old=SelectObject(a->back_dc,bitmap);
        if(!old || old==HGDI_ERROR){DeleteObject(bitmap);return presentation_error("select bitmap");}
        if(!a->back_original)a->back_original=old;
        if(a->back_bitmap)DeleteObject(a->back_bitmap);
        a->back_bitmap=bitmap;a->back_width=w;a->back_height=h;
        a->back_pixels=bits;a->back_info=info;
    }
    dc=a->back_dc;
    /* The black bars, complete frame and text are composed offscreen. Only
     * the finished image reaches the window, in one blit. */
    FillRect(dc,&rect,(HBRUSH)GetStockObject(BLACK_BRUSH));
    if(!ssa_ui_viewport(w,h,rw,rh,&ox,&oy,&ow,&oh))return presentation_error("viewport");
    SetStretchBltMode(dc,COLORONCOLOR);
    n=StretchDIBits(dc,ox,oy,ow,oh,0,0,rw,rh,pixels,&a->bitmap,DIB_RGB_COLORS,SRCCOPY);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    if(a->menu_active && a->menu_notice_ticks && a->menu_notice)
        TextOutA(dc,12,h-28,a->menu_notice,(int)strlen(a->menu_notice));
    if(n==0 || n==(int)GDI_ERROR)return presentation_error("stretch frame");
    if(!GdiFlush())return presentation_error("finish offscreen composition");
    if(a->startup_test && a->transition_test && a->map_views==1 && a->level_revealing){
        const wchar_t *directory=_wgetenv(L"SSA_TRANSITION_CAPTURE_DIR");
        unsigned bit=a->level_fade==LEVEL_FADE_TICKS?1:a->level_fade==LEVEL_FADE_TICKS/2?2:!a->level_fade?4:0;
        if(directory && bit && !(a->transition_capture_mask&bit)){wchar_t path[32768];
            int count=_snwprintf(path,32768,L"%ls\\%ls-%ls-source.bmp",directory,a->original_renderer?L"reference":L"pc",
                bit==1?L"black":bit==2?L"mid":L"full");
            if(count<0 || count>=32768 || !snapshot_surface(path,&a->back_info,a->back_pixels))return presentation_error("live transition source");
        }
    }
    /* Apply the reveal to the completed window image, including HUD/text and
     * bars. The PC/reference render buffers stay at their original brightness. */
    if(a->level_revealing){size_t i;unsigned gain=LEVEL_FADE_TICKS-a->level_fade;
        for(i=0;i<(size_t)w*(size_t)h;++i){unsigned c=a->back_pixels[i];
            a->back_pixels[i]=((((c>>16)&255)*gain/LEVEL_FADE_TICKS)<<16)|
                ((((c>>8)&255)*gain/LEVEL_FADE_TICKS)<<8)|((c&255)*gain/LEVEL_FADE_TICKS);
        }
    }
    if(a->smoke && rw==w && rh==h && w>200 && h>200){unsigned i;
        for(i=1;i<=3;++i){size_t at=(size_t)(h*(int)i/4)*(unsigned)w+(unsigned)(w*(int)i/4);
            if((a->back_pixels[at]&0xffffff)!=(pixels[at]&0xffffff))return presentation_error("offscreen pixel verification");
            ++a->presentation_samples;
        }}
    n=StretchDIBits(window_dc,0,0,w,h,0,0,w,h,a->back_pixels,&a->back_info,DIB_RGB_COLORS,SRCCOPY);
    if(n==0 || n==(int)GDI_ERROR)return presentation_error("finished frame blit");
    if(!GdiFlush())return presentation_error("display frame completion");
    if(a->settings.vsync && a->vsync_available && IsWindowVisible(a->window) && !IsIconic(a->window))
        presentation_sync_wait(a);
    if(a->net_active){unsigned weapons,enemies;
        if(ssa_netricsa_viewed(a->netricsa,&weapons,&enemies)!=1 ||
           ssa_native_game_netricsa_read(a->game,weapons,enemies)!=1)return presentation_error("NETRICSA read acknowledgement");}
    if(a->death_active && a->death_fade==0xf00)a->death_black=1;
    if(a->startup_test && a->transition_test && a->map_views==1 && a->level_revealing){
        const wchar_t *directory=_wgetenv(L"SSA_TRANSITION_CAPTURE_DIR");
        unsigned bit=a->level_fade==LEVEL_FADE_TICKS?1:a->level_fade==LEVEL_FADE_TICKS/2?2:!a->level_fade?4:0;
        if(directory && bit && !(a->transition_capture_mask&bit)){wchar_t path[32768];
            int path_chars=_snwprintf(path,32768,L"%ls\\%ls-%ls.bmp",directory,a->original_renderer?L"reference":L"pc",
                bit==1?L"black":bit==2?L"mid":L"full");
            if(path_chars<0 || path_chars>=32768 || !snapshot_surface(path,&a->back_info,a->back_pixels))return presentation_error("transition snapshot");
            a->transition_capture_mask|=bit;
        }
    }
    if(a->map_active && a->map_view->done && !a->black_presented_at.QuadPart)
        if(!QueryPerformanceCounter(&a->black_presented_at))return 0;
    if(a->level_revealing){
        if(a->level_fade==LEVEL_FADE_TICKS && !a->reveal_presented_at.QuadPart)
            if(!QueryPerformanceCounter(&a->reveal_presented_at))return 0;
        if(a->startup_test){
            if(a->game->frame_game.counter!=1+a->reveal_game_ticks-a->reveal_tick_base)
                return presentation_error("gameplay did not advance during level reveal");
            if(a->level_fade>0 && a->level_fade<LEVEL_FADE_TICKS)++a->level_fade_presents;
            if(a->level_fade==LEVEL_FADE_TICKS){size_t i;
                for(i=0;i<(size_t)w*(size_t)h;++i)if(a->back_pixels[i]&0xffffff)
                    return presentation_error("level reveal did not start on black");
            }
        }
    }
    if(a->startup_test && a->map_active && a->map_view->leaving){
        if(a->map_view->done){size_t i;
            for(i=0;i<(size_t)w*(size_t)h;++i)if(a->back_pixels[i]&0xffffff)
                return presentation_error("map fade did not finish on black");
            a->map_black_presents=a->map_views;
        }else if(a->map_view->fade_volume>0 && a->map_view->fade_volume<0xf00)++a->map_fade_presents;
    }
    ++a->presents;return 1;
}
static int render(App *a)
{
    RECT rect;unsigned w=a->render_width,h=a->render_height;
    if(a->screen_active){
        if(ssa_intermission_overlay(a->intermission,a->screen_backdrop,a->screen_width,a->screen_height,a->screen_pixels)!=1)return 0;
        a->bitmap.bmiHeader.biWidth=(LONG)a->screen_width;a->bitmap.bmiHeader.biHeight=-(LONG)a->screen_height;return 1;
    }
    if(a->net_active && !a->net_prelevel){
        if(ssa_netricsa_overlay(a->netricsa,a->net_backdrop,a->net_hud_frame,a->net_width,a->net_height,a->net_pixels)!=1)return 0;
        a->bitmap.bmiHeader.biWidth=(LONG)a->net_width;a->bitmap.bmiHeader.biHeight=-(LONG)a->net_height;return 1;
    }
    if(a->map_active || a->menu_active || a->story_active || a->net_active){unsigned i;
        if((a->net_active?ssa_netricsa_draw(a->netricsa,a->game->rgb555,SSA_DISPLAY_PIXELS):
            a->story_active?ssa_campaign_screen_draw(a->story,a->game->rgb555,SSA_DISPLAY_PIXELS):
            a->menu_active?ssa_menu_draw(a->menu,a->game->rgb555,SSA_DISPLAY_PIXELS):
            ssa_map_view_draw(a->map_view,a->game->rgb555,SSA_DISPLAY_PIXELS))!=1)return 0;
        for(i=0;i<SSA_DISPLAY_PIXELS;++i){unsigned c=a->game->rgb555[i];
            unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
            a->game->rgb888[i]=((r*255/31)<<16)|((g*255/31)<<8)|(b*255/31);
        }
        a->bitmap.bmiHeader.biWidth=240;a->bitmap.bmiHeader.biHeight=-160;return 1;
    }
    if(a->original_renderer || (a->game->players[0].motion.movement.flags&8)){a->bitmap.bmiHeader.biWidth=240;a->bitmap.bmiHeader.biHeight=-160;
        if(ssa_native_game_draw(a->game)!=1)return 0;
    }else{
    if(ssa_native_game_prepare_view(a->game)<0)return 0;
    if(!w){if(!GetClientRect(a->window,&rect))return 0;
        if(rect.right<=0 || rect.bottom<=0)return 1;
        w=(unsigned)rect.right;h=(unsigned)rect.bottom;}
    if(a->settings.aspect_ratio==SSA_ASPECT_GBA){int x,y,width,height;
        if(!ssa_ui_viewport((int)w,(int)h,240,160,&x,&y,&width,&height))return 0;
        w=(unsigned)width;h=(unsigned)height;
    }
    if(ssa_pc_renderer_resize(&a->renderer,w,h)<0){
        fprintf(stderr,"Could not allocate render resolution %ux%u.\n",w,h);return 0;}
    a->renderer.hardware=a->settings.renderer==SSA_RENDER_VULKAN?ssa_vulkan_backend(a->vulkan):NULL;
    if(ssa_pc_renderer_draw(&a->renderer,a->game)<0){
        if(a->renderer.hardware){
            fprintf(stderr,"Vulkan rendering failed; returning to software.\n");
            a->renderer.hardware=NULL;if(a->renderer.reflection)a->renderer.reflection->hardware=NULL;
            ssa_vulkan_close(a->vulkan);a->vulkan=NULL;a->settings.renderer=SSA_RENDER_SOFTWARE;
            if(a->menu){a->menu->settings.renderer=SSA_RENDER_SOFTWARE;a->menu->vulkan_available=0;}
            a->menu_notice="Vulkan unavailable; using Software.";a->menu_notice_ticks=300;
            if(ssa_pc_renderer_draw(&a->renderer,a->game)>0)goto rendered;
        }
        fprintf(stderr,"PC renderer rejected frame %lu.\n",(unsigned long)a->game->frame_game.counter);return 0;}
rendered:
    a->bitmap.bmiHeader.biWidth=(LONG)w;a->bitmap.bmiHeader.biHeight=-(LONG)h;
    }
    if(a->death_active){size_t i,count=(size_t)a->bitmap.bmiHeader.biWidth*(unsigned)-a->bitmap.bmiHeader.biHeight;
        uint32_t *pixels=a->original_renderer || (a->game->players[0].motion.movement.flags&8)?a->game->rgb888:a->renderer.pixels;
        unsigned darken=a->death_fade/256;
        for(i=0;i<count;++i){unsigned c=pixels[i],r=(c>>16)&255,g=(c>>8)&255,b=c&255;
            pixels[i]=a->death_fade==0xf00?0:((r-r*darken/16)<<16)|((g-g*darken/16)<<8)|(b-b*darken/16);}
    }
    return 1;
}
#include "game_screens.inc"
static int level_options(const App *a,SsaNativeGame *g)
{
    static const uint8_t continues[]={5,3,2};
    g->damage.difficulty=(uint8_t)a->difficulty;
    g->hud_notice.language=a->language;
    g->high_score=ssa_scores_valid(&a->scores.data)?a->scores.data.entries[0].score:0;
    if(ssa_native_game_apply_settings(g,&a->settings)!=1)return 0;
    g->gameplay.life_game.continue_flag=g->game.life_game.continue_flag=continues[a->difficulty];
    return 1;
}
static void save_change(App *a,unsigned action,unsigned value)
{
    int ok=ssa_win32_profile_store(&a->profile,action,value);
    if(action==SSA_SAVE_CLEAR && ssa_scores_valid(&a->scores.data)){
        if(!ssa_win32_scores_store(&a->scores,1,0,NULL))ok=0;
        if(a->menu)a->menu->scores=a->scores.data;
    }
    if(!ok){
        fprintf(stderr,"Could not save progress/settings (Windows error %u): %ls\n",a->profile.last_error,a->profile.path);
        a->menu_notice="Could not save data. Check the save file's folder permissions.";a->menu_notice_ticks=300;
    }
}
static void score_change(App *a)
{
    if(!a->menu->score_submitted)return;
    if(!ssa_win32_scores_store(&a->scores,0,a->menu->score_candidate,a->menu->scores.name)){
        a->scores.data=a->menu->scores;
        a->menu_notice="Could not save Scores. The record is kept for this session.";a->menu_notice_ticks=300;
        fprintf(stderr,"Could not save Scores (Windows error %u): %ls\n",a->scores.last_error,a->scores.path);
    }else{
        a->menu->scores=a->scores.data;
        a->menu->score_highlight=a->scores.last_rank<18?a->scores.last_rank+1:0;
    }
    a->menu->score_submitted=0;
}
static void settings_change(App *a)
{
    SsaSettings next=a->menu->settings;
    a->menu->vulkan_available=a->vulkan!=NULL;
    a->menu->vsync_available=a->vsync_available;
    if(next.renderer==SSA_RENDER_VULKAN && !a->vulkan){next.renderer=SSA_RENDER_SOFTWARE;a->menu->settings.renderer=next.renderer;}
    if(a->menu_pressed || a->menu->page!=SSA_MENU_SETTINGS){
        SsaWin32AudioStatus status=ssa_win32_audio_status(&a->audio);
        a->menu->soundfont_available=status.soundfont_available;a->menu->midi_available=status.midi_available;
    }
    if((next.music_style==SSA_MUSIC_SOUNDFONT && !a->menu->soundfont_available) ||
       (next.music_style==SSA_MUSIC_MIDI && !a->menu->midi_available)){
        next.music_style=SSA_MUSIC_ORIGINAL;a->menu->settings.music_style=next.music_style;}
    if(ssa_settings_equal(&next,&a->settings))return;
    if((next.music_style!=a->settings.music_style || next.music_volume!=a->settings.music_volume ||
        next.sound_volume!=a->settings.sound_volume) && !ssa_win32_audio_settings(&a->audio,&next)){
        a->menu->settings.music_style=a->settings.music_style;
        a->menu_notice="Music device could not be enabled.";a->menu_notice_ticks=300;return;}
    if(next.display_mode!=a->settings.display_mode && !display_mode_apply(a,next.display_mode)){
        next.display_mode=a->settings.display_mode;a->menu->settings.display_mode=next.display_mode;
        a->menu_notice="Display mode could not be changed.";a->menu_notice_ticks=300;}
    if(next.renderer!=a->settings.renderer || next.vulkan_motion_blur!=a->settings.vulkan_motion_blur)
        ++a->renderer.motion.epoch; /* Also reset if modes are toggled between scene draws. */
    a->settings=next;a->original_renderer=next.renderer==SSA_RENDER_GBA;
    if(ssa_native_game_apply_settings(a->game,&next)!=1){
        fprintf(stderr,"Could not apply runtime settings at frame%lu.\n",(unsigned long)a->game->frame_game.counter);
        a->failed=1;
    }
    if(!next.automap_enabled){a->automap_keys=0;a->wheel_remainder=0;}
    if(a->profile.enabled && !ssa_win32_settings_write(&next,a->settings_path)){
        a->menu_notice="Could not save Settings. Changes apply for this session.";a->menu_notice_ticks=300;}
}
static void screen_commands(App *a)
{
    unsigned i;
    for(i=0;i<a->intermission->command_count;++i){unsigned c=a->intermission->commands[i];
        if(c>=256)ssa_win32_audio_play(&a->audio,c-256);
        else ssa_win32_audio_menu_command(&a->audio,c,a->level,!a->no_music);
    }
}
static int begin_intermission(App *a,unsigned completion)
{
    size_t count;uint32_t *back,*pixels;
    if(a->screen_active || a->menu_active || a->story_active || a->map_active || a->net_active || a->level_revealing)return 0;
    if(!a->intermission){a->intermission=calloc(1,sizeof(*a->intermission));if(!a->intermission)return 0;}
    if(ssa_intermission_init(a->intermission,a->game,a->language,completion)!=1)return 0;
    {SsaSpriteOam saved=a->game->oam,filtered=saved,saved_layers[6];SsaSpriteDispatch sprites=a->game->sprites;
        SsaWeaponObject objects[SSA_SPRITE_CAPACITY];unsigned i;int ok;
        for(i=0;i<SSA_SPRITE_CAPACITY;++i){objects[i]=*sprites.objects[i];sprites.objects[i]=&objects[i];}
        sprites.active_count=0;
        /* Old pickup/intro notices must not remain in the frozen scene.
         * Completion owns its score/digits; pause retains the gameplay HUD. */
        if(!completion)for(i=0;i<a->game->sprites.active_count;++i){unsigned slot=a->game->sprites.active[i];
            if(objects[slot].type!=13 && objects[slot].type!=14)sprites.active[sprites.active_count++]=slot;}
        memcpy(saved_layers,a->game->original_hud_oam,sizeof saved_layers);
        ok=ssa_native_game_build_oam(a->game,&a->game->oam,&sprites)==1;filtered=a->game->oam;
        if(ok){unsigned suppressed=a->game->hud_suppressed;
            if(completion)a->game->hud_suppressed=1;
            a->game->oam=filtered;ok=render(a);a->game->oam=saved;
            a->game->hud_suppressed=suppressed;}
        a->game->oam=saved;memcpy(a->game->original_hud_oam,saved_layers,sizeof saved_layers);
        if(!ok)return 0;
    }
    a->screen_width=(unsigned)a->bitmap.bmiHeader.biWidth;a->screen_height=(unsigned)-a->bitmap.bmiHeader.biHeight;
    count=(size_t)a->screen_width*a->screen_height;
    if(!count || count>SIZE_MAX/sizeof(*back))return 0;
    back=malloc(count*sizeof(*back));pixels=malloc(count*sizeof(*pixels));
    if(!back || !pixels){free(back);free(pixels);return 0;}
    memcpy(back,display_pixels(a),count*sizeof(*back));
    free(a->screen_backdrop);free(a->screen_pixels);a->screen_backdrop=back;a->screen_pixels=pixels;
    release(a);a->screen_held=a->screen_pressed=0;a->screen_active=1;
    if(completion && a->audio.device && !ssa_win32_audio_screen(&a->audio,a->level,9))return 0;
    screen_commands(a);++a->transition_frame;return render(a);
}
static unsigned screen_completion_music_finished(App *a)
{
    if(!a->audio.device)return 1;
    /* Original02932 reads03006994: player0 from the ROM player table.
     * Song7 is a short PSG cue on player1, not a spoken Sam recording. */
    return !ssa_win32_audio_status(&a->audio).music_active;
}
static int finish_intermission(App *a)
{
    if(a->intermission->completion){SsaCompletionRecord record;unsigned i;
        if(ssa_native_completion_record(&record,&a->intermission->tally,a->game,&a->statistics,(uint8_t)a->completion_setting)!=1 ||
           ssa_native_game_carry_next(&a->completed_carry,a->game)!=1)return 0;
        a->completed_carry.score=record.totals.score;a->statistics=record.totals;a->tally_recorded=1;
        for(i=0;i<12;++i){a->statistics_rows.enemies[i].spawned+=record.checkpoint.enemies[i].spawned;
            a->statistics_rows.enemies[i].hits+=record.checkpoint.enemies[i].hits;
            a->statistics_rows.enemies[i].kills+=record.checkpoint.enemies[i].kills;}
        for(i=0;i<10;++i){a->statistics_rows.weapons[i].shots+=record.checkpoint.weapons[i].shots;
            a->statistics_rows.weapons[i].hits+=record.checkpoint.weapons[i].hits;}
    }
    a->screen_active=a->screen_pressed=a->screen_held=0;
    free(a->screen_backdrop);free(a->screen_pixels);a->screen_backdrop=a->screen_pixels=NULL;
    ++a->transition_frame;
    if(a->intermission->completion){
        if(a->level+1==SSA_NATIVE_CAMPAIGN_LEVELS)return begin_story(a,12);
        a->reload=a->advance=1;return 1;
    }
    if(a->intermission->pause.input_state==5){
        return begin_score_menu(a,a->game->score);
    }
    capture(a);
    return render(a);
}
static int intermission_tick(App *a)
{
    int active=ssa_intermission_step(a->intermission,a->screen_held?1u:0u,a->screen_pressed,screen_completion_music_finished(a));
    a->screen_pressed=0;if(active<0)return 0;
    screen_commands(a);++a->transition_frame;
    return active || finish_intermission(a);
}
static int begin_death_exit(App *a)
{
    if(a->game->game.life_game.input_state!=4 || a->death_active)return 0;
    release(a);a->death_active=1;a->death_fade=a->death_black=0;
    a->level_revealing=a->level_fade=0;
    /* Original08E74(7) fades music/effects; it is not sound7. */
    if(!a->no_music)ssa_win32_audio_fade(&a->audio,7);
    ++a->transition_frame;return 1;
}
static int death_tick(App *a)
{
    int destination;
    if(!a->death_active)return 0;
    if(a->reload)return 1;
    ++a->transition_frame;
    /* Original15698 clamps only after crossing0xF00. Present full black
     * before retiring the frozen level, even when catching up a slow frame. */
    if(a->death_fade<0xf00){a->death_fade+=0x40;return 1;}
    if(!a->death_black)return 1;
    destination=ssa_run_exit(&a->game->game.life_game,&a->game->frame_game.flags);
    if(destination<0)return 0;
    a->game->gameplay.life_game=a->game->game.life_game;
    /* The original terminal handoff consumes a life. Rebuild the level from
     * its entry snapshot while lives remain, without a restart prompt. */
    a->advance=0;
    if(destination==SSA_RUN_RETRY){a->reload=1;return 1;}
    a->death_active=0;
    a->reload=0;
    if(!begin_menu(a))return 0;
    a->menu->page=a->menu->draw_page=SSA_MENU_MAIN;
    if(ssa_menu_step(a->menu,0)!=1)return 0;
    ssa_win32_audio_menu_command(&a->audio,8,a->level,!a->no_music);
    return render(a);
}
static int game_tick(App *a)
{
    int result;
    if(a->death_active)return 1;
    /* Pause belongs to the frontend screens. A dead player still needs entity
     * updates to finish06154 and request its terminal fade; retaining bit1
     * here would freeze that scene indefinitely. Mode4 freezes it deliberately. */
    if(a->game->players[0].inventory.health<=0 && a->game->game.life_game.input_state!=4)
        a->game->frame_game.flags&=~1u;
    if(a->game->players[0].motion.movement.flags&8){
        if((a->automap_keys&1) && ssa_native_game_automap_zoom(a->game,1)<0)return -1;
        if((a->automap_keys&2) && ssa_native_game_automap_zoom(a->game,-1)<0)return -1;
    }
    if(!a->settings.thirdperson_enabled){
        a->game->input.view_toggle=0;a->game->player.view.third_person=0;
    }
    if(!a->settings.jump_enabled){
        a->game->input.down&=~(uint32_t)SSA_PC_SPACE;a->game->input.pressed&=~(uint32_t)SSA_PC_SPACE;
        a->game->input.jump_pressed=0;
    }
    result=ssa_native_game_tick(a->game);
    if(result==1 && a->window && a->game->game.life_game.input_state==4 && !a->death_active &&
       !a->smoke && !begin_death_exit(a))return -1;
    if(result==1 && a->game->finished && a->game->level_runtime.script.exit_toggle==1 &&
       a->game->players[0].inventory.health>0 && !a->completion_recorded){
        save_change(a,SSA_SAVE_COMPLETE,a->level);a->completion_recorded=1;
    }
    if(result==1 && a->window && a->game->finished && a->game->players[0].inventory.health>0 && !a->screen_active &&
       !a->tally_recorded && !a->level_revealing && !a->smoke && !begin_intermission(a,1))return -1;
    return result;
}
static int finish_menu(App *a)
{
    memset(&a->statistics,0,sizeof(a->statistics));memset(&a->statistics_rows,0,sizeof(a->statistics_rows));
    a->tally_recorded=a->completion_recorded=a->advance=a->reload=a->death_active=a->death_fade=a->death_black=0;
    a->language=a->menu->language;a->difficulty=a->menu->difficulty;a->no_music=!a->menu->music_enabled;
    {SsaNativeGame *next=calloc(1,sizeof(*next));
        if(!next)return 0;
        if(a->menu->level>=SSA_NATIVE_CAMPAIGN_LEVELS ||
           (a->menu->loadout?
            ssa_native_game_init_loadout(next,a->rom,a->rom_size,a->menu->level,a->menu->loadout):
            ssa_native_game_init_campaign(next,a->rom,a->rom_size,a->menu->level,a->difficulty,NULL))!=1){
            ssa_native_game_free(next);free(next);return 0;}
        if(ssa_native_game_alternate_weapons(next,a->settings.alternate_weapons,
            a->menu->loadout?SSA_ALTERNATE_LOADOUT:a->menu->level==0?SSA_ALTERNATE_NEW_GAME:SSA_ALTERNATE_RETAIN)!=1){
            ssa_native_game_free(next);free(next);return 0;}
        release(a);ssa_native_game_free(a->game);free(a->game);a->game=next;a->level=a->menu->level;
        ssa_pc_renderer_free(&a->renderer);
        if(a->audio.device){SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a->audio,
            ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
            if(ssa_native_game_bind_audio(a->game,&ops)<0)return 0;}
        if(ssa_native_game_carry_capture(&a->entry,a->game)!=1)return 0;
    }
    a->completion_setting=a->menu->loadout;
    if(!level_options(a,a->game))return 0;
    a->menu_active=0;a->menu_pressed=0;++a->menu_starts;
    return !a->menu->loadout && a->level==0?begin_story(a,0):begin_netricsa(a,1);
}
static int menu_frozen(const App *a)
{return a->game->frame_game.counter==a->menu_game_counter;}
static int menu_action(App *a)
{
    if(a->menu->quit_requested){release(a);a->running=0;return 1;}
    return !a->menu->done || finish_menu(a);
}
static int begin_level_reveal(App *a)
{
    /* Flush old screen/death audio before the first gameplay tick. Music and
     * non-music starts reset the sound fade and transient confirm effect. */
    if(a->audio.device && !ssa_win32_audio_begin_level(&a->audio,a->level)){
        fprintf(stderr,"Level audio start failed (%u); continuing silently.\n",a->audio.last_error);
        ssa_win32_audio_close(&a->audio);(void)ssa_native_game_bind_audio(a->game,NULL);
    }
    a->map_active=0;a->map_fire=0;
    a->completion_recorded=a->tally_recorded=0;
    a->reveal_presented_at.QuadPart=0;
    if(a->startup_test && a->audio.device){unsigned song;int valid;
        EnterCriticalSection(&a->audio.mutex);
        valid=ssa_audio_music_select(a->rom,a->rom_size,2,a->level,1,&song)==1 &&
            a->audio.mixer.master_gain==1 && !(a->audio.mixer.music.flags&12) &&
            (a->no_music?!a->audio.mixer.players[0].active:
             a->audio.mixer.music_command==2 && a->audio.mixer.players[0].active && a->audio.mixer.players[0].song==song);
        LeaveCriticalSection(&a->audio.mutex);
        if(!valid){fprintf(stderr,"Level music check failed on map%u, enabled%u.\n",a->level,!a->no_music);return 0;}
        ++a->level_music_checks;
    }
    if(a->startup_test && (a->transition_test || a->password_test || a->direct_level || a->level_select_test)){
        if(memcmp(&a->game->players[0].inventory,&a->entry.inventory,sizeof(SsaInventory)) ||
           a->game->score!=a->entry.score || a->game->gameplay.weapon.requested!=a->entry.weapon)return 0;
        ++a->reveal_entry_checks;
    }
    /* Reveal the complete first-person image from its first visible pixel.
     * Keep the weapon's ready position mirrors in
     * agreement before the first tick builds HUD/cache/OAM state. */
    a->game->weapon.object.y=0x5000;
    a->game->gameplay.weapon_action=0x5000;
    a->game->gameplay.weapon_view.y=0x5000;
    if(game_tick(a)!=1){fprintf(stderr,"Level bootstrap failed.\n");return 0;}
    /* Finish the normal sprite transfer queue before the first visible frame.
     * These are presentation passes, with no extra world or input ticks. */
    {unsigned pass;
        for(pass=0;pass<SSA_SPRITE_CAPACITY;++pass){
            unsigned pending=a->game->cache.read_cursor!=a->game->cache.write_cursor;
            if(ssa_sprite_cache_prepare(&a->game->cache,&a->game->sprites,&a->game->sprite_assets,0)<0 ||
               ssa_native_game_build_oam(a->game,&a->game->oam,&a->game->sprites)<0 ||
               ssa_sprite_cache_flush(&a->game->cache,&a->game->sprite_assets)<0)return 0;
            if(!pending && a->game->cache.read_cursor==a->game->cache.write_cursor)break;
        }
        if(pass==SSA_SPRITE_CAPACITY)return 0;
    }
    if(a->startup_test && !a->game->player.pose.third_person){
        const SsaWeaponObject *weapon=&a->game->weapon.object;
        unsigned image=weapon->previous_frame&0x3fffu;
        if(weapon->y!=0x5000 || weapon->screen_y!=80 || (weapon->flags&5)!=5 ||
           image>=SSA_SPRITE_IMAGE_COUNT || !a->game->cache.entries[image].resident){
            fprintf(stderr,"Weapon missing from initial level reveal.\n");return 0;}
        ++a->reveal_weapon_checks;
    }
    if(a->startup_test){unsigned i,visible=0;
        for(i=0;i<a->game->sprites.active_count;++i){const SsaWeaponObject *o=a->game->sprites.objects[a->game->sprites.active[i]];
            unsigned image=o->frame&0x3fffu;
            if(o->type!=2 && (o->flags&5)==5 && image<SSA_SPRITE_IMAGE_COUNT){
                if(!a->game->cache.entries[image].resident || o->previous_frame!=o->frame)return 0;
                ++visible;
            }
        }
        if(!visible){fprintf(stderr,"HUD missing on map%u at first reveal.\n",a->level);return 0;}
        ++a->reveal_hud_checks;
    }
    a->level_revealing=1;a->level_fade=LEVEL_FADE_TICKS;++a->level_fade_starts;++a->transition_frame;
    a->reveal_tick_base=a->reveal_game_ticks;
    a->reveal_x=a->game->players[0].entity.body.x;a->reveal_y=a->game->players[0].entity.body.y;
    if(a->startup_test && a->transition_test){
        ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
        SendMessageW(a->window,WM_KEYDOWN,'W',0);
    }else if(!a->startup_test)capture(a);
    if(!render(a))return 0;
    return 1;
}
static int finish_map(App *a)
{
    uint64_t duration=elapsed_us(a->black_presented_at);
    if(a->startup_test && a->map_black_presents!=a->map_views){
        fprintf(stderr,"Map handoff skipped the final black presentation.\n");return 0;}
    if(a->startup_test && duration<250000){fprintf(stderr,"Black hold was too short.\n");return 0;}
    if(duration && (!a->black_hold_min_us || duration<a->black_hold_min_us))a->black_hold_min_us=duration;
    ++a->map_starts;return begin_level_reveal(a);
}
static int reload(App *a)
{
    SsaNativeCarry carry=a->entry;SsaNativeGame *next;unsigned level=a->level,chapter=a->advance && a->level==5;
    unsigned retry=a->death_active && !a->advance;
    unsigned lives=a->game->frame_game.counter?a->game->game.life_game.continue_flag:a->game->gameplay.life_game.continue_flag;
    if(!a->advance && ssa_native_game_netricsa_entries(&carry.journal,a->game)!=1)return 0;
    if(a->advance){
        if(!a->game->finished || a->level+1>=SSA_NATIVE_CAMPAIGN_LEVELS || ssa_native_game_carry_next(&carry,a->game)!=1)return 0;
        if(a->tally_recorded)carry=a->completed_carry;
        level=a->level+1;
    }
    next=calloc(1,sizeof(*next));
    if(!next)return 0;
    if(ssa_native_game_init_campaign(next,a->rom,a->rom_size,level,a->difficulty,&carry)!=1){
        ssa_native_game_free(next);free(next);return 0;
    }
    if(a->advance && !a->tally_recorded){SsaLevelStatistics completed;unsigned i;
        if(ssa_native_game_statistics(&completed,a->game)!=1 || ssa_statistics_complete(&a->statistics,&completed)!=1){
            ssa_native_game_free(next);free(next);return 0;}
        for(i=0;i<12;++i){a->statistics_rows.enemies[i].spawned+=completed.rows.enemies[i].spawned;
            a->statistics_rows.enemies[i].hits+=completed.rows.enemies[i].hits;a->statistics_rows.enemies[i].kills+=completed.rows.enemies[i].kills;}
        for(i=0;i<10;++i){a->statistics_rows.weapons[i].shots+=completed.rows.weapons[i].shots;
            a->statistics_rows.weapons[i].hits+=completed.rows.weapons[i].hits;}
    }
    if(!level_options(a,next)){ssa_native_game_free(next);free(next);return 0;}
    next->gameplay.life_game.continue_flag=next->game.life_game.continue_flag=(uint8_t)lives;
    if(a->advance)a->completion_setting=0;
    release(a);ssa_native_game_free(a->game);free(a->game);a->game=next;a->level=level;a->entry=carry;
    a->death_active=a->death_fade=a->death_black=0;
    ssa_pc_renderer_free(&a->renderer);
    if(a->audio.device){SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a->audio,
        ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
        if(ssa_native_game_bind_audio(a->game,&ops)<0)return 0;
    }
    ssa_pc_input_runtime_focus(&a->game->input_runtime,0);ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
    a->reload=a->advance=0;
    if(retry){
        /* Death already presented black. Retry at the usual level reveal,
         * retaining the reduced lives and restored entry snapshot. */
        a->net_active=a->net_prelevel=a->map_active=a->story_active=0;
        return begin_level_reveal(a);
    }
    return chapter?begin_story(a,6):begin_netricsa(a,1);
}
/* Test-only shell transitions. Actual exits/combat are verified separately by
 * the normal-input campaign checker, without these inventory/death fixtures. */
static int transition_check(App *a,unsigned frames)
{
    unsigned cycle=a->transition_stage/4,phase=a->transition_stage%4;
    unsigned target=cycle+1,base=40+70*cycle;
    if(target>=SSA_NATIVE_CAMPAIGN_LEVELS)return 1;
    if(!phase && frames>=base){
        SsaInventory *inventory=&a->game->players[0].inventory;
        inventory->health=(int16_t)(target==1?61:52+10*target);
        inventory->weapons=7;inventory->ammo[1]=35;inventory->ammo[2]=(int16_t)(target==1?17:21+target);
        a->game->score=12345+11111*cycle;
        if(ssa_native_game_select_weapon(a->game,3)!=1 ||
           ssa_native_game_carry_capture(&a->transition_expected,a->game)!=1)return 0;
        /* The fixture ends before the requested handoff finishes. Original
         * advancement restores the equipped gun, not that pending request. */
        a->transition_expected.weapon=a->game->gameplay.weapon.current;
        a->game->game.life_game.input_state=3;++a->transition_stage;
        a->transition_expected.inventory.health=100;
        a->transition_expected.inventory.stat_c4=a->transition_expected.inventory.stat_c6=0;
        a->transition_expected.inventory.pending_count=0;
        memset(a->transition_expected.inventory.pending_weapons,0,sizeof a->transition_expected.inventory.pending_weapons);
    }else if(phase==1 && a->game->finished){
        if(a->screen_active){const SsaCompletion *tally=&a->intermission->tally;
            uint32_t seconds=tally->par_seconds-tally->elapsed/60,ammo=0;unsigned i;
            a->transition_expected.score=tally->source_score;
            if(seconds && seconds<0x80000000u)a->transition_expected.score+=seconds*1000*(tally->difficulty+1);
            for(i=0;i<8;++i)ammo+=(uint32_t)(int32_t)tally->ammo[i]*10u;
            ammo*=tally->difficulty+1;
            if(ammo && ammo<0x80000000u)a->transition_expected.score+=ammo*10*(tally->difficulty+1);
        }
        SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);++a->transition_stage;
    }else if((phase==2 && frames>=base+35) || (phase==3 && frames>=base+60)){
        if(a->level!=target || a->startup_restarts!=2*target-(phase==2?1u:0u) ||
           a->entry.score!=a->transition_expected.score || a->entry.weapon!=a->transition_expected.weapon ||
           memcmp(&a->entry.inventory,&a->transition_expected.inventory,sizeof(SsaInventory)) ||
           a->game->finished || a->game->error_line)return 0;
        if(phase==2){
            a->game->players[0].inventory.health=0;a->game->players[0].inventory.ammo[2]=0;a->game->score=99999;
            SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
        }
        ++a->transition_stage;
    }
    return 1;
}
static unsigned menu_pointer_input(App *a,int x,int y,unsigned click)
{
    unsigned before=a->menu->selection,input=ssa_menu_pointer(a->menu,x,y,click);
    /* Pointer updates happen before the next menu tick clears its command
     * queue. Deliver the same cycling cue as keyboard movement right here. */
    if(a->menu->selection!=before)ssa_win32_audio_menu_command(&a->audio,12,a->level,!a->no_music);
    return input;
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp)
{
    App *a=(App *)(uintptr_t)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(msg==WM_NCCREATE){a=((CREATESTRUCTW *)lp)->lpCreateParams;a->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)a);}
    if(!a)return DefWindowProcW(w,msg,wp,lp);
    switch(msg){
    case WM_CLOSE:release(a);DestroyWindow(w);return 0;
    case WM_DESTROY:release(a);a->running=0;PostQuitMessage(0);return 0;
    case WM_KILLFOCUS:case WM_ENTERSIZEMOVE:case WM_ENTERMENULOOP:case WM_CANCELMODE:release(a);break;
    case WM_SETFOCUS:case WM_EXITSIZEMOVE:case WM_EXITMENULOOP:
        if(!a->startup_test && !a->smoke)capture(a);break;
    case WM_DPICHANGED:{const RECT *bounds=(const RECT *)lp;
        if(!a->display_switching && a->display_applied && a->window_mode==SSA_DISPLAY_FULLSCREEN){
            (void)fullscreen_bounds(a,MonitorFromRect(bounds,MONITOR_DEFAULTTONEAREST));return 0;}
        SetWindowPos(w,NULL,bounds->left,bounds->top,bounds->right-bounds->left,bounds->bottom-bounds->top,
            SWP_NOACTIVATE|SWP_NOZORDER);return 0;}
    case WM_DWMCOMPOSITIONCHANGED:presentation_sync_refresh(a);return 0;
    case WM_DISPLAYCHANGE:
        presentation_sync_refresh(a);
        if(!a->display_switching && a->display_applied && a->window_mode==SSA_DISPLAY_FULLSCREEN)
            (void)fullscreen_bounds(a,MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST));
        InvalidateRect(w,NULL,FALSE);break;
    case WM_SIZE:case WM_MOVE:if(a->captured && !confine(a))release(a);InvalidateRect(w,NULL,FALSE);break;
    case WM_MOUSEMOVE:
        if(a->screen_active){RECT bounds;int x,y;
            if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,(int)a->screen_width,(int)a->screen_height,
                (short)LOWORD(lp),(short)HIWORD(lp),&x,&y) &&
               ssa_ui_pointer((int)a->screen_width,(int)a->screen_height,240,160,x,y,&x,&y)){
                unsigned previous=a->intermission->pause.selection;
                (void)ssa_intermission_pointer(a->intermission,x,y,0);
                if(previous!=a->intermission->pause.selection)ssa_win32_audio_menu_command(&a->audio,12,a->level,!a->no_music);
                ++a->transition_frame;
            }
        }else if(a->net_active){int x,y;
            if(netricsa_client_pointer(a,(short)LOWORD(lp),(short)HIWORD(lp),&x,&y))netricsa_pointer(a,x,y,0);
            else (void)ssa_netricsa_pointer(a->netricsa,-1,-1,0);
        }else if(a->menu_active){RECT bounds;int x,y;
            if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,240,160,
                (short)LOWORD(lp),(short)HIWORD(lp),&x,&y))
                (void)menu_pointer_input(a,x,y,0);
        }return 0;
    case WM_CHAR:
        if(a->menu_active && (a->menu->page==SSA_MENU_PASSWORD || a->menu->page==SSA_MENU_SCORE_NAME)){
            (void)ssa_menu_text(a->menu,(unsigned)wp);return 0;
        }break;
    case WM_LBUTTONDOWN:
        if(!a->smoke){if(a->screen_active){RECT bounds;int x,y;
                a->screen_held|=1;
                if(a->intermission->completion)a->screen_pressed|=1;
                else if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,(int)a->screen_width,(int)a->screen_height,
                    (short)LOWORD(lp),(short)HIWORD(lp),&x,&y) &&
                    ssa_ui_pointer((int)a->screen_width,(int)a->screen_height,240,160,x,y,&x,&y))
                    a->screen_pressed|=ssa_intermission_pointer(a->intermission,x,y,1);
            }else if(a->net_active){int x,y;
                if(netricsa_client_pointer(a,(short)LOWORD(lp),(short)HIWORD(lp),&x,&y))netricsa_pointer(a,x,y,1);
            }else if(a->story_active)a->story_pressed|=0x10000;
            else if(a->menu_active){RECT bounds;int x,y;
                if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,240,160,
                    (short)LOWORD(lp),(short)HIWORD(lp),&x,&y))
                    a->menu_pressed|=menu_pointer_input(a,x,y,1);
            }
            else if(a->map_active)a->map_fire=1;
            else if(a->captured)ssa_pc_input_runtime_button(&a->game->input_runtime,1,1);}return 0;
    case WM_LBUTTONUP:a->screen_held&=~1u;ssa_pc_input_runtime_button(&a->game->input_runtime,1,0);return 0;
    case WM_MOUSEWHEEL:
        if(a->net_active){int delta=a->wheel_remainder+(short)HIWORD(wp),steps=delta/WHEEL_DELTA;
            a->wheel_remainder=delta%WHEEL_DELTA;if(steps)netricsa_wheel(a,steps);return 0;}
        if(a->captured && !a->menu_active && !a->map_active &&
           !(a->game->frame_game.flags&1) && a->game->players[0].inventory.health>0 &&
           a->game->input.focused && a->game->input.enabled){
            int delta=a->wheel_remainder+(short)HIWORD(wp),steps=delta/WHEEL_DELTA;
            a->wheel_remainder=delta%WHEEL_DELTA;
            if(steps){if(a->game->players[0].motion.movement.flags&8)ssa_native_game_automap_zoom(a->game,steps);
                else ssa_native_game_cycle_weapon(a->game,-steps);}
        }else a->wheel_remainder=0;
        return 0;
    case WM_KEYDOWN:case WM_KEYUP:{
        int down=msg==WM_KEYDOWN,repeat=down && (lp&((LPARAM)1<<30))!=0;
        if(wp==VK_F11){if(down && !repeat && !a->display_switching)display_toggle(a);return 0;}
        if(a->screen_active){
            unsigned bit=wp==VK_RETURN?2u:wp==VK_SPACE?4u:0;
            if(down)a->screen_held|=bit;else a->screen_held&=~bit;
            if(down && !repeat){
                if(a->intermission->completion){if(bit)a->screen_pressed|=1;}
                else if(wp==VK_UP || wp=='W')a->screen_pressed|=0x400000;
                else if(wp==VK_DOWN || wp=='S')a->screen_pressed|=0x800000;
                else if(bit)a->screen_pressed|=0x10000;
                else if(wp==VK_ESCAPE || wp=='P'){
                    a->intermission->pause.selection=a->intermission->pause.selection>=2?3:0;
                    a->screen_pressed|=0x10000;
                }
            }return 0;
        }
        if(a->net_active){unsigned held=wp==VK_UP || wp=='W'?SSA_NET_SCROLL_UP:wp==VK_DOWN || wp=='S'?SSA_NET_SCROLL_DOWN:0;
            if(down)a->net_held|=held;else a->net_held&=~held;
            if(down && !repeat){
                if(wp==VK_ESCAPE || wp=='N' || wp==VK_TAB)a->net_pressed|=SSA_NET_CLOSE;
                else if(wp==VK_RETURN || wp==VK_SPACE)a->net_pressed|=SSA_NET_FOCUS;
                else if(wp==VK_UP || wp=='W')a->net_pressed|=SSA_NET_UP;
                else if(wp==VK_DOWN || wp=='S')a->net_pressed|=SSA_NET_DOWN;
                else if(wp==VK_LEFT || wp=='A')a->net_pressed|=SSA_NET_PREVIOUS;
                else if(wp==VK_RIGHT || wp=='D')a->net_pressed|=SSA_NET_NEXT;
            }return 0;
        }
        if(a->story_active){
            unsigned bit=wp==VK_UP || wp=='W'?1u:wp==VK_DOWN || wp=='S'?2u:wp==VK_RIGHT?4u:0;
            if(down)a->story_held|=bit;else a->story_held&=~bit;
            if(down && !repeat){
                if(wp==VK_RETURN || wp==VK_SPACE || wp==VK_ESCAPE)a->story_pressed|=0x10000;
            }
            return 0;
        }
        if(a->menu_active){
            int editing=a->menu && (a->menu->page==SSA_MENU_PASSWORD || a->menu->page==SSA_MENU_SCORE_NAME);
            unsigned key=wp==VK_UP?1u:!editing && wp=='W'?2u:wp==VK_DOWN?4u:!editing && wp=='S'?8u:0;
            if(down)a->menu_held_keys|=key;else a->menu_held_keys&=~key;
            if(editing && down && wp==VK_DELETE){(void)ssa_menu_text(a->menu,127);return 0;}
            if(editing && down && (wp==VK_HOME || wp==VK_END)){
                if(a->menu->page==SSA_MENU_SCORE_NAME && a->menu->score_typed)
                    a->menu->score_cursor=wp==VK_HOME?0:(uint8_t)a->menu->score_length;
                else if(a->menu->page==SSA_MENU_PASSWORD && a->menu->password_typed)
                    a->menu->password_cursor=wp==VK_HOME?0:(uint8_t)strlen(a->menu->password);
                return 0;}
            if(down && !repeat){
                if(wp==VK_UP || (!editing && wp=='W'))a->menu_pressed|=SSA_MENU_UP;
                else if(wp==VK_DOWN || (!editing && wp=='S'))a->menu_pressed|=SSA_MENU_DOWN;
                else if(wp==VK_LEFT || (!editing && wp=='A'))a->menu_pressed|=SSA_MENU_LEFT;
                else if(wp==VK_RIGHT || (!editing && wp=='D'))a->menu_pressed|=SSA_MENU_RIGHT;
                else if(wp==VK_RETURN || (!editing && wp==VK_SPACE))a->menu_pressed|=SSA_MENU_CONFIRM|SSA_MENU_START;
                else if(wp==VK_ESCAPE)a->menu_pressed|=SSA_MENU_BACK;
            }return 0;
        }
        if(down && !repeat && (wp=='N' || wp==VK_TAB) && !a->map_active && !a->level_revealing){
            if(a->settings.ingame_netricsa && !begin_netricsa(a,0))a->failed=1;return 0;}
        if(down && !repeat && (wp==VK_ESCAPE || wp=='P')){
            if(!a->map_active && !a->level_revealing && !a->game->finished && a->game->players[0].inventory.health>0){
                if(!begin_intermission(a,0))a->failed=1;
            }
            return 0;
        }
        if(wp=='M'){
            if(down && !repeat && a->settings.automap_enabled && a->captured && !a->map_active && !a->level_revealing &&
               !a->game->finished && a->game->players[0].inventory.health>0 && !(a->game->frame_game.flags&1)){
                if(ssa_native_game_automap(a->game,!(a->game->players[0].motion.movement.flags&8))<0)a->failed=1;
                a->automap_keys=0;a->wheel_remainder=0;++a->transition_frame;
                /* Keep pixel ownership and bitmap dimensions synchronized
                 * before a queued WM_PAINT can present this mode. */
                if(!a->failed && !render(a))a->failed=1;
            }return 0;
        }
        if(wp==VK_ADD || wp==VK_OEM_PLUS || wp==VK_SUBTRACT || wp==VK_OEM_MINUS){
            unsigned bit=(wp==VK_ADD || wp==VK_OEM_PLUS)?1u:2u;
            if(!down)a->automap_keys&=~bit;
            else if(a->captured && (a->game->players[0].motion.movement.flags&8) && !repeat)a->automap_keys|=bit;
            return 0;
        }
        if(a->map_active && down && !repeat && (wp==VK_RETURN || wp==VK_SPACE)){a->map_fire=1;return 0;}
        if(down && !repeat && wp==VK_RETURN && a->startup_test){
            if(wp==VK_RETURN && a->game->finished && a->level+1==SSA_NATIVE_CAMPAIGN_LEVELS){
                if(!begin_story(a,12))a->failed=1;return 0;
            }
            a->advance=wp==VK_RETURN && a->game->finished && a->level+1<SSA_NATIVE_CAMPAIGN_LEVELS;
            a->reload=1;return 0;
        }
        if((a->captured || (a->startup_test && a->transition_test && a->level_revealing)) && wp<128){
            if(down && !repeat && wp>='0' && wp<='9' && !(a->game->players[0].motion.movement.flags&8))
                ssa_native_game_select_weapon(a->game,ssa_weapon_order_id(wp=='0'?9:(unsigned)(wp-'1')));
            else if(down && !repeat && wp=='Q' && !a->map_active && !(a->game->players[0].motion.movement.flags&8) && !(a->game->frame_game.flags&1) &&
                    a->game->input.focused && a->game->input.enabled && a->game->players[0].inventory.health>0)
                ssa_native_game_previous_weapon(a->game);
            else ssa_pc_input_runtime_key(&a->game->input_runtime,(int)wp,down,repeat);
        }return 0;}
    case WM_INPUT:{
        RAWINPUT raw;UINT size=sizeof(raw),got=GetRawInputData((HRAWINPUT)lp,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER));
        if(got!=(UINT)-1 && got>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE) && raw.header.dwType==RIM_TYPEMOUSE &&
           !(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) && a->captured && GetForegroundWindow()==w)
            ssa_pc_input_runtime_relative(&a->game->input_runtime,(int32_t)raw.data.mouse.lLastX,
                a->original_renderer?0:(int32_t)raw.data.mouse.lLastY);
        return DefWindowProcW(w,msg,wp,lp);}
    case WM_SETCURSOR:if(a->captured && LOWORD(lp)==HTCLIENT){SetCursor(NULL);return TRUE;}break;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);if(!present(a,dc)){a->failed=1;a->running=0;}EndPaint(w,&ps);return 0;}
    default:break;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
static int snapshot_surface(const wchar_t *path,const BITMAPINFO *info,const uint32_t *pixels)
{
    BITMAPFILEHEADER h={0};FILE *f;int ok;size_t bytes;
    if(!path)return 1;
    bytes=(size_t)info->bmiHeader.biWidth*(size_t)(-info->bmiHeader.biHeight)*4;
    if(bytes>UINT32_MAX-sizeof(h)-sizeof(BITMAPINFOHEADER))return 0;
    f=_wfopen(path,L"wb");if(!f)return 0;h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(BITMAPINFOHEADER);
    h.bfSize=h.bfOffBits+(DWORD)bytes;
    ok=fwrite(&h,1,sizeof(h),f)==sizeof(h) && fwrite(&info->bmiHeader,1,sizeof(BITMAPINFOHEADER),f)==sizeof(BITMAPINFOHEADER) &&
       fwrite(pixels,1,bytes,f)==bytes;
    if(fclose(f))ok=0;return ok;
}
static int snapshot(App *a,const wchar_t *path)
{
    const uint32_t *pixels=display_pixels(a);
    return snapshot_surface(path,&a->bitmap,pixels);
}
#include "game_diagnostics.inc"
static int run(App *a)
{
    LARGE_INTEGER frequency,previous,now;double acc=0;MSG m;unsigned frames=0;
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&previous))return 0;
    if(a->smoke){ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);}
    else {
        ssa_pc_input_runtime_focus(&a->game->input_runtime,0);ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
        if(!(a->story_requested?begin_story(a,a->story_requested-1):a->direct_level?begin_netricsa(a,1):begin_menu(a)))return 0;
        ShowWindow(a->window,a->startup_test?SW_SHOWNOACTIVATE:SW_SHOW);UpdateWindow(a->window);
    }
    a->running=1;
    while(a->running){unsigned rendered_frame=frames,map_rendered_frame=a->map_view?a->map_view->frame:0,
        menu_rendered_frame=a->menu_wait_ticks,transition_rendered_frame=a->transition_frame;
        while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){if(m.message==WM_QUIT){a->running=0;break;}TranslateMessage(&m);DispatchMessageW(&m);}
        if(!a->running || a->failed)break;
        /* Gameplay owns mouse look automatically; no click-to-play mode. */
        if(!a->startup_test && !a->smoke && !a->captured)capture(a);
        if(a->reload){if(!reload(a))return 0;if(a->startup_test)++a->startup_restarts;}
        if(a->audio.device && !ssa_win32_audio_pump(&a->audio)){
            fprintf(stderr,"Audio output failed (%u); continuing silently.\n",a->audio.last_error);ssa_win32_audio_close(&a->audio);
            (void)ssa_native_game_bind_audio(a->game,NULL);
        }
        diagnostics_audio(a,frames);
        if(a->smoke_resize && frames==2 && !SetWindowPos(a->window,NULL,0,0,1024,768,
            SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER))return 0;
        if(a->smoke){
            unsigned n=frames%240;
            ssa_pc_input_runtime_key(&a->game->input_runtime,'W',n<30,0);
            ssa_pc_input_runtime_key(&a->game->input_runtime,' ',n<3,0);
            if(n==60 || n==180){ssa_pc_input_runtime_key(&a->game->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&a->game->input_runtime,'H',0,0);}
            ssa_pc_input_runtime_button(&a->game->input_runtime,1,n%30<15);
            if(a->smoke_look && !a->original_renderer && (n==10 || n==90 || n==150))
                ssa_pc_input_runtime_relative(&a->game->input_runtime,0,n==90?170:-85);
            acc=1.0/60.0;
        } else {
            if(!QueryPerformanceCounter(&now))return 0;
            acc+=(double)(now.QuadPart-previous.QuadPart)/(double)frequency.QuadPart;previous=now;
            if(acc>8.0/60.0)acc=8.0/60.0;
        }
        while(acc>=1.0/60.0){
            if(a->death_active){
                if(!death_tick(a))return 0;
            }else if(a->screen_active){
                /* Screen clock is60Hz even when the scene/render rate differs. */
                if(a->transition_test && a->intermission->completion){a->screen_held=1;a->screen_pressed=1;}
                if(!intermission_tick(a))return 0;
            }else if(a->net_active){uint32_t input;
                if(a->game->frame_game.counter!=a->net_counter)return 0;
                if(a->net_prelevel){
                    ++a->net_prelevel_ticks;
                    if(a->startup_test && a->net_ticks==65){
                        if(a->net_test){if(!netricsa_pointer_check(a,136,12,0) || !(a->net_pressed&SSA_NET_CLOSE))return 0;++a->net_checks;}
                        else SendMessageW(a->window,WM_KEYDOWN,VK_TAB,0);
                    }
                }else if(a->net_test){unsigned t=a->net_ticks;
                    if(t==30 || t==35)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                    if(t==31 || t==36)SendMessageW(a->window,WM_KEYUP,VK_RETURN,0);
                    if(t==40 || t==45)SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                    if(t==41 || t==46)SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);
                    if(t==50)SendMessageW(a->window,WM_KEYDOWN,VK_RIGHT,0);
                    if(t==51)SendMessageW(a->window,WM_KEYUP,VK_RIGHT,0);
                    if(t==60){unsigned expected=a->netricsa->has_enemies?1:2;
                        if(!netricsa_pointer_check(a,12,61,0) || a->netricsa->category!=expected){fprintf(stderr,"NETRICSA enemy pointer check failed.\n");return 0;}++a->net_checks;}
                    if(t==65){if(!netricsa_pointer_check(a,12,49,0) || a->netricsa->category!=2){fprintf(stderr,"NETRICSA weapon pointer check failed.\n");return 0;}++a->net_checks;}
                    if(t==68){if(!netricsa_pointer_check(a,92,20,0) || a->netricsa->weapon!=0)return 0;++a->net_checks;}
                    if(t==69){if(!netricsa_pointer_check(a,100,20,0) || a->netricsa->weapon!=1)return 0;++a->net_checks;}
                    if(t==70){if(!netricsa_pointer_check(a,130,40,-2) || a->netricsa->focus!=1 || !a->netricsa->scroll[0])return 0;++a->net_checks;}
                    if(t==75){unsigned j;SsaLevelStatistics snapshot;
                        if(ssa_native_game_statistics(&snapshot,a->game)!=1 || a->netricsa->data.level!=a->level ||
                           a->netricsa->data.totals.health!=snapshot.health || a->netricsa->data.totals.score!=snapshot.score)return 0;
                        for(j=0;j<10;++j)if(a->netricsa->data.rows.weapons[j].owned!=snapshot.rows.weapons[j].owned ||
                           a->netricsa->data.rows.weapons[j].ammo!=snapshot.rows.weapons[j].ammo)return 0;
                        ++a->net_data_checks;
                    }
                    if(t>=25 && t<95 && a->audio.device){int valid;unsigned song;
                        EnterCriticalSection(&a->audio.mutex);
                        valid=a->no_music?!a->audio.mixer.players[0].active:
                            ssa_audio_music_select(a->rom,a->rom_size,a->game->music_mode,a->level,1,&song)==1 &&
                            a->audio.mixer.music_command==a->game->music_mode && a->audio.mixer.players[0].song==song;
                        LeaveCriticalSection(&a->audio.mutex);if(!valid)return 0;++a->net_music_checks;
                    }
                    if(t==95){if(!netricsa_pointer_check(a,136,12,0) || !(a->net_pressed&SSA_NET_CLOSE))return 0;++a->net_checks;}
                }
                input=a->net_pressed|a->net_held;
                if(!netricsa_tick(a,input))return 0;
            }else if(a->story_active){unsigned i;uint32_t input;
                if(a->game->frame_game.counter!=a->story_game_counter)return 0;
                if(!diagnostics_story_input(a))return 0;
                input=a->story_pressed|(a->story_held&1?0x40u:0u)|(a->story_held&2?0x80u:0u)|(a->story_held&4?0x800000u:0u);
                if(ssa_campaign_screen_step(a->story,input)<0)return 0;
                a->story_pressed=0;++a->story_ticks;++a->transition_frame;
                for(i=0;i<a->story->command_count;++i)
                    ssa_win32_audio_menu_command(&a->audio,a->story->commands[i],a->story->next_level,!a->no_music);
                for(i=0;i<a->story->fade_count;++i)ssa_win32_audio_fade(&a->audio,a->story->fade_intervals[i]);
                if(a->story->done){
                    if(!finish_story(a))return 0;
                    if(a->startup_test && a->story_requested){
                        if(a->game->frame_game.counter!=a->story_game_counter || a->captured ||
                           a->audio.mixer.errors || a->story_ticks<200 || !a->presents)return 0;
                        printf("{\"result\":\"pass\",\"story\":%u,\"story_ticks\":%u,\"frozen_game_counter\":%u,"
                            "\"music_checks\":%u,\"audio_device\":%u,\"audio_errors\":%u,\"map_active\":%u,\"menu_active\":%u,\"briefing_active\":%u}\n",
                            a->story_requested-1,a->story_ticks,a->game->frame_game.counter,a->story_music_checks,
                            a->audio.device?1u:0u,a->audio.mixer.errors,a->map_active,a->menu_active,a->net_active && a->net_prelevel);
                        return 1;
                    }
                }
            }else if(a->menu_active){unsigned i;
                if(!menu_frozen(a)){fprintf(stderr,"Gameplay advanced during menu.\n");return 0;}
                ++a->menu_wait_ticks;
                if(a->menu_notice_ticks)--a->menu_notice_ticks;
                if(a->menu_last_page!=a->menu->page){a->menu_last_page=a->menu->page;a->menu_page_ticks=0;
                    if(a->menu->page==SSA_MENU_PASSWORD)a->menu_held_keys=0;}
                if(!diagnostics_menu_input(a))return 0;
                {unsigned previous_music=a->menu->music_enabled,previous_language=a->menu->language,p=a->menu->page,t=a->menu_page_ticks;
                    unsigned held=(a->menu_held_keys&3?SSA_MENU_UP:0u)|(a->menu_held_keys&12?SSA_MENU_DOWN:0u);
                    int32_t old_y=a->menu->credits_y;
                    if(ssa_menu_step_input(a->menu,a->menu_pressed,held)!=1)return 0;
                    score_change(a);
                    settings_change(a);
                    if(a->menu->clear_requested)save_change(a,SSA_SAVE_CLEAR,0);
                    else if(previous_language!=a->menu->language)save_change(a,SSA_SAVE_LANGUAGE,a->menu->language);
                    if(previous_music!=a->menu->music_enabled)++a->menu_toggle_count;
                    if(a->startup_test && a->menu_test && p==SSA_MENU_CREDITS){
                        if(t<70){int32_t speed=t>=2 && t<6?0:t>=6 && t<10?0x20000:0x8000;
                            if(a->menu->credits_speed!=speed || a->menu->credits_y!=(t?old_y:160*65536)-speed)return 0;
                        }else if(t==70){if(a->menu->credits_y!=old_y || !a->menu->credits_done)return 0;}
                        else{if(a->menu->page!=SSA_MENU_OPTIONS || a->menu->selection!=4)return 0;a->menu_credits_test=2;}
                        ++a->menu_credits_checks;
                    }
                }
                a->menu_pressed=0;++a->menu_page_ticks;a->no_music=!a->menu->music_enabled;
                for(i=0;i<a->menu->command_count;++i){
                    if(a->startup_test && a->audio.synchronized && a->menu->commands[i]==8){
                        SsaAudioPlayer previous_song;SsaAudioMusicState state;unsigned continuing;
                        EnterCriticalSection(&a->audio.mutex);previous_song=a->audio.mixer.players[0];state=a->audio.mixer.music;
                        continuing=!a->no_music && a->audio.mixer.music_command==8 && previous_song.active && previous_song.song==0x6f;
                        ssa_win32_audio_menu_command(&a->audio,8,a->level,!a->no_music);
                        if(continuing){
                            if(memcmp(&previous_song,&a->audio.mixer.players[0],sizeof(previous_song)) ||
                               memcmp(&state,&a->audio.mixer.music,sizeof(state))){
                                LeaveCriticalSection(&a->audio.mutex);fprintf(stderr,"Menu music restarted.\n");return 0;}
                            ++a->menu_music_continuity;
                        }
                        LeaveCriticalSection(&a->audio.mutex);
                    }else ssa_win32_audio_menu_command(&a->audio,a->menu->commands[i],a->level,!a->no_music);
                }
                if(a->menu->fade_audio)ssa_win32_audio_fade(&a->audio,5);
                if(a->menu->unsupported){
                    a->menu_notice=a->menu->unsupported==3?"Multiplayer is not available in this build.":
                        a->menu->unsupported==13?"The scores screen is not available in this build.":
                        a->menu->unsupported>=SSA_MENU_UNAVAILABLE_LEVEL?"This level is not yet available in this build.":
                        "This menu is not available in this build.";
                    a->menu_notice_ticks=180;
                }
                if(!menu_action(a))return 0;
                if(!a->running)return 1;
            }else if(a->map_active && a->map_view->done){
                if(a->map_black_hold){--a->map_black_hold;++a->map_hold_ticks;++a->transition_frame;}
                else if(!finish_map(a))return 0;
            }else if(a->map_active){int first;
                if(a->game->frame_game.counter){fprintf(stderr,"Gameplay advanced during map view.\n");return 0;}
                ++a->map_wait_ticks;
                if(a->startup_test && a->audio.device && !a->map_view->leaving){unsigned song;int valid;
                    EnterCriticalSection(&a->audio.mutex);
                    valid=ssa_audio_music_select(a->rom,a->rom_size,3,a->level,1,&song)==1 &&
                        (a->no_music?!a->audio.mixer.players[0].active:a->audio.mixer.music_command==3 &&
                        a->audio.mixer.players[0].song==song);
                    LeaveCriticalSection(&a->audio.mutex);
                    if(!valid){fprintf(stderr,"Map music check failed on map%u, enabled%u.\n",a->level,!a->no_music);return 0;}
                    ++a->map_music_checks;
                }
                /* Automated normal-window tests press the same mapped fire
                 * key and wait the complete original fade. No game ticks skip. */
                if(a->startup_test && a->map_view->frame==8)SendMessageW(a->window,WM_KEYDOWN,VK_SPACE,0);
                first=ssa_map_view_step(a->map_view,a->map_fire);a->map_fire=0;
                if(first<0)return 0;
                if(first)ssa_win32_audio_map_skip(&a->audio);
            }else if(a->level_revealing){
                /* Retain the normal simulation clock even when rendering a
                 * fade frame takes longer than one tick. Present every fade
                 * step separately while catching up world/input updates. */
                while(acc>=2.0/60.0){
                    if(game_tick(a)!=1)return 0;
                    ++a->reveal_game_ticks;acc-=1.0/60.0;
                }
                if(game_tick(a)!=1)return 0;
                ++a->reveal_game_ticks;
                /* The simulation clock can include time spent preparing the
                 * first frame. Fade timing starts after black was displayed. */
                if(!a->death_active && a->reveal_presented_at.QuadPart && elapsed_us(a->reveal_presented_at)>=
                   (uint64_t)(LEVEL_FADE_TICKS-a->level_fade+1)*1000000/LEVEL_FADE_TICKS){
                    --a->level_fade;++a->level_fade_ticks;
                    if(a->startup_test && a->transition_test && a->level_fade==LEVEL_FADE_TICKS-6){
                        SendMessageW(a->window,WM_KEYUP,'W',0);
                        if(a->game->players[0].entity.body.x==a->reveal_x && a->game->players[0].entity.body.y==a->reveal_y){
                            fprintf(stderr,"Movement blocked during level reveal.\n");return 0;}
                        ++a->reveal_move_checks;
                    }
                }
                ++a->transition_frame;
            }else{
                if(a->net_test && !a->net_overlay_checks && frames==3){
                    SendMessageW(a->window,WM_KEYDOWN,VK_TAB,0);
                    if(!a->net_active || a->net_prelevel || !a->net_backdrop || !a->net_pixels)return 0;
                    ++a->net_overlay_checks;continue;
                }
                if(game_tick(a)!=1){fprintf(stderr,"Native frame failed at%u, line%u\n",frames,a->game->error_line);return 0;}
                ++frames;
                if(a->net_test && a->net_ticks && frames>=6){
                    if(a->net_active || a->net_ticks!=110 || a->net_checks!=7 || a->net_data_checks!=1 || a->net_briefings!=1 || a->net_prelevel_ticks!=127 ||
                       !a->net_hud_removed || !a->net_weapon_pieces ||
                       a->game->frame_game.counter<=a->net_counter || a->audio.mixer.errors)return 0;
                    printf("{\"result\":\"pass\",\"netricsa_ticks\":%u,\"pointer_checks\":%u,\"snapshot_checks\":%u,\"music_checks\":%u,\"frozen_game_counter\":%u,\"resumed_game_counter\":%u,\"audio_errors\":%u,\"briefings\":%u,\"briefing_ticks\":%u,\"overlay_checks\":%u,\"overlay_width\":%u,\"overlay_height\":%u,\"hud_pixels_removed\":%u,\"weapon_pieces\":%u}\n",
                        a->net_ticks,a->net_checks,a->net_data_checks,a->net_music_checks,a->net_counter,a->game->frame_game.counter,a->audio.mixer.errors,
                        a->net_briefings,a->net_prelevel_ticks,a->net_overlay_checks,a->net_width,a->net_height,a->net_hud_removed,a->net_weapon_pieces);return 1;
                }
            }
            acc-=1.0/60.0;
            /* Present terminal black while the map still owns the display.
             * Drop catch-up time at this boundary: level initialization must
             * wait for the next presentation tick, even after a slow frame. */
            if(a->map_active && a->map_view->leaving){acc=0;break;}
            if(a->transition_frame!=transition_rendered_frame)break;
        }
        if(frames!=rendered_frame || (a->map_active && a->map_view->frame!=map_rendered_frame) ||
            (a->menu_active && a->menu_wait_ticks!=menu_rendered_frame) || a->transition_frame!=transition_rendered_frame){
            HDC dc;int shown;
            if(!render(a))return 0;
            if(a->net_test && a->net_active && a->net_ticks==(a->net_prelevel?64u:80u)){
                const wchar_t *directory=_wgetenv(L"SSA_NETRICSA_CAPTURE_DIR");
                if(directory && *directory){wchar_t path[32768];
                    if(swprintf(path,32768,L"%ls/netricsa-%u-%ls.bmp",directory,a->level,a->net_prelevel?L"briefing":L"overlay")<0 ||
                       !snapshot(a,path))return 0;
                }
            }
            /* Present this batch now; queued paints redraw a complete frame. */
            if(!a->smoke || a->smoke_present_each){dc=GetDC(a->window);if(!dc)return 0;
                shown=present(a,dc);ReleaseDC(a->window,dc);if(!shown)return 0;ValidateRect(a->window,NULL);}
            /* Count the black interval from its completed presentation, not
             * from before drawing it. Slow composition cannot consume it. */
            if(a->map_active && a->map_view->leaving){
                if(!QueryPerformanceCounter(&previous))return 0;acc=0;}
            if(a->level_revealing && !a->level_fade){uint64_t duration=elapsed_us(a->reveal_presented_at);
                if(a->startup_test && duration<1000000){fprintf(stderr,"Level reveal was too short.\n");return 0;}
                if(duration && (!a->level_fade_min_us || duration<a->level_fade_min_us))a->level_fade_min_us=duration;
                a->level_revealing=0;
            }
        }
        if(!a->initial_width){a->initial_width=(unsigned)a->bitmap.bmiHeader.biWidth;a->initial_height=(unsigned)(-a->bitmap.bmiHeader.biHeight);}
        if(a->smoke && frames==a->smoke_frames){
            HDC dc=GetDC(a->window);int shown;
            if(!dc)return 0;shown=present(a,dc);ReleaseDC(a->window,dc);if(!shown)return 0;
            printf("{\"result\":\"pass\",\"frames\":%u,\"health\":%d,\"entities\":%u,\"score\":%u,\"hidden_window\":true,"
                "\"render_width\":%ld,\"render_height\":%ld,\"world_rays\":%llu,\"initial_width\":%u,\"initial_height\":%u,\"pitch\":%ld,\"buffered_presents\":%llu,\"presentation_samples\":%llu}\n",
                frames,a->game->players[0].inventory.health,a->game->spawn.pool.active_count,a->game->score,
                a->bitmap.bmiHeader.biWidth,-a->bitmap.bmiHeader.biHeight,(unsigned long long)a->renderer.rays,
                a->initial_width,a->initial_height,(long)a->game->player.pitch,(unsigned long long)a->presents,
                (unsigned long long)a->presentation_samples);return 1;
        }
        if(a->transition_test && !transition_check(a,frames))return 0;
        if(a->startup_test && !a->transition_test && frames>=60 && !a->startup_restarts)a->reload=1;
        if(a->startup_test && frames>=(a->transition_test?50u+70u*(SSA_NATIVE_CAMPAIGN_LEVELS-1):120u)){
            if(a->direct_level || a->level_select_test){unsigned j,owned=1,selected=1,setting=a->difficulty*12+a->level+1;
                if(a->entry.inventory.ammo[0]!=999 || a->entry.inventory.pending_count)return 0;
                for(j=1;j<10;++j){const uint8_t *p=a->rom+0x44bcf0+setting*20+(j-1)*2;
                    int ammo=(int16_t)((unsigned)p[0]|(unsigned)p[1]<<8),cap=(int16_t)a->game->inventory_rules.ammo_capacity[j];
                    if(a->entry.inventory.ammo[j]!=(ammo && ammo>cap?cap:ammo))return 0;
                    if(ammo){owned|=1u<<j;selected=j+1;}
                }
                if(a->entry.inventory.weapons!=owned || a->entry.weapon!=selected)return 0;
                ++a->level_starter_checks;
            }
            if(a->audio_test || a->transition_test)ssa_win32_audio_quiesce(&a->audio);
            if(!a->presents || !IsWindowVisible(a->window) || a->startup_restarts!=(a->transition_test?2u*(SSA_NATIVE_CAMPAIGN_LEVELS-1):1u) || a->reload)return 0;
            if(a->menu_active || a->menu_starts!=(a->direct_level?0u:1u) ||
               (!a->password_test && !a->level_select_test && a->menu_wait_ticks!=(a->direct_level?0u:a->menu_test?290u+2*((a->menu_test-1)/3):187u)) ||
               a->difficulty!=(a->menu_test?(a->menu_test-1)%3:a->password_test?(a->password_test-1)/PASSWORD_TEST_LEVELS:1) ||
               a->language!=(a->menu_test?(a->menu_test-1)/3:a->password_test?(a->password_test-1)%3:a->profile.data.language) ||
               (a->level_select_test && (a->level!=a->level_select_test-1 ||
                a->menu->loadout!=a->difficulty*12+a->level+1 || ssa_profile_unlocked(&a->profile.data,a->level)!=1)) ||
               a->game->damage.difficulty!=a->difficulty ||
               (a->password_test && (a->level!=(a->password_test-1)%PASSWORD_TEST_LEVELS+1 || !a->password_test_edges || !a->audio.device ||
                a->menu->loadout!=((a->password_test-1)/PASSWORD_TEST_LEVELS)*12+(a->password_test-1)%PASSWORD_TEST_LEVELS+2 ||
                a->level_music_checks!=a->map_views)) ||
               (a->menu_test && (a->menu_toggle_count || a->menu_credits_checks!=72 || a->menu_mouse_checks!=2 ||
                a->menu_music_continuity!=(a->no_music?0u:2u) || !a->audio.device || a->level_music_checks!=a->map_views)) ||
               a->map_active || a->map_views!=a->startup_restarts+1 || a->map_starts!=a->map_views ||
               a->map_black_presents!=a->map_views || a->map_fade_presents<a->map_views ||
               a->level_revealing || a->map_hold_ticks!=MAP_BLACK_HOLD_TICKS*a->map_views ||
               a->level_fade_ticks!=LEVEL_FADE_TICKS*a->map_views || a->level_fade_starts!=a->map_views ||
               a->level_fade_presents<a->map_views || a->black_hold_min_us<250000 || a->level_fade_min_us<1000000 ||
               a->map_wait_ticks!=70*a->map_views ||
               ((a->audio_test || a->transition_test) &&
                (a->map_music_checks!=9*a->map_views || a->level_music_checks!=a->map_views)))return 0;
            if(a->transition_test && (a->transition_stage!=4u*(SSA_NATIVE_CAMPAIGN_LEVELS-1) || !a->audio.device || a->audio.mixer.errors ||
               a->audio.mixer.master_gain!=1 || (a->no_music?a->audio.mixer.players[0].active:
               !a->audio.mixer.players[0].active || a->audio.mixer.music_command!=2)))return 0;
            if(a->audio_test && (!a->audio.device || !a->audio.recycled || !ssa_win32_audio_position(&a->audio) ||
                a->audio.mixer.errors || !a->audio_test_worker || !a->audio_test_m_key ||
                a->audio_test_footsteps!=2 || a->audio_test_spatial!=2 || a->audio_test_music!=3 || a->audio_test_level_starts!=2 || a->audio_test_fade!=2))return 0;
            printf("{\"result\":\"pass\",\"startup_path\":\"normal\",\"frames\":%u,\"raw_input_registered\":true,"
                "\"buffered_presents\":%llu,\"restarts\":%u,\"level\":%u,\"transition_steps\":%u,\"audio_device\":%u,\"audio_buffers\":%llu,\"audio_recycled\":%llu,"
                "\"audio_position\":%u,\"audio_notes\":%llu,\"audio_errors\":%u,\"audio_worker_progress\":%u,\"audio_m_key_preserves_mute\":%u,\"audio_footsteps\":%u,\"audio_spatial\":%u,\"audio_music\":%u,\"audio_level_starts\":%u,\"audio_fade\":%u,"
                "\"map_views\":%u,\"map_starts\":%u,\"map_frozen_ticks\":%u,\"map_music_checks\":%u,\"level_music_checks\":%u,\"map_black_presents\":%u,\"map_fade_presents\":%u,"
                "\"map_black_hold_ticks\":%u,\"level_fade_ticks\":%u,\"level_fade_starts\":%u,\"level_fade_presents\":%u,\"black_hold_min_us\":%llu,\"level_fade_min_us\":%llu,"
                "\"menu_starts\":%u,\"menu_frozen_ticks\":%u,\"difficulty\":%u,\"language\":%u,\"menu_music_toggles\":%u,\"menu_credits_checks\":%u,\"password_edges\":%u,\"password_loadout\":%u,\"reveal_weapon_checks\":%u,"
                "\"reveal_game_ticks\":%u,\"reveal_move_checks\":%u,\"reveal_hud_checks\":%u,\"reveal_entry_checks\":%u,\"menu_mouse_checks\":%u,\"menu_music_continuity\":%u,\"level_starter_checks\":%u}\n",frames,(unsigned long long)a->presents,a->startup_restarts,
                a->level,a->transition_stage,a->audio.device?1u:0u,(unsigned long long)a->audio.submitted,(unsigned long long)a->audio.recycled,
                ssa_win32_audio_position(&a->audio),(unsigned long long)a->audio.mixer.notes,a->audio.mixer.errors,a->audio_test_worker,a->audio_test_m_key,a->audio_test_footsteps,a->audio_test_spatial,a->audio_test_music,a->audio_test_level_starts,a->audio_test_fade,
                a->map_views,a->map_starts,a->map_wait_ticks,a->map_music_checks,a->level_music_checks,a->map_black_presents,a->map_fade_presents,
                a->map_hold_ticks,a->level_fade_ticks,a->level_fade_starts,a->level_fade_presents,
                (unsigned long long)a->black_hold_min_us,(unsigned long long)a->level_fade_min_us,
                a->menu_starts,a->menu_wait_ticks,a->difficulty,a->language,a->menu_toggle_count,a->menu_credits_checks,a->password_test_edges,a->menu?a->menu->loadout:0,a->reveal_weapon_checks,
                a->reveal_game_ticks,a->reveal_move_checks,a->reveal_hud_checks,a->reveal_entry_checks,
                a->menu_mouse_checks,a->menu_music_continuity,a->level_starter_checks);
            return 1;
        }
        if(!a->smoke)MsgWaitForMultipleObjectsEx(0,NULL,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    return !a->failed;
}
static int resolution(App *a,const wchar_t *value)
{
    wchar_t *end;unsigned long w,h;
    if(!wcscmp(value,L"auto")){a->render_width=a->render_height=0;return 1;}
    errno=0;w=wcstoul(value,&end,10);if(errno || end==value || (*end!=L'x' && *end!=L'X') || !w || w>INT_MAX)return 0;
    value=end+1;errno=0;h=wcstoul(value,&end,10);if(errno || end==value || *end || !h || h>INT_MAX)return 0;
    a->render_width=(unsigned)w;a->render_height=(unsigned)h;return 1;
}
int wmain(int argc,wchar_t **argv)
{
    App a={0};WNDCLASSEXW wc={0};RAWINPUTDEVICE raw={0};HINSTANCE instance=GetModuleHandleW(NULL);
    const wchar_t *path=NULL,*shot=NULL,*save_path=NULL,*soundfont_path=L"soundfonts/SC-55.sf2";int i,result=1,registered=0,raw_registered=0;
    unsigned midi_delay_ms=SSA_MIDI_DELAY_MS;
    /* Keep auto resolution in physical client pixels on high-DPI monitors. */
    if(!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))SetProcessDPIAware();
    a.smoke_frames=600;a.difficulty=1;
    for(i=1;i<argc;++i){
        if(!wcscmp(argv[i],L"--smoke"))a.smoke=1;
        else if(!wcscmp(argv[i],L"--smoke-resize"))a.smoke_resize=1;
        else if(!wcscmp(argv[i],L"--smoke-look"))a.smoke_look=1;
        else if(!wcscmp(argv[i],L"--smoke-present-each"))a.smoke_present_each=1;
        else if(!wcscmp(argv[i],L"--startup-test"))a.startup_test=1;
        else if(!wcscmp(argv[i],L"--audio-test"))a.audio_test=1;
        else if(!wcscmp(argv[i],L"--netricsa-test"))a.net_test=1;
        else if(!wcscmp(argv[i],L"--transition-test"))a.transition_test=1;
        else if(!wcscmp(argv[i],L"--menu-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>8)goto usage;a.menu_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--password-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>=3*PASSWORD_TEST_LEVELS)goto usage;a.password_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--level-select-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>=SSA_NATIVE_CAMPAIGN_LEVELS)goto usage;a.level_select_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--mute"))a.mute=1;
        else if(!wcscmp(argv[i],L"--no-music"))a.no_music=1;
        else if(!wcscmp(argv[i],L"--soundfont") && i+1<argc)soundfont_path=argv[++i];
        else if(!wcscmp(argv[i],L"--midi-delay") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>SSA_MIDI_MAX_DELAY_MS)goto usage;midi_delay_ms=(unsigned)n;}
        else if(!wcscmp(argv[i],L"--smoke-frames") && i+1<argc){
            wchar_t *end;unsigned long n;errno=0;n=wcstoul(argv[++i],&end,10);
            if(errno || argv[i][0]<L'0' || argv[i][0]>L'9' || end==argv[i] || *end || !n)goto usage;a.smoke_frames=(unsigned)n;
        }
        else if(!wcscmp(argv[i],L"--resolution") && i+1<argc){if(!resolution(&a,argv[++i]))goto usage;}
        else if(!wcscmp(argv[i],L"--original-renderer"))a.original_renderer=1;
        else if(!wcscmp(argv[i],L"--vulkan"))a.vulkan_requested=1;
        else if(!wcscmp(argv[i],L"--story") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(a.direct_level || !argv[i][0] || *end || (n!=0 && n!=6 && n!=12))goto usage;
            a.story_requested=(unsigned)n+1;a.level=n==12?11:(unsigned)n;a.direct_level=1;}
        else if(!wcscmp(argv[i],L"--level") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);if(a.story_requested || !argv[i][0] || *end || n>=SSA_NATIVE_CAMPAIGN_LEVELS)goto usage;a.level=(unsigned)n;a.direct_level=1;}
        else if(!wcscmp(argv[i],L"--snapshot") && i+1<argc)shot=argv[++i];
        else if(!wcscmp(argv[i],L"--save-file") && i+1<argc)save_path=argv[++i];
        else if(argv[i][0]!=L'-' && !path)path=argv[i];else goto usage;
    }
    if(!path || (!a.smoke && (a.smoke_frames!=600 || a.smoke_resize || a.smoke_look || a.smoke_present_each)))goto usage;
    if(a.startup_test && a.smoke)goto usage;
    if(a.story_requested && (a.smoke || a.menu_test || a.password_test || a.level_select_test || a.audio_test || a.transition_test))goto usage;
    if(a.audio_test && !a.startup_test)goto usage;
    if(a.net_test && (!a.startup_test || a.smoke || a.story_requested || a.audio_test || a.menu_test || a.password_test || a.transition_test))goto usage;
    if(a.transition_test && (!a.startup_test || a.audio_test || a.level || !a.mute))goto usage;
    if(a.menu_test && (!a.startup_test || a.audio_test || a.transition_test || a.direct_level || !a.mute))goto usage;
    if(a.password_test && (!a.startup_test || a.audio_test || a.transition_test || a.direct_level || a.menu_test || !a.mute))goto usage;
    if(a.level_select_test && (!a.startup_test || a.audio_test || a.transition_test || a.direct_level || a.menu_test || a.password_test || !a.mute))goto usage;
    a.game=calloc(1,sizeof(*a.game));if(!a.game){fprintf(stderr,"Could not allocate native game state.\n");goto done;}
    if(!read_rom(&a,path)){fwprintf(stderr,L"Could not load the expected SSA.gba ROM: %ls\n",path);goto done;}
    if(!ssa_win32_profile_open(&a.profile,save_path?save_path:L"save.dat",save_path || (!a.smoke && !a.startup_test))){
        fprintf(stderr,"Could not load save data (Windows error %u); using defaults: %ls\n",a.profile.last_error,a.profile.path);
        a.menu_notice="Save data could not be loaded. Using defaults.";a.menu_notice_ticks=300;
    }
    a.language=a.profile.data.language;
    a.settings=(SsaSettings)SSA_SETTINGS_DEFAULT;
    if(save_path){if(swprintf(a.settings_path,32768,L"%ls.settings",save_path)<0)goto done;}
    else wcscpy_s(a.settings_path,32768,L"settings.dat");
    if(a.profile.enabled && !ssa_win32_settings_read(&a.settings,a.settings_path)){
        fprintf(stderr,"Could not load Settings; using defaults.\n");
        a.menu_notice="Settings could not be loaded. Using defaults.";a.menu_notice_ticks=300;}
    {wchar_t scores_path[32768];
        if(save_path){if(swprintf(scores_path,32768,L"%ls.scores",save_path)<0)goto done;}
        else wcscpy_s(scores_path,32768,L"scores.dat");
        if(!ssa_win32_scores_open(&a.scores,a.rom,a.rom_size,scores_path,a.profile.enabled)){
            fprintf(stderr,"Could not load Scores (Windows error %u); using defaults.\n",a.scores.last_error);
            a.menu_notice="Scores could not be loaded. Using defaults.";a.menu_notice_ticks=300;
        }
    }
    if(a.original_renderer)a.settings.renderer=SSA_RENDER_GBA; /* Explicit CLI override. */
    if(a.vulkan_requested)a.settings.renderer=SSA_RENDER_VULKAN;
    presentation_sync_init(&a);
    a.vulkan=ssa_vulkan_open();
    if(a.settings.renderer==SSA_RENDER_VULKAN && !a.vulkan){
        a.settings.renderer=SSA_RENDER_SOFTWARE;
        fprintf(stderr,"Vulkan hardware unavailable; using Software.\n");
        a.menu_notice="Vulkan unavailable; using Software.";a.menu_notice_ticks=300;
    }
    a.original_renderer=a.settings.renderer==SSA_RENDER_GBA;
    if(a.direct_level?
       ssa_native_game_init_loadout(a.game,a.rom,a.rom_size,a.level,a.difficulty*12+a.level+1)!=1:
       ssa_native_game_init_campaign(a.game,a.rom,a.rom_size,a.level,a.difficulty,NULL)!=1){
        fprintf(stderr,"Native init failed line%u\n",a.game->error_line);goto done;}
    if(!level_options(&a,a.game))goto done;
    if(ssa_native_game_alternate_weapons(a.game,a.settings.alternate_weapons,
        a.direct_level?SSA_ALTERNATE_LOADOUT:SSA_ALTERNATE_NEW_GAME)!=1)goto done;
    a.completion_setting=a.direct_level?a.difficulty*12+a.level+1:0;
    if(ssa_native_game_carry_capture(&a.entry,a.game)!=1)goto done;
    if(!a.smoke && (!a.startup_test || a.audio_test || a.transition_test || a.menu_test || a.password_test || a.story_requested || a.net_test)){
        if(ssa_win32_audio_open(&a.audio,a.rom,a.rom_size,a.level,a.mute,a.no_music)){
            EnterCriticalSection(&a.audio.mutex);
            if(a.audio.midi)(void)ssa_win32_midi_set_delay(a.audio.midi,midi_delay_ms);
            LeaveCriticalSection(&a.audio.mutex);
            (void)ssa_win32_audio_load_soundfont(&a.audio,soundfont_path);
            if(!ssa_win32_audio_settings(&a.audio,&a.settings)){
                a.settings.music_style=SSA_MUSIC_ORIGINAL;
                (void)ssa_win32_audio_settings(&a.audio,&a.settings);
                a.menu_notice="Saved music mode unavailable; using Original.";a.menu_notice_ticks=300;}
            SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a.audio,
                ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
            if(ssa_native_game_bind_audio(a.game,&ops)<0)goto done;
        }else fprintf(stderr,"Audio output unavailable (%u); continuing silently.\n",a.audio.last_error);
    }
    a.bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a.bitmap.bmiHeader.biWidth=240;a.bitmap.bmiHeader.biHeight=-160;
    a.bitmap.bmiHeader.biPlanes=1;a.bitmap.bmiHeader.biBitCount=32;a.bitmap.bmiHeader.biCompression=BI_RGB;
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,MAKEINTRESOURCEW(32512)) /* IDC_ARROW, also in non-UNICODE test builds */;wc.lpszClassName=CLASS_NAME;
    if(!RegisterClassExW(&wc))goto done;registered=1;
    a.window=CreateWindowExW(0,CLASS_NAME,TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,900,NULL,NULL,instance,&a);
    if(!a.window)goto done;
    if(!display_mode_apply(&a,a.settings.display_mode)){
        fprintf(stderr,"Could not apply saved display mode (Windows error %lu).\n",(unsigned long)GetLastError());goto done;}
    if(!a.smoke){raw.usUsagePage=1;raw.usUsage=2;raw.hwndTarget=a.window;
        if(!RegisterRawInputDevices(&raw,1,sizeof(raw))){fprintf(stderr,"Raw mouse registration failed (Windows error %lu).\n",(unsigned long)GetLastError());goto done;}
        raw_registered=1;}
    if(run(&a) && snapshot(&a,shot))result=0;
done:
    ssa_win32_audio_close(&a.audio);
    if(a.window)release(&a);
    if(raw_registered){raw.dwFlags=RIDEV_REMOVE;raw.hwndTarget=NULL;if(!RegisterRawInputDevices(&raw,1,sizeof(raw)))result=1;}
    if(a.window && IsWindow(a.window))DestroyWindow(a.window);
    if(registered)UnregisterClassW(CLASS_NAME,instance);
    ssa_pc_renderer_free(&a.renderer);
    ssa_vulkan_close(a.vulkan);
    if(a.dwm)FreeLibrary(a.dwm);
    if(a.back_dc){if(a.back_original)SelectObject(a.back_dc,a.back_original);
        if(a.back_bitmap)DeleteObject(a.back_bitmap);DeleteDC(a.back_dc);}
    free(a.intermission);free(a.screen_backdrop);free(a.screen_pixels);
    if(a.game){ssa_native_game_free(a.game);free(a.game);}free(a.map_view);free(a.menu);free(a.story);free(a.netricsa);free(a.net_backdrop);free(a.net_hud_frame);free(a.net_pixels);free(a.rom);return result;
usage:
    fwprintf(stderr,L"Usage: ssa_native_game.exe ROM.gba [--resolution auto|WIDTHxHEIGHT] [--original-renderer] [--mute] [--no-music] [--level 0..%u] [--smoke] [--snapshot output.bmp]\nNative campaign (%u levels); M toggles automap (enable in Extras).\n",SSA_NATIVE_CAMPAIGN_LEVELS-1,SSA_NATIVE_CAMPAIGN_LEVELS);return 2;
}
