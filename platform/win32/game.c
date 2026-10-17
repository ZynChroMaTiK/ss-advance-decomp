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
#include "ssa/map_view.h"
#include "ssa/menu.h"
#include "audio_win32.h"
#include "profile_win32.h"

typedef struct App {
    HWND window;SsaNativeGame *game;uint8_t *rom;size_t rom_size;
    unsigned level,captured,running,smoke,failed,reload,smoke_frames;
    unsigned advance;
    int wheel_remainder;
    SsaNativeCarry entry;
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
    unsigned menu_active,menu_pressed,menu_wait_ticks,menu_starts,menu_last_page,menu_page_ticks;
    unsigned direct_level,language,difficulty,menu_notice_ticks;
    unsigned menu_test,menu_options_test,menu_toggle_count,menu_held_keys;
    unsigned menu_credits_test,menu_credits_checks;
    unsigned menu_mouse_checks,menu_music_continuity;
    unsigned password_test,password_test_edges;
    unsigned level_select_test;
    unsigned level_starter_checks;
    SsaWin32Profile profile;
    unsigned completion_recorded;
    const char *menu_notice;
    unsigned render_width,render_height,original_renderer;
    unsigned smoke_resize,initial_width,initial_height;
    unsigned smoke_look;
    unsigned smoke_present_each;
    unsigned startup_test,startup_restarts;
    unsigned transition_test,transition_stage;
    SsaNativeCarry transition_expected;
    unsigned mute,no_music,audio_test,audio_test_sound;
    unsigned audio_test_worker,audio_test_mute,audio_test_footsteps,audio_test_spatial,audio_test_music;
    unsigned audio_test_music_queued,audio_test_level_starts,audio_test_fade;
    SsaWin32Audio audio;
    SsaPcRenderer renderer;
    HDC back_dc;HBITMAP back_bitmap;HGDIOBJ back_original;
    uint32_t *back_pixels;BITMAPINFO back_info;
    int back_width,back_height;
    uint64_t presents;
    uint64_t presentation_samples;
    BITMAPINFO bitmap;
} App;
static const wchar_t CLASS_NAME[]=L"SeriousSamAdvanceNativeGame";
static const wchar_t TITLE[]=L"Serious Sam Advance | Native PC prototype | Click to play";
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
    unsigned held=a->captured;a->captured=0;a->wheel_remainder=0;a->menu_held_keys=0;
    ssa_pc_input_runtime_focus(&a->game->input_runtime,0);
    ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
    if(held){ClipCursor(NULL);if(GetCapture()==a->window)ReleaseCapture();SetCursor(LoadCursorW(NULL,IDC_ARROW));}
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
    if(a->map_active || a->menu_active)return;
    if(!confine(a))return;
    a->captured=1;SetCapture(a->window);if(GetCapture()!=a->window){release(a);return;}
    SetCursor(NULL);ssa_pc_input_runtime_focus(&a->game->input_runtime,1);
    ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
}
static int presentation_error(const char *stage)
{fprintf(stderr,"Presentation failed: %s (Windows error %lu).\n",stage,(unsigned long)GetLastError());return 0;}
static int snapshot_surface(const wchar_t *,const BITMAPINFO *,const uint32_t *);
static int present(App *a,HDC window_dc)
{
    RECT rect;int w,h,ox,oy,ow,oh,n;HDC dc;const char *notice=a->game->notice;
    int rw=a->bitmap.bmiHeader.biWidth,rh=-a->bitmap.bmiHeader.biHeight;
    const uint32_t *pixels=!a->map_active && !a->menu_active && a->renderer.pixels && !a->original_renderer?a->renderer.pixels:a->game->rgb888;
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
    if(!a->captured && !a->smoke && !a->map_active && !a->menu_active && !a->level_revealing){
        const char *help="Click to capture | WASD | Mouse fire | Space jump | H camera | E use | Wheel / 1-0 weapons | Q last weapon | R restart | Esc release";
        TextOutA(dc,12,12,help,(int)strlen(help));
    }
    if(a->menu_active && a->menu_notice_ticks && a->menu_notice)
        TextOutA(dc,12,h-28,a->menu_notice,(int)strlen(a->menu_notice));
    if(!a->map_active && !a->menu_active && (a->game->notice_ticks || a->game->finished || !a->game->players[0].inventory.health))
        TextOutA(dc,12,36,notice,(int)strlen(notice));
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
    if(a->map_active || a->menu_active){unsigned i;
        if((a->menu_active?ssa_menu_draw(a->menu,a->game->rgb555,SSA_DISPLAY_PIXELS):
            ssa_map_view_draw(a->map_view,a->game->rgb555,SSA_DISPLAY_PIXELS))!=1)return 0;
        for(i=0;i<SSA_DISPLAY_PIXELS;++i){unsigned c=a->game->rgb555[i];
            unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
            a->game->rgb888[i]=((r*255/31)<<16)|((g*255/31)<<8)|(b*255/31);
        }
        a->bitmap.bmiHeader.biWidth=240;a->bitmap.bmiHeader.biHeight=-160;return 1;
    }
    if(a->original_renderer){a->bitmap.bmiHeader.biWidth=240;a->bitmap.bmiHeader.biHeight=-160;
        return ssa_native_game_draw(a->game)==1;}
    if(ssa_native_game_prepare_view(a->game)<0)return 0;
    if(!w){if(!GetClientRect(a->window,&rect))return 0;
        if(rect.right<=0 || rect.bottom<=0)return 1;
        w=(unsigned)rect.right;h=(unsigned)rect.bottom;}
    if(ssa_pc_renderer_resize(&a->renderer,w,h)<0){
        fprintf(stderr,"Could not allocate render resolution %ux%u.\n",w,h);return 0;}
    if(ssa_pc_renderer_draw(&a->renderer,a->game)<0){
        fprintf(stderr,"PC renderer rejected frame %lu.\n",(unsigned long)a->game->frame_game.counter);return 0;}
    a->bitmap.bmiHeader.biWidth=(LONG)w;a->bitmap.bmiHeader.biHeight=-(LONG)h;return 1;
}
static int begin_map(App *a)
{
    if(!a->map_view){a->map_view=calloc(1,sizeof(*a->map_view));if(!a->map_view)return 0;}
    else memset(a->map_view,0,sizeof(*a->map_view));
    if(ssa_map_view_init(a->map_view,a->rom,a->rom_size,a->level,a->language,a->difficulty)!=1)return 0;
    if(a->audio.device && !ssa_win32_audio_restart(&a->audio,a->level)){
        fprintf(stderr,"Map audio start failed (%u); continuing silently.\n",a->audio.last_error);
        ssa_win32_audio_close(&a->audio);(void)ssa_native_game_bind_audio(a->game,NULL);
    }
    a->map_active=1;a->map_fire=0;a->map_black_hold=MAP_BLACK_HOLD_TICKS;a->completion_recorded=0;
    a->black_presented_at.QuadPart=a->reveal_presented_at.QuadPart=0;
    a->level_revealing=a->level_fade=0;++a->map_views;return render(a);
}
static int begin_menu(App *a)
{
    a->menu=calloc(1,sizeof(*a->menu));if(!a->menu)return 0;
    if(ssa_menu_init(a->menu,a->rom,a->rom_size,!a->no_music)!=1)return 0;
    a->menu->level_limit=SSA_NATIVE_CAMPAIGN_LEVELS;
    a->menu->language=a->language;a->menu->completed_levels=a->profile.data.completed;
    a->menu_active=1;a->menu_last_page=65535;return render(a);
}
static void level_options(const App *a,SsaNativeGame *g)
{
    static const uint8_t continues[]={5,3,2};
    g->damage.difficulty=(uint8_t)a->difficulty;
    g->gameplay.life_game.continue_flag=continues[a->difficulty];
}
static void save_change(App *a,unsigned action,unsigned value)
{
    if(!ssa_win32_profile_store(&a->profile,action,value)){
        fprintf(stderr,"Could not save progress/settings (Windows error %u): %ls\n",a->profile.last_error,a->profile.path);
        a->menu_notice="Could not save data. Check the save file's folder permissions.";a->menu_notice_ticks=300;
    }
}
static int game_tick(App *a)
{
    int result=ssa_native_game_tick(a->game);
    if(result==1 && a->game->finished && a->game->level_runtime.script.exit_toggle==1 &&
       a->game->players[0].inventory.health>0 && !a->completion_recorded){
        save_change(a,SSA_SAVE_COMPLETE,a->level);a->completion_recorded=1;
    }
    return result;
}
static int finish_menu(App *a)
{
    a->language=a->menu->language;a->difficulty=a->menu->difficulty;a->no_music=!a->menu->music_enabled;
    if(a->menu->loadout){SsaNativeGame *next=calloc(1,sizeof(*next));
        if(!next)return 0;
        if(a->menu->level>=SSA_NATIVE_CAMPAIGN_LEVELS ||
           ssa_native_game_init_loadout(next,a->rom,a->rom_size,a->menu->level,a->menu->loadout)!=1){
            ssa_native_game_free(next);free(next);return 0;}
        release(a);ssa_native_game_free(a->game);free(a->game);a->game=next;a->level=a->menu->level;
        ssa_pc_renderer_free(&a->renderer);
        if(a->audio.device){SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a->audio,
            ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
            if(ssa_native_game_bind_audio(a->game,&ops)<0)return 0;}
        if(ssa_native_game_carry_capture(&a->entry,a->game)!=1)return 0;
    }
    level_options(a,a->game);a->menu_active=0;a->menu_pressed=0;++a->menu_starts;
    return begin_map(a);
}
static int finish_map(App *a)
{
    uint64_t duration=elapsed_us(a->black_presented_at);
    if(a->startup_test && a->map_black_presents!=a->map_views){
        fprintf(stderr,"Map handoff skipped the final black presentation.\n");return 0;}
    if(a->startup_test && duration<250000){fprintf(stderr,"Black hold was too short.\n");return 0;}
    if(duration && (!a->black_hold_min_us || duration<a->black_hold_min_us))a->black_hold_min_us=duration;
    /* Flush queued map audio before the first gameplay tick. Both music and
     * non-music starts reset the sound fade and transient confirm effect. */
    if(a->audio.device && !ssa_win32_audio_begin_level(&a->audio,a->level)){
        fprintf(stderr,"Level audio start failed (%u); continuing silently.\n",a->audio.last_error);
        ssa_win32_audio_close(&a->audio);(void)ssa_native_game_bind_audio(a->game,NULL);
    }
    a->map_active=0;a->map_fire=0;++a->map_starts;
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
               ssa_sprite_oam_build(&a->game->oam,&a->game->sprites,&a->game->sprite_assets,a->game->cache.entries,&a->game->math)<0 ||
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
static int reload(App *a)
{
    SsaNativeCarry carry=a->entry;SsaNativeGame *next;unsigned level=a->level;
    if(a->advance){
        if(!a->game->finished || a->level+1>=SSA_NATIVE_CAMPAIGN_LEVELS || ssa_native_game_carry_capture(&carry,a->game)!=1)return 0;
        level=a->level+1;
    }
    next=calloc(1,sizeof(*next));
    if(!next)return 0;
    if(ssa_native_game_init_campaign(next,a->rom,a->rom_size,level,a->difficulty,&carry)!=1){
        ssa_native_game_free(next);free(next);return 0;
    }
    level_options(a,next);
    release(a);ssa_native_game_free(a->game);free(a->game);a->game=next;a->level=level;a->entry=carry;
    ssa_pc_renderer_free(&a->renderer);
    if(a->audio.device){SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a->audio,
        ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
        if(ssa_native_game_bind_audio(a->game,&ops)<0)return 0;
    }
    ssa_pc_input_runtime_focus(&a->game->input_runtime,0);ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
    a->reload=a->advance=0;return begin_map(a);
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
        a->game->game.life_game.input_state=3;++a->transition_stage;
    }else if(phase==1 && a->game->finished){
        SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);++a->transition_stage;
    }else if((phase==2 && frames>=base+35) || (phase==3 && frames>=base+60)){
        if(a->level!=target || a->startup_restarts!=2*target-(phase==2?1u:0u) ||
           a->entry.score!=a->transition_expected.score || a->entry.weapon!=3 ||
           memcmp(&a->entry.inventory,&a->transition_expected.inventory,sizeof(SsaInventory)) ||
           a->game->finished || a->game->error_line)return 0;
        if(phase==2){
            a->game->players[0].inventory.health=0;a->game->players[0].inventory.ammo[2]=0;a->game->score=99999;
            SendMessageW(a->window,WM_KEYDOWN,'R',0);
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
    case WM_CAPTURECHANGED:if(a->captured && (HWND)lp!=w)release(a);break;
    case WM_DPICHANGED:{const RECT *bounds=(const RECT *)lp;
        SetWindowPos(w,NULL,bounds->left,bounds->top,bounds->right-bounds->left,bounds->bottom-bounds->top,
            SWP_NOACTIVATE|SWP_NOZORDER);return 0;}
    case WM_SIZE:case WM_MOVE:if(a->captured && !confine(a))release(a);InvalidateRect(w,NULL,FALSE);break;
    case WM_MOUSEMOVE:
        if(a->menu_active){RECT bounds;int x,y;
            if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,240,160,
                (short)LOWORD(lp),(short)HIWORD(lp),&x,&y))
                (void)menu_pointer_input(a,x,y,0);
        }return 0;
    case WM_CHAR:
        if(a->menu_active && a->menu->page==SSA_MENU_PASSWORD){
            (void)ssa_menu_text(a->menu,(unsigned)wp);return 0;
        }break;
    case WM_LBUTTONDOWN:
        if(!a->smoke){if(a->menu_active){RECT bounds;int x,y;
                if(GetClientRect(w,&bounds) && ssa_ui_pointer(bounds.right,bounds.bottom,240,160,
                    (short)LOWORD(lp),(short)HIWORD(lp),&x,&y))
                    a->menu_pressed|=menu_pointer_input(a,x,y,1);
            }
            else if(a->map_active)a->map_fire=1;
            else {if(!a->captured)capture(a);else ssa_pc_input_runtime_button(&a->game->input_runtime,1,1);}}return 0;
    case WM_LBUTTONUP:ssa_pc_input_runtime_button(&a->game->input_runtime,1,0);return 0;
    case WM_MOUSEWHEEL:
        if(a->captured && !a->menu_active && !a->map_active &&
           !(a->game->frame_game.flags&1) && a->game->players[0].inventory.health>0 &&
           a->game->input.focused && a->game->input.enabled){
            int delta=a->wheel_remainder+(short)HIWORD(wp),steps=delta/WHEEL_DELTA;
            a->wheel_remainder=delta%WHEEL_DELTA;
            if(steps)ssa_native_game_cycle_weapon(a->game,-steps);
        }else a->wheel_remainder=0;
        return 0;
    case WM_KEYDOWN:case WM_KEYUP:{
        int down=msg==WM_KEYDOWN,repeat=down && (lp&((LPARAM)1<<30))!=0;
        if(a->menu_active){
            int editing=a->menu && a->menu->page==SSA_MENU_PASSWORD;
            unsigned key=wp==VK_UP?1u:!editing && wp=='W'?2u:wp==VK_DOWN?4u:!editing && wp=='S'?8u:0;
            if(down)a->menu_held_keys|=key;else a->menu_held_keys&=~key;
            if(editing && down && wp==VK_DELETE){(void)ssa_menu_text(a->menu,127);return 0;}
            if(editing && down && (wp==VK_HOME || wp==VK_END) && a->menu->password_typed){
                a->menu->password_cursor=wp==VK_HOME?0:(uint8_t)strlen(a->menu->password);return 0;}
            if(down && !repeat){
                if(wp==VK_UP || (!editing && wp=='W'))a->menu_pressed|=SSA_MENU_UP;
                else if(wp==VK_DOWN || (!editing && wp=='S'))a->menu_pressed|=SSA_MENU_DOWN;
                else if(wp==VK_LEFT || (!editing && wp=='A'))a->menu_pressed|=SSA_MENU_LEFT;
                else if(wp==VK_RIGHT || (!editing && wp=='D'))a->menu_pressed|=SSA_MENU_RIGHT;
                else if(wp==VK_RETURN || (!editing && wp==VK_SPACE))a->menu_pressed|=SSA_MENU_CONFIRM|SSA_MENU_START;
                else if(wp==VK_ESCAPE)a->menu_pressed|=SSA_MENU_BACK;
                else if(wp=='M' && !editing)ssa_win32_audio_toggle_mute(&a->audio);
            }return 0;
        }
        if(down && wp==VK_ESCAPE){release(a);return 0;}
        if(down && !repeat && wp=='M'){ssa_win32_audio_toggle_mute(&a->audio);return 0;}
        if(a->map_active && down && !repeat && (wp==VK_RETURN || wp==VK_SPACE)){a->map_fire=1;return 0;}
        if(down && !repeat && (wp=='R' || wp==VK_RETURN)){
            a->advance=wp==VK_RETURN && a->game->finished && a->level+1<SSA_NATIVE_CAMPAIGN_LEVELS;
            a->reload=1;return 0;
        }
        if((a->captured || (a->startup_test && a->transition_test && a->level_revealing)) && wp<128){
            if(down && !repeat && wp>='0' && wp<='9')
                ssa_native_game_select_weapon(a->game,wp=='0'?10:(unsigned)(wp-'0'));
            else if(down && !repeat && wp=='Q' && !a->map_active && !(a->game->frame_game.flags&1) &&
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
    const uint32_t *pixels=!a->map_active && !a->menu_active && a->renderer.pixels && !a->original_renderer?a->renderer.pixels:a->game->rgb888;
    return snapshot_surface(path,&a->bitmap,pixels);
}
static int run(App *a)
{
    LARGE_INTEGER frequency,previous,now;double acc=0;MSG m;unsigned frames=0;
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&previous))return 0;
    if(a->smoke){ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);}
    else {
        ssa_pc_input_runtime_focus(&a->game->input_runtime,0);ssa_pc_input_runtime_enable(&a->game->input_runtime,0);
        if(!(a->direct_level?begin_map(a):begin_menu(a)))return 0;
        ShowWindow(a->window,a->startup_test?SW_SHOWNOACTIVATE:SW_SHOW);UpdateWindow(a->window);
    }
    a->running=1;
    while(a->running){unsigned rendered_frame=frames,map_rendered_frame=a->map_view?a->map_view->frame:0,
        menu_rendered_frame=a->menu_wait_ticks,transition_rendered_frame=a->transition_frame;
        while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){if(m.message==WM_QUIT){a->running=0;break;}TranslateMessage(&m);DispatchMessageW(&m);}
        if(!a->running || a->failed)break;
        if(a->reload){if(!reload(a))return 0;if(a->startup_test)++a->startup_restarts;}
        if(a->audio.device && !ssa_win32_audio_pump(&a->audio)){
            fprintf(stderr,"Audio output failed (%u); continuing silently.\n",a->audio.last_error);ssa_win32_audio_close(&a->audio);
            (void)ssa_native_game_bind_audio(a->game,NULL);
        }
        if(a->audio_test && frames>=20 && !a->audio_test_sound){
            if(a->game->audio_ops.play)a->game->audio_ops.play(a->game->audio_ops.context,0x14);
            if(a->audio.device){uint64_t before;unsigned muted;SsaAudioPlayer music;
                EnterCriticalSection(&a->audio.mutex);muted=a->audio.mixer.muted;music=a->audio.mixer.players[0];
                a->audio_test_level_starts=a->no_music?!music.active:music.active && music.song==0x73 && a->audio.mixer.music_command==2;
                a->game->audio_ops.play(a->game->audio_ops.context,13);
                a->audio_test_footsteps=!memcmp(&music,&a->audio.mixer.players[0],sizeof(music)) &&
                    audio_test_latest(&a->audio.mixer,13)->active &&
                    audio_test_latest(&a->audio.mixer,13)->song==13;
                SendMessageW(a->window,WM_KEYDOWN,'M',0);
                a->audio_test_mute=a->audio.mixer.muted!=muted;
                SendMessageW(a->window,WM_KEYDOWN,'M',0);
                a->audio_test_mute&=a->audio.mixer.muted==muted;
                before=a->audio.recycled;LeaveCriticalSection(&a->audio.mutex);
                /* Deliberately stall rendering beyond the50ms audio queue.
                 * The audio worker must continue delivering device buffers. */
                Sleep(160);
                EnterCriticalSection(&a->audio.mutex);a->audio_test_worker=a->audio.recycled>before;
                LeaveCriticalSection(&a->audio.mutex);
            }
            a->audio_test_sound=1;
        }
        if(a->audio_test && a->audio.device && frames>=30 && !a->audio_test_fade){
            EnterCriticalSection(&a->audio.mutex);
            if(ssa_audio_fade(&a->audio.mixer,0)==1)a->audio_test_fade=1;
            LeaveCriticalSection(&a->audio.mutex);
        }
        if(a->audio_test && a->audio.device && frames>=40 && a->audio_test_fade==1){
            EnterCriticalSection(&a->audio.mutex);
            if(a->audio.mixer.master_gain<1 && a->audio.mixer.music.flags&4)a->audio_test_fade=2;
            LeaveCriticalSection(&a->audio.mutex);
        }
        if(a->audio_test && frames>=40 && a->audio_test_footsteps==1 && a->audio.device){SsaAudioPlayer music;
            EnterCriticalSection(&a->audio.mutex);music=a->audio.mixer.players[0];
            a->game->audio_ops.play(a->game->audio_ops.context,14);
            if(!memcmp(&music,&a->audio.mixer.players[0],sizeof(music)) &&
               audio_test_latest(&a->audio.mixer,14)->active &&
               audio_test_latest(&a->audio.mixer,14)->song==14)a->audio_test_footsteps=2;
            LeaveCriticalSection(&a->audio.mutex);
        }
        if(a->audio_test && frames>=80 && !a->audio_test_spatial && a->audio.device){SsaAudioPlayer music;SsaAudioPoint point;
            EnterCriticalSection(&a->audio.mutex);music=a->audio.mixer.players[0];point=a->audio.mixer.listener.position;
            /* The level-clear cue ran before restart. A sticky failure here
             * must not be hidden by the later explicit level-music callback. */
            if(a->audio_test_music!=1 || (a->no_music?music.active:!music.active || music.song!=0x73 || a->audio.mixer.music_command!=2))a->audio_test_music=4;
            else if(a->audio_test_level_starts==1 && a->audio.mixer.master_gain==1 && !(a->audio.mixer.music.flags&12))a->audio_test_level_starts=2;
            point.x+=a->audio.mixer.listener.right_x*256;point.y+=a->audio.mixer.listener.right_y*256;
            a->game->audio_ops.play_at(a->game->audio_ops.context,0x14,&point);
            if(a->audio.mixer.listener_ready && audio_test_latest(&a->audio.mixer,0x14)->positioned &&
               audio_test_latest(&a->audio.mixer,0x14)->target_right>audio_test_latest(&a->audio.mixer,0x14)->target_left*3 &&
               !memcmp(&music,&a->audio.mixer.players[0],sizeof(music)))a->audio_test_spatial=1;
            LeaveCriticalSection(&a->audio.mutex);
        }
        if(a->audio_test && frames>=90 && a->audio_test_spatial==1 && a->audio.device){SsaAudioPoint point;uint64_t notes;
            EnterCriticalSection(&a->audio.mutex);notes=a->audio.mixer.notes;point=a->audio.mixer.listener.position;
            point.x-=a->audio.mixer.listener.right_x*256;point.y-=a->audio.mixer.listener.right_y*256;
            a->game->audio_ops.move(a->game->audio_ops.context,0x14,&point);
            if(a->audio.mixer.notes==notes && audio_test_latest(&a->audio.mixer,0x14)->target_left>audio_test_latest(&a->audio.mixer,0x14)->target_right*3)
                a->audio_test_spatial=2;
            LeaveCriticalSection(&a->audio.mutex);
        }
        if(a->audio_test && a->audio.device && a->audio_test_music<3){
            static const unsigned at[]={45,100,110},command[]={9,10,2},song[]={0x78,0x81,0x73};
            unsigned step=a->audio_test_music;
            if(frames>=at[step]){SsaAudioPlayer *player;
                EnterCriticalSection(&a->audio.mutex);
                if(!a->audio_test_music_queued){
                    a->game->audio_ops.music(a->game->audio_ops.context,command[step],a->level);a->audio_test_music_queued=1;
                }
                player=&a->audio.mixer.players[0];
                if((a->no_music?!player->active:player->active && player->song==song[step]) && !(a->audio.mixer.music.flags&8)){
                    ++a->audio_test_music;a->audio_test_music_queued=0;
                }
                LeaveCriticalSection(&a->audio.mutex);
            }
        }
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
            if(a->menu_active){unsigned i;
                if(a->game->frame_game.counter){fprintf(stderr,"Gameplay advanced during menu.\n");return 0;}
                ++a->menu_wait_ticks;
                if(a->menu_notice_ticks)--a->menu_notice_ticks;
                if(a->menu_last_page!=a->menu->page){a->menu_last_page=a->menu->page;a->menu_page_ticks=0;
                    if(a->menu->page==SSA_MENU_PASSWORD)a->menu_held_keys=0;}
                if(a->startup_test){unsigned p=a->menu->page,t=a->menu_page_ticks;
                    unsigned wanted_language=a->menu_test?(a->menu_test-1)/3:a->password_test?(a->password_test-1)%3:0;
                    unsigned wanted_difficulty=a->menu_test?(a->menu_test-1)%3:1;
                    if(p==SSA_MENU_MAIN && (a->menu_test || (a->password_test && wanted_language)) &&
                            !a->menu_options_test && (t==2 || t==4))
                        SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                    else if(p==SSA_MENU_OPTIONS && (a->menu_test || a->password_test)){
                        if(a->menu_credits_test==2){
                            if(t==2){SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);a->menu_options_test=1;}
                        }else if(a->menu_test && (t==2 || t==4))SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                        else if(t==6 || t==8)SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                        else if((t==10 && wanted_language>=1) || (t==12 && wanted_language==2))
                            SendMessageW(a->window,WM_KEYDOWN,VK_RIGHT,0);
                        else if(t==14){
                            if(a->password_test){SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);a->menu_options_test=1;}
                            else SendMessageW(a->window,WM_KEYDOWN,VK_UP,0);
                        }else if(a->menu_test && t==16)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                    }
                    else if(p==SSA_MENU_CREDITS && a->menu_test){
                        a->menu_credits_test=1;
                        if(t==0){SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);SendMessageW(a->window,WM_KEYUP,VK_UP,0);}
                        else if(t==2)SendMessageW(a->window,WM_KEYDOWN,VK_UP,0);
                        else if(t==4)SendMessageW(a->window,WM_KEYDOWN,'W',0);
                        else if(t==5)SendMessageW(a->window,WM_KEYUP,VK_UP,0);
                        else if(t==6){SendMessageW(a->window,WM_KEYUP,'W',0);SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);}
                        else if(t==8)SendMessageW(a->window,WM_KEYDOWN,'S',0);
                        else if(t==9)SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);
                        else if(t==10)SendMessageW(a->window,WM_KILLFOCUS,0,0);
                        else if(t==70)SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);
                    }
                    else if(p==SSA_MENU_SINGLE && a->password_test && (t==2 || t==4))
                        SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                    else if(p==SSA_MENU_SINGLE && a->level_select_test && t==2)
                        SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                    else if(p==SSA_MENU_LEVEL_SELECT && a->level_select_test && t>=2)
                        SendMessageW(a->window,WM_KEYDOWN,a->menu->selection==a->level_select_test-1?VK_RETURN:VK_DOWN,0);
                    else if(p==SSA_MENU_MAIN && a->menu_test && t==8){
                        RECT bounds;int ox,oy,ow,oh;unsigned selection=a->menu_options_test?0:2;
                        unsigned before,valid;uint64_t starts;SsaAudioPlayer music;
                        int x=120+(int)a->menu->offset,y=90+(int)selection*16;
                        if(!a->audio.synchronized || !GetClientRect(a->window,&bounds) ||
                           !ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&ox,&oy,&ow,&oh))return 0;
                        /* Keep the mixer stable while checking real pointer
                         * messages. Critical sections permit the cue callback
                         * to enter recursively on this same thread. */
                        EnterCriticalSection(&a->audio.mutex);starts=a->audio.mixer.starts;music=a->audio.mixer.players[0];
                        /* Blank canvas clicks must not submit or make sound. */
                        SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+ow/240,oy+oh/160));
                        valid=!a->menu_pressed && a->audio.mixer.starts==starts;
                        /* Hover another visible label, then the intended row. */
                        before=a->menu->selection;
                        SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+90*oh/160));
                        starts+=before!=0;valid&=!a->menu->selection && a->audio.mixer.starts==starts;
                        before=a->menu->selection;
                        SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
                        starts+=before!=selection;valid&=a->menu->selection==selection && a->audio.mixer.starts==starts;
                        /* Moving within the same label, leaving into blank
                         * space and returning, and clicking it stay quiet. */
                        SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+(x+1)*ow/240,oy+y*oh/160));
                        SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+ow/240,oy+oh/160));
                        SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
                        SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+x*ow/240,oy+y*oh/160));
                        valid&=a->menu_pressed==SSA_MENU_CONFIRM && a->menu->selection==selection &&
                            a->audio.mixer.starts==starts && !memcmp(&music,&a->audio.mixer.players[0],sizeof music);
                        LeaveCriticalSection(&a->audio.mutex);
                        if(!valid)return 0;
                        ++a->menu_mouse_checks;
                    }
                    else if(p==SSA_MENU_PASSWORD && a->password_test && !a->menu->leaving && a->menu->draw_page==SSA_MENU_PASSWORD){
                        unsigned index=((a->password_test-1)/PASSWORD_TEST_LEVELS)*12+(a->password_test-1)%PASSWORD_TEST_LEVELS+1;
                        const char *target=ssa_menu_password(a->menu,index);
                        size_t length;
                        if(!target)return 0;
                        /* Release menu navigation before original editor hold-repeat. */
                        SendMessageW(a->window,WM_KEYUP,VK_DOWN,0);SendMessageW(a->window,WM_KEYUP,VK_UP,0);
                        {unsigned j;
                            length=strlen(target);while(length && target[length-1]=='_')--length;
                            SendMessageW(a->window,WM_CHAR,'x',0);SendMessageW(a->window,WM_CHAR,8,0);
                            a->password_test_edges+=2;
                            for(j=0;j<length;++j){unsigned c=(unsigned char)target[j];
                                SendMessageW(a->window,WM_KEYDOWN,c,0);
                                if(a->menu_pressed)return 0;
                                SendMessageW(a->window,WM_CHAR,c>='A' && c<='Z'?c+32:c,0);
                                SendMessageW(a->window,WM_KEYUP,c,0);++a->password_test_edges;
                            }
                            if(strlen(a->menu->password)!=length || strncmp(a->menu->password,target,length))return 0;
                        }
                        if(a->original_renderer){SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);SendMessageW(a->window,WM_KEYUP,VK_RETURN,0);}
                        else{RECT bounds;int ox,oy,ow,oh;
                            if(!GetClientRect(a->window,&bounds) || !ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&ox,&oy,&ow,&oh))return 0;
                            SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ox+120*ow/240,oy+120*oh/160));
                            SendMessageW(a->window,WM_LBUTTONUP,0,MAKELPARAM(ox+120*ow/240,oy+120*oh/160));
                        }
                        ++a->password_test_edges;
                    }
                    else if((p==SSA_MENU_MAIN || p==SSA_MENU_SINGLE) && t==8)
                        SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                    else if(p==SSA_MENU_TITLE && a->menu->counter==96)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                    else if(p==SSA_MENU_DIFFICULTY && ((wanted_difficulty>=1 && t==4) || (wanted_difficulty==2 && t==6)))
                        SendMessageW(a->window,WM_KEYDOWN,VK_DOWN,0);
                    else if(p==SSA_MENU_DIFFICULTY && t==10)SendMessageW(a->window,WM_KEYDOWN,VK_RETURN,0);
                }
                {unsigned previous_music=a->menu->music_enabled,previous_language=a->menu->language,p=a->menu->page,t=a->menu_page_ticks;
                    unsigned held=(a->menu_held_keys&3?SSA_MENU_UP:0u)|(a->menu_held_keys&12?SSA_MENU_DOWN:0u);
                    int32_t old_y=a->menu->credits_y;
                    if(ssa_menu_step_input(a->menu,a->menu_pressed,held)!=1)return 0;
                    if(a->menu->clear_requested)save_change(a,SSA_SAVE_CLEAR,0);
                    else if(previous_language!=a->menu->language)save_change(a,SSA_SAVE_LANGUAGE,a->menu->language);
                    if(previous_music!=a->menu->music_enabled)++a->menu_toggle_count;
                    if(a->startup_test && a->menu_test && p==SSA_MENU_CREDITS){
                        if(t<70){int32_t speed=t>=2 && t<6?0:t>=6 && t<10?0x20000:0x8000;
                            if(a->menu->credits_speed!=speed || a->menu->credits_y!=(t?old_y:160*65536)-speed)return 0;
                        }else if(t==70){if(a->menu->credits_y!=old_y || !a->menu->credits_done)return 0;}
                        else{if(a->menu->page!=SSA_MENU_OPTIONS || a->menu->selection)return 0;a->menu_credits_test=2;}
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
                if(a->menu->done && !finish_menu(a))return 0;
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
                if(a->reveal_presented_at.QuadPart && elapsed_us(a->reveal_presented_at)>=
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
                if(game_tick(a)!=1){fprintf(stderr,"Native frame failed at%u, line%u\n",frames,a->game->error_line);return 0;}
                ++frames;
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
               (!a->password_test && !a->level_select_test && a->menu_wait_ticks!=(a->direct_level?0u:a->menu_test?288u:187u)) ||
               a->difficulty!=(a->menu_test?(a->menu_test-1)%3:a->password_test?(a->password_test-1)/PASSWORD_TEST_LEVELS:1) ||
               a->language!=(a->menu_test?(a->menu_test-1)/3:a->password_test?(a->password_test-1)%3:a->profile.data.language) ||
               (a->level_select_test && (a->level!=a->level_select_test-1 ||
                a->menu->loadout!=a->difficulty*12+a->level+1 || ssa_profile_unlocked(&a->profile.data,a->level)!=1)) ||
               a->game->damage.difficulty!=a->difficulty ||
               (a->password_test && (a->level!=(a->password_test-1)%PASSWORD_TEST_LEVELS+1 || !a->password_test_edges || !a->audio.device ||
                a->menu->loadout!=((a->password_test-1)/PASSWORD_TEST_LEVELS)*12+(a->password_test-1)%PASSWORD_TEST_LEVELS+2 ||
                a->level_music_checks!=a->map_views)) ||
               (a->menu_test && (a->menu_toggle_count!=2 || a->menu_credits_checks!=72 || a->menu_mouse_checks!=2 ||
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
                a->audio.mixer.errors || !a->audio_test_worker || !a->audio_test_mute ||
                a->audio_test_footsteps!=2 || a->audio_test_spatial!=2 || a->audio_test_music!=3 || a->audio_test_level_starts!=2 || a->audio_test_fade!=2))return 0;
            printf("{\"result\":\"pass\",\"startup_path\":\"normal\",\"frames\":%u,\"raw_input_registered\":true,"
                "\"buffered_presents\":%llu,\"restarts\":%u,\"level\":%u,\"transition_steps\":%u,\"audio_device\":%u,\"audio_buffers\":%llu,\"audio_recycled\":%llu,"
                "\"audio_position\":%u,\"audio_notes\":%llu,\"audio_errors\":%u,\"audio_worker_progress\":%u,\"audio_mute_toggle\":%u,\"audio_footsteps\":%u,\"audio_spatial\":%u,\"audio_music\":%u,\"audio_level_starts\":%u,\"audio_fade\":%u,"
                "\"map_views\":%u,\"map_starts\":%u,\"map_frozen_ticks\":%u,\"map_music_checks\":%u,\"level_music_checks\":%u,\"map_black_presents\":%u,\"map_fade_presents\":%u,"
                "\"map_black_hold_ticks\":%u,\"level_fade_ticks\":%u,\"level_fade_starts\":%u,\"level_fade_presents\":%u,\"black_hold_min_us\":%llu,\"level_fade_min_us\":%llu,"
                "\"menu_starts\":%u,\"menu_frozen_ticks\":%u,\"difficulty\":%u,\"language\":%u,\"menu_music_toggles\":%u,\"menu_credits_checks\":%u,\"password_edges\":%u,\"password_loadout\":%u,\"reveal_weapon_checks\":%u,"
                "\"reveal_game_ticks\":%u,\"reveal_move_checks\":%u,\"reveal_hud_checks\":%u,\"reveal_entry_checks\":%u,\"menu_mouse_checks\":%u,\"menu_music_continuity\":%u,\"level_starter_checks\":%u}\n",frames,(unsigned long long)a->presents,a->startup_restarts,
                a->level,a->transition_stage,a->audio.device?1u:0u,(unsigned long long)a->audio.submitted,(unsigned long long)a->audio.recycled,
                ssa_win32_audio_position(&a->audio),(unsigned long long)a->audio.mixer.notes,a->audio.mixer.errors,a->audio_test_worker,a->audio_test_mute,a->audio_test_footsteps,a->audio_test_spatial,a->audio_test_music,a->audio_test_level_starts,a->audio_test_fade,
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
    const wchar_t *path=NULL,*shot=NULL,*save_path=NULL;int i,result=1,registered=0,raw_registered=0;
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
        else if(!wcscmp(argv[i],L"--transition-test"))a.transition_test=1;
        else if(!wcscmp(argv[i],L"--menu-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>8)goto usage;a.menu_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--password-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>=3*PASSWORD_TEST_LEVELS)goto usage;a.password_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--level-select-test") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>=SSA_NATIVE_CAMPAIGN_LEVELS)goto usage;a.level_select_test=(unsigned)n+1;}
        else if(!wcscmp(argv[i],L"--mute"))a.mute=1;
        else if(!wcscmp(argv[i],L"--no-music"))a.no_music=1;
        else if(!wcscmp(argv[i],L"--smoke-frames") && i+1<argc){
            wchar_t *end;unsigned long n;errno=0;n=wcstoul(argv[++i],&end,10);
            if(errno || argv[i][0]<L'0' || argv[i][0]>L'9' || end==argv[i] || *end || !n)goto usage;a.smoke_frames=(unsigned)n;
        }
        else if(!wcscmp(argv[i],L"--resolution") && i+1<argc){if(!resolution(&a,argv[++i]))goto usage;}
        else if(!wcscmp(argv[i],L"--original-renderer"))a.original_renderer=1;
        else if(!wcscmp(argv[i],L"--level") && i+1<argc){wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);if(!argv[i][0] || *end || n>=SSA_NATIVE_CAMPAIGN_LEVELS)goto usage;a.level=(unsigned)n;a.direct_level=1;}
        else if(!wcscmp(argv[i],L"--snapshot") && i+1<argc)shot=argv[++i];
        else if(!wcscmp(argv[i],L"--save-file") && i+1<argc)save_path=argv[++i];
        else if(argv[i][0]!=L'-' && !path)path=argv[i];else goto usage;
    }
    if(!path || (!a.smoke && (a.smoke_frames!=600 || a.smoke_resize || a.smoke_look || a.smoke_present_each)))goto usage;
    if(a.startup_test && a.smoke)goto usage;
    if(a.audio_test && !a.startup_test)goto usage;
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
    if(a.direct_level?
       ssa_native_game_init_loadout(a.game,a.rom,a.rom_size,a.level,a.difficulty*12+a.level+1)!=1:
       ssa_native_game_init_campaign(a.game,a.rom,a.rom_size,a.level,a.difficulty,NULL)!=1){
        fprintf(stderr,"Native init failed line%u\n",a.game->error_line);goto done;}
    if(ssa_native_game_carry_capture(&a.entry,a.game)!=1)goto done;
    if(!a.smoke && (!a.startup_test || a.audio_test || a.transition_test || a.menu_test || a.password_test)){
        if(ssa_win32_audio_open(&a.audio,a.rom,a.rom_size,a.level,a.mute,a.no_music)){
            SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a.audio,
                ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
            if(ssa_native_game_bind_audio(a.game,&ops)<0)goto done;
        }else fprintf(stderr,"Audio output unavailable (%u); continuing silently.\n",a.audio.last_error);
    }
    a.bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a.bitmap.bmiHeader.biWidth=240;a.bitmap.bmiHeader.biHeight=-160;
    a.bitmap.bmiHeader.biPlanes=1;a.bitmap.bmiHeader.biBitCount=32;a.bitmap.bmiHeader.biCompression=BI_RGB;
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.lpszClassName=CLASS_NAME;
    if(!RegisterClassExW(&wc))goto done;registered=1;
    a.window=CreateWindowExW(0,CLASS_NAME,TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,900,NULL,NULL,instance,&a);
    if(!a.window)goto done;
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
    if(a.back_dc){if(a.back_original)SelectObject(a.back_dc,a.back_original);
        if(a.back_bitmap)DeleteObject(a.back_bitmap);DeleteDC(a.back_dc);}
    if(a.game){ssa_native_game_free(a.game);free(a.game);}free(a.map_view);free(a.menu);free(a.rom);return result;
usage:
    fwprintf(stderr,L"Usage: ssa_native_game.exe ROM.gba [--resolution auto|WIDTHxHEIGHT] [--original-renderer] [--mute] [--no-music] [--level 0..%u] [--smoke] [--snapshot output.bmp]\nNative campaign prototype (%u levels); M toggles audio. Remaining campaign is unfinished.\n",SSA_NATIVE_CAMPAIGN_LEVELS-1,SSA_NATIVE_CAMPAIGN_LEVELS);return 2;
}
