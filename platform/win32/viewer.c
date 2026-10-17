/* Diagnostic native world/actor viewer. This is not the complete game. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>
#include "ssa/world_scene.h"
#include "ssa/player_spawn.h"
#include "ssa/pc_input.h"
#include "ssa/sprite_assets.h"
#include "ssa/map_actors.h"
#include "ssa/actor_scene.h"
#include "ssa/actor_raster.h"
#include "ssa/player_frame.h"
#include "ssa/hud_scene.h"

typedef struct Viewer {
    HWND window;
    unsigned level,smoke,captured,raw_registered,running,failed,loaded;
    unsigned frames,airborne,third_person,chase_selected,changed_frames;
    unsigned walking,actor_commands,actor_changed_pixels,body_frames,body_pixels;
    unsigned hud_draws,weapon_draws,presentation_pixels;
    uint32_t previous_hash,first_hash;
    SsaMathTables math;
    SsaVisibilityTables visibility_tables;
    SsaWallRenderTables wall_tables;
    SsaTextureAssets textures;
    SsaSpriteAssets sprites;
    SsaSkyAssets sky;
    SsaMapAssets map;
    SsaMapTextureBindings bindings;
    SsaWorld world;
    SsaSlide slide;
    SsaMapCollision view_collision;
    SsaPlayerSpawn players[4];
    SsaJump jump;
    SsaCamera eye;
    SsaCameraFrame camera_frame;
    SsaView view;
    SsaViewPose pose;
    SsaRenderView render_view;
    SsaWorldScene scene;
    SsaSpawnDefinitions definitions;
    SsaActorAnimations animation_data;
    SsaActorAssets actor_assets;
    SsaMapActors map_actors;
    SsaActorAnimation player_animation;
    SsaActorScene actor_scene;
    SsaHudScene hud_scene;
    SsaActorSource sources[SSA_ENTITY_POOL_CAPACITY];
    SsaPcInput input;
    uint16_t rgb555[SSA_DISPLAY_PIXELS];
    uint32_t dib[SSA_DISPLAY_PIXELS];
    BITMAPINFO bitmap;
} Viewer;

static const wchar_t CLASS_NAME[]=L"SSANativeGeometryViewer";
static const wchar_t TITLE[]=L"Serious Sam Advance - INCOMPLETE native viewer | WASD/mouse, Space jump, H camera, Esc release";
static int32_t signed_bits(uint32_t n)
{return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}

static int rom_identity(const uint8_t *rom,size_t size)
{
    static const uint8_t expected[20]={0x4f,0xf4,0x8d,0x8c,0x18,0xb7,0x0a,0x51,0x84,0x75,
        0x8f,0x19,0x17,0x34,0x3a,0xe8,0x5a,0x70,0x85,0x8e};
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    uint8_t digest[20];int result=0;
    if(size!=8u*1024u*1024u)return 0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,NULL,0)<0)goto done;
    if(BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)<0)goto done;
    if(BCryptHashData(hash,(PUCHAR)rom,(ULONG)size,0)<0)goto done;
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
    result=memcmp(digest,expected,sizeof(expected))==0;
done:
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    return result;
}
static uint8_t *read_rom(const wchar_t *path,size_t *size)
{
    FILE *f=_wfopen(path,L"rb");uint8_t *rom;long length;
    if(!f)return NULL;
    if(fseek(f,0,SEEK_END) || (length=ftell(f))!=8*1024*1024) {fclose(f);return NULL;}
    rewind(f);rom=(uint8_t *)malloc((size_t)length);
    if(!rom || fread(rom,1,(size_t)length,f)!=(size_t)length) {free(rom);fclose(f);return NULL;}
    fclose(f);
    if(!rom_identity(rom,(size_t)length)){free(rom);return NULL;}
    *size=(size_t)length;return rom;
}
static int render_event(void *context,SsaRenderView *view,SsaRenderViewEvent event,uint32_t argument)
{
    Viewer *v=(Viewer *)context;
    if(event==SSA_RENDER_VIEW_CLEAR)return ssa_world_scene_clear(&v->scene,(uint8_t)argument);
    if(event==SSA_RENDER_VIEW_WORLD)
        return ssa_world_scene_draw(&v->scene,&view->camera,(uint8_t)v->frames,
            v->players[0].motion.movement.heading);
    /* This viewer never selects automap/overlay. Unsupported stages are errors. */
    return -1;
}
static int draw(Viewer *v)
{
    SsaPlayerSpawn *p=&v->players[0];SsaRenderViewOps ops={render_event,v};
    const uint8_t *pixels;unsigned i;size_t count=v->map_actors.plan.object_count;
    uint32_t hash=2166136261u;uint8_t before[14400];
    if(!ssa_view_pose(&v->view,&p->entity.body,&v->eye,p->motion.movement.heading,
        &v->math,&v->view_collision,&v->pose))return 0;
    if(ssa_render_view_pose(&v->render_view,&v->pose,p->motion.movement.heading,&v->map)<0 ||
       ssa_render_view_step(&v->render_view,&ops)<0)return 0;
    pixels=ssa_world_scene_pixels(&v->scene);if(!pixels)return 0;
    if(v->smoke)memcpy(before,pixels,sizeof(before));
    for(i=0;i<count;++i) {
        const SsaSpawnObject *o=ssa_map_actor_object(&v->map_actors,i);
        const SsaActorAnimation *a=ssa_map_actor_animation(&v->map_actors,i);
        if(!o || !a)return 0;
        v->sources[i]=(SsaActorSource){&o->entity,a->animation,a->frame,
            o->entity.body.flags,o->facing,o->entity.player_index};
    }
    v->sources[count]=(SsaActorSource){&p->entity,v->player_animation.animation,
        v->player_animation.frame,p->entity.body.flags,p->motion.facing,p->entity.player_index};
    /* H exposes only the rendering snapshot; canonical gameplay flags stay intact. */
    if(v->pose.third_person)v->sources[count].flags&=UINT16_C(0xff7f);
    if(ssa_actor_scene_draw(&v->actor_scene,&v->render_view.camera,v->frames,
        v->sources,count+1)<0)return 0;
    if(v->smoke) {
        uint8_t body[14400]={0};unsigned visible=0;
        SsaActorQueue *q=&v->actor_scene.queue;
        v->actor_commands+=q->draw_count;
        for(i=0;i<14400;++i)if(before[i]!=pixels[i])++v->actor_changed_pixels;
        for(i=0;i<q->draw_count;++i) {
            const SsaActorRecord *a=&q->actors[q->draw_order[i]];
            const SsaActorProjection *r=&q->projections[a->projection];
            if(r->animation==v->player_animation.animation ||
               r->animation==v->player_animation.animation+5) {
                if(!v->pose.third_person)return 0;
                if(ssa_actor_raster_draw(&v->actor_assets,r,q->clip_words+a->clip_offset,
                    SSA_ACTOR_CLIP_WORDS-a->clip_offset,body,sizeof(body))<0)return 0;
            }
        }
        for(i=0;i<14400;++i)if(body[i])++visible;
        if(visible){++v->body_frames;v->body_pixels+=visible;}
    }
    /* Diagnostic projection: health comes from the spawned entity; score/time
     * remain their initialized zero state until actual game integration. */
    v->hud_scene.state.health=p->entity_health;
    v->hud_scene.state.armor=p->inventory.stat_c4;
    v->hud_scene.state.local_flags=p->motion.movement.flags;
    v->hud_scene.control.keys=p->motion.movement.flags;
    if(ssa_hud_scene_step_idle(&v->hud_scene,1,0,v->pose.third_person)<0 ||
       ssa_hud_scene_draw(&v->hud_scene,pixels,14400,
        (unsigned)ssa_texture_assets_family(v->level),v->rgb555,SSA_DISPLAY_PIXELS)<0 ||
       ssa_display_rgb888(v->rgb555,SSA_DISPLAY_PIXELS,v->dib,SSA_DISPLAY_PIXELS)<0)return 0;
    if(v->smoke){
        uint16_t background[SSA_DISPLAY_PIXELS];unsigned changed=0;
        if(ssa_display_compose(&v->hud_scene.registers,pixels,14400,
            v->sprites.background_palette[ssa_texture_assets_family(v->level)],v->sprites.palette,
            NULL,NULL,0,background,SSA_DISPLAY_PIXELS)<0)return 0;
        for(i=0;i<SSA_DISPLAY_PIXELS;++i)if(background[i]!=v->rgb555[i])++changed;
        if(changed){++v->hud_draws;v->presentation_pixels+=changed;}
        if(!v->pose.third_person && v->hud_scene.weapon_object->y==0x5000 &&
           (v->hud_scene.weapon_object->flags&1))++v->weapon_draws;
    }
    for(i=0;i<SSA_DISPLAY_PIXELS;++i)hash=(hash^v->dib[i])*16777619u;
    if(!v->frames)v->first_hash=hash;
    else if(hash!=v->previous_hash)++v->changed_frames;
    v->previous_hash=hash;return 1;
}
static int step(Viewer *v)
{
    SsaPcInputSample sample;SsaPlayerSpawn *p=&v->players[0];
    const SsaEntity *players[1]={&p->entity};
    SsaMotionFrame frame={players,1,1,0,0};SsaMotionOptions options;
    if(ssa_pc_input_sample(&v->input,&sample)<0)return 0;
    p->motion.movement.keys=sample.enabled?sample.keys:0;
    if(sample.enabled)p->motion.movement.heading=(uint16_t)((uint32_t)p->motion.movement.heading+(uint32_t)sample.yaw_delta);
    if(sample.enabled && sample.view_toggle) {
        ssa_view_key(&v->view,'H',1,0,1);ssa_view_key(&v->view,'H',0,0,1);
    }
    options.jump=&v->jump;options.jump_down=sample.jump_down;
    options.controls_enabled=sample.enabled;options.retain_heading=1;
    if(ssa_player_motion_world_step(&p->motion,&p->entity,&p->vertical,&v->world,
        &v->slide,&v->math,&frame,&options)<0)return 0;
    if(!v->walking && ssa_player_starts_moving(&p->motion.movement)) {
        v->walking=1;if(ssa_actor_animation_set(&v->player_animation,0x66,4)<0)return 0;
    } else if(v->walking && ssa_player_stops_moving(&p->motion.movement)) {
        v->walking=0;if(ssa_actor_animation_set(&v->player_animation,0x65,4)<0)return 0;
    }
    if(ssa_map_actors_step(&v->map_actors,1)<0 ||
       ssa_actor_animation_step(&v->player_animation,&v->animation_data,p->entity.body.flags,1)<0)return 0;
    v->camera_frame.counter=(uint16_t)v->frames;
    v->camera_frame.is_local=1;v->camera_frame.footsteps_muted=1;
    (void)ssa_camera_step(&p->entity.body,&p->vertical,&v->eye,&v->camera_frame,&v->math);
    if(p->entity.body.z>(int32_t)p->entity.body.floor*65536)++v->airborne;
    if(!draw(v))return 0;
    if(v->pose.third_person)++v->third_person;
    if(v->view.third_person)++v->chase_selected;
    ++v->frames;return 1;
}
static int initialize(Viewer *v,const wchar_t *path)
{
    size_t size=0;uint8_t *rom=read_rom(path,&size);SsaSpawnPlan plan={0};int family;
    if(!rom){fprintf(stderr,"Could not read the authorized AENP ROM (8 MiB, expected SHA-1).\n");return 0;}
#define LOAD(condition) do {if(!(condition)){fprintf(stderr,"Native initialization failed at line%d.\n",__LINE__);free(rom);return 0;}}while(0)
    LOAD(ssa_math_tables_load(&v->math,rom,size));
    LOAD(ssa_map_assets_load(&v->map,rom,size,v->level));
    LOAD(ssa_texture_assets_load(&v->textures,rom,size)==1);
    LOAD(ssa_sprite_assets_load(&v->sprites,rom,size)==1);
    LOAD(ssa_sky_assets_load(&v->sky,rom,size)==1);
    LOAD(ssa_visibility_tables_load(&v->visibility_tables,rom,size)==1);
    LOAD(ssa_wall_render_tables_load(&v->wall_tables,rom,size)==1);
    LOAD(ssa_spawn_definitions_load(&v->definitions,rom,size));
    LOAD(ssa_actor_animations_load(&v->animation_data,rom,size)==1);
    LOAD(ssa_actor_assets_load(&v->actor_assets,rom,size)==1);
    family=ssa_texture_assets_family(v->level);LOAD(family>=0);
    LOAD(ssa_map_texture_bindings_init(&v->bindings,&v->map,&v->textures,(unsigned)family)==1);
    LOAD(ssa_world_init(&v->world,&v->map,SSA_ENTITY_POOL_CAPACITY));LOAD(ssa_slide_init(&v->slide,&v->map));
    LOAD(ssa_map_collision_init(&v->view_collision,&v->map));
    LOAD(ssa_map_actors_init(&v->map_actors,&v->world,&v->definitions,&v->animation_data,v->level)==1);
    LOAD(ssa_map_actors_step(&v->map_actors,0)==1);
    /* Original map allocation precedes player construction. */
    plan.map=&v->map;LOAD(ssa_players_spawn(v->players,&v->world,&plan,1,0));
    LOAD(v->players[0].active);
    LOAD(ssa_actor_animation_set(&v->player_animation,v->players[0].animation,
        v->players[0].animation_ticks)==1);
    LOAD(ssa_actor_animation_step(&v->player_animation,&v->animation_data,
        v->players[0].entity.body.flags,0)==1);
    LOAD(ssa_world_scene_init(&v->scene,&v->bindings,&v->math,&v->visibility_tables,
        &v->wall_tables,&v->sky,v->level)==1);
    LOAD(ssa_actor_scene_init(&v->actor_scene,&v->scene,&v->world,&v->actor_assets,&v->math)==1);
    LOAD(ssa_hud_scene_init(&v->hud_scene,&v->players[0].inventory,&v->players[0].entity,
        &v->sprites,&v->math,rom,size,v->level,0)==1);
    LOAD(ssa_pc_input_init(&v->input,64)==1);
#undef LOAD
    free(rom);ssa_pc_input_enable(&v->input,0);ssa_pc_input_focus(&v->input,0);
    ssa_jump_init(&v->jump);ssa_view_init(&v->view);
    v->eye.eye_z=signed_bits((uint32_t)v->players[0].entity.body.z+(uint32_t)v->players[0].vertical.eye_offset);
    v->render_view.math=&v->math;v->render_view.players[0].flags=4;v->render_view.ticks=1;
    v->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    v->bitmap.bmiHeader.biWidth=SSA_DISPLAY_WIDTH;v->bitmap.bmiHeader.biHeight=-SSA_DISPLAY_HEIGHT;
    v->bitmap.bmiHeader.biPlanes=1;v->bitmap.bmiHeader.biBitCount=32;
    v->bitmap.bmiHeader.biCompression=BI_RGB;v->loaded=1;
    return draw(v);
}
static void teardown(Viewer *v)
{
    if(v->hud_scene.loaded)(void)ssa_hud_scene_free(&v->hud_scene);
    if(v->actor_scene.loaded)(void)ssa_actor_scene_free(&v->actor_scene);
    if(v->scene.loaded)(void)ssa_world_scene_free(&v->scene);
    if(v->map_actors.loaded)(void)ssa_map_actors_free(&v->map_actors);
    ssa_map_collision_free(&v->view_collision);ssa_slide_free(&v->slide);ssa_world_free(&v->world);
    ssa_map_texture_bindings_free(&v->bindings);ssa_texture_assets_free(&v->textures);
    ssa_sprite_assets_free(&v->sprites);ssa_map_assets_free(&v->map);
    ssa_actor_assets_free(&v->actor_assets);
}
static void release_controls(Viewer *v)
{
    unsigned held=v->captured;v->captured=0;
    ssa_pc_input_focus(&v->input,0);ssa_pc_input_enable(&v->input,0);
    ssa_view_focus_lost(&v->view);v->jump.was_down=0;
    if(held) {
        ClipCursor(NULL);
        if(GetCapture()==v->window)ReleaseCapture();
        SetCursor(LoadCursorW(NULL,IDC_ARROW));
    }
}
static int confine(Viewer *v)
{
    RECT rect;POINT corners[2];
    if(v->smoke || GetForegroundWindow()!=v->window || IsIconic(v->window))return 0;
    if(!GetClientRect(v->window,&rect) || rect.right<=0 || rect.bottom<=0)return 0;
    corners[0].x=rect.left;corners[0].y=rect.top;corners[1].x=rect.right;corners[1].y=rect.bottom;
    if(!ClientToScreen(v->window,&corners[0]) || !ClientToScreen(v->window,&corners[1]))return 0;
    rect.left=corners[0].x;rect.top=corners[0].y;rect.right=corners[1].x;rect.bottom=corners[1].y;
    return ClipCursor(&rect)!=0;
}
static void capture_controls(Viewer *v)
{
    if(!confine(v))return;
    v->captured=1;SetCapture(v->window);
    if(GetCapture()!=v->window){release_controls(v);return;}
    SetCursor(NULL);
    ssa_pc_input_focus(&v->input,1);ssa_pc_input_enable(&v->input,1);
}
static int present(Viewer *v,HDC dc)
{
    RECT client;int width,height,out_width,out_height,result;
    if(!GetClientRect(v->window,&client))return 0;
    width=client.right-client.left;height=client.bottom-client.top;
    if(width<=0 || height<=0)return 1;
    FillRect(dc,&client,(HBRUSH)GetStockObject(BLACK_BRUSH));
    out_width=width;out_height=(int)((int64_t)width*2/3);
    if(out_height>height){out_height=height;out_width=(int)((int64_t)height*3/2);}
    SetStretchBltMode(dc,COLORONCOLOR);
    result=StretchDIBits(dc,(width-out_width)/2,(height-out_height)/2,out_width,out_height,
        0,0,SSA_DISPLAY_WIDTH,SSA_DISPLAY_HEIGHT,
        v->dib,&v->bitmap,DIB_RGB_COLORS,SRCCOPY);
    return result!=0 && result!=(int)GDI_ERROR;
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    Viewer *v=(Viewer *)(uintptr_t)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_NCCREATE) {
        const CREATESTRUCTW *create=(const CREATESTRUCTW *)lp;
        v=(Viewer *)create->lpCreateParams;v->window=window;
        SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)v);
    }
    if(!v)return DefWindowProcW(window,message,wp,lp);
    switch(message) {
    case WM_CLOSE:release_controls(v);DestroyWindow(window);return 0;
    case WM_DESTROY:release_controls(v);v->running=0;PostQuitMessage(0);return 0;
    case WM_KILLFOCUS:case WM_ENTERSIZEMOVE:case WM_ENTERMENULOOP:case WM_CANCELMODE:
        if(!v->smoke)release_controls(v);break;
    case WM_CAPTURECHANGED:
        if(v->captured && (HWND)lp!=window)release_controls(v);break;
    case WM_SIZE:case WM_MOVE:
        if(v->captured && !confine(v))release_controls(v);
        InvalidateRect(window,NULL,FALSE);break;
    case WM_LBUTTONDOWN:
        if(!v->smoke && !v->captured)capture_controls(v);
        return 0;
    case WM_KEYDOWN:case WM_KEYUP:
        if(wp==VK_ESCAPE && message==WM_KEYDOWN) {release_controls(v);return 0;}
        if(v->captured && wp<128)
            (void)ssa_pc_input_key(&v->input,(int)wp,message==WM_KEYDOWN,
                message==WM_KEYDOWN && (lp&((LPARAM)1<<30))!=0);
        return 0;
    case WM_INPUT: {
        RAWINPUT raw;UINT size=sizeof(raw),got;
        got=GetRawInputData((HRAWINPUT)lp,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER));
        if(got!=(UINT)-1 && got>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE) &&
           raw.header.dwType==RIM_TYPEMOUSE && !(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) &&
           v->captured && GetForegroundWindow()==window)
            (void)ssa_pc_input_relative(&v->input,(int32_t)raw.data.mouse.lLastX,(int32_t)raw.data.mouse.lLastY);
        /* Foreground WM_INPUT requires DefWindowProc cleanup after retrieval. */
        return DefWindowProcW(window,message,wp,lp);
    }
    case WM_SETCURSOR:
        if(v->captured && LOWORD(lp)==HTCLIENT){SetCursor(NULL);return TRUE;}
        break;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
        if(v->loaded && !present(v,dc)){v->failed=1;v->running=0;}
        EndPaint(window,&paint);return 0;
    }
    default:break;
    }
    return DefWindowProcW(window,message,wp,lp);
}
static int snapshot(Viewer *v,const wchar_t *path)
{
    FILE *f;BITMAPFILEHEADER header={0};int ok;
    if(!path)return 1;
    f=_wfopen(path,L"wb");if(!f)return 0;
    header.bfType=0x4d42;header.bfOffBits=(DWORD)(sizeof(header)+sizeof(BITMAPINFOHEADER));
    header.bfSize=header.bfOffBits+(DWORD)sizeof(v->dib);
    ok=fwrite(&header,1,sizeof(header),f)==sizeof(header) &&
       fwrite(&v->bitmap.bmiHeader,1,sizeof(BITMAPINFOHEADER),f)==sizeof(BITMAPINFOHEADER) &&
       fwrite(v->dib,1,sizeof(v->dib),f)==sizeof(v->dib);
    if(fclose(f))ok=0;return ok;
}
static int smoke(Viewer *v,const wchar_t *chase_snapshot)
{
    unsigned i,distinct=0,seen[256]={0};HDC dc;const uint8_t *pixels;
    ssa_pc_input_focus(&v->input,1);ssa_pc_input_enable(&v->input,1);
    for(i=0;i<240;++i) {
        if(i==0)ssa_pc_input_key(&v->input,'W',1,0);
        if(i==4 || i==124){ssa_pc_input_key(&v->input,' ',1,0);ssa_pc_input_key(&v->input,' ',0,0);}
        if(i==24)ssa_pc_input_key(&v->input,'W',0,0);
        if(i==32)ssa_pc_input_key(&v->input,'D',1,0);
        if(i==48)ssa_pc_input_key(&v->input,'D',0,0);
        if(i==80 || i==180){ssa_pc_input_key(&v->input,'H',1,0);ssa_pc_input_key(&v->input,'H',0,0);}
        if(i>=60 && i<76)ssa_pc_input_relative(&v->input,1,0);
        if(i==200) {
            ssa_pc_input_key(&v->input,'W',1,0);ssa_pc_input_relative(&v->input,100,0);
            release_controls(v);
            if(v->input.down || v->input.pending_yaw || v->input.focused)return 0;
        }
        if(i==204){ssa_pc_input_focus(&v->input,1);ssa_pc_input_enable(&v->input,1);}
        if(!step(v)){fprintf(stderr,"Smoke native step failed at frame%u (level%u).\n",i,v->level);return 0;}
        if(i==100 && !snapshot(v,chase_snapshot))return 0;
    }
    dc=GetDC(v->window);if(!dc)return 0;
    i=(unsigned)present(v,dc);ReleaseDC(v->window,dc);if(!i)return 0;
    pixels=ssa_world_scene_pixels(&v->scene);
    for(i=0;i<14400;++i)if(!seen[pixels[i]]){seen[pixels[i]]=1;++distinct;}
    if(!v->changed_frames || v->chase_selected!=100 || distinct<2 || v->captured)return 0;
    printf("{\"result\":\"pass\",\"level\":%u,\"frames\":%u,\"airborne_frames\":%u,"
        "\"third_person_frames\":%u,\"chase_selected_frames\":%u,\"changed_frames\":%u,\"distinct_indices\":%u,"
        "\"actor_commands\":%u,\"actor_changed_pixels\":%u,\"body_frames\":%u,\"body_pixels\":%u,"
        "\"hud_draws\":%u,\"idle_weapon_draws\":%u,\"presentation_pixels\":%u,\"display_width\":240,\"display_height\":160,"
        "\"map_objects\":%u,\"first_hash\":%u,\"last_hash\":%u,\"hidden_window\":true,\"input_injected\":false}\n",
        v->level,v->frames,v->airborne,v->third_person,v->chase_selected,v->changed_frames,distinct,
        v->actor_commands,v->actor_changed_pixels,v->body_frames,v->body_pixels,
        v->hud_draws,v->weapon_draws,v->presentation_pixels,
        (unsigned)v->map_actors.plan.object_count,v->first_hash,v->previous_hash);
    return 1;
}
static int interactive(Viewer *v)
{
    LARGE_INTEGER frequency,previous,now;double accumulator=0;MSG message;
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&previous))return 0;
    v->running=1;ShowWindow(v->window,SW_SHOW);UpdateWindow(v->window);
    if(v->failed)return 0;
    while(v->running) {
        while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT){v->running=0;break;}
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(!v->running)break;
        if(!QueryPerformanceCounter(&now))return 0;
        accumulator+=(double)(now.QuadPart-previous.QuadPart)/(double)frequency.QuadPart;previous=now;
        if(accumulator>8.0/60.0)accumulator=8.0/60.0;
        while(accumulator>=1.0/60.0) {
            if(!step(v)){fprintf(stderr,"Native geometry update failed at frame%u.\n",v->frames);return 0;}
            accumulator-=1.0/60.0;InvalidateRect(v->window,NULL,FALSE);
        }
        MsgWaitForMultipleObjectsEx(0,NULL,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    return !v->failed;
}
int wmain(int argc,wchar_t **argv)
{
    Viewer *v;const wchar_t *rom_path=NULL,*snapshot_path=NULL,*chase_snapshot=NULL;unsigned level=0,is_smoke=0;
    WNDCLASSEXW wc={0};RAWINPUTDEVICE raw={0};int i,result=1;HINSTANCE instance=GetModuleHandleW(NULL);
    for(i=1;i<argc;++i) {
        if(!wcscmp(argv[i],L"--smoke"))is_smoke=1;
        else if(!wcscmp(argv[i],L"--level") && i+1<argc) {
            wchar_t *end;unsigned long n=wcstoul(argv[++i],&end,10);
            if(!argv[i][0] || *end || n>=16)goto usage;level=(unsigned)n;
        } else if(!wcscmp(argv[i],L"--snapshot") && i+1<argc)snapshot_path=argv[++i];
        else if(!wcscmp(argv[i],L"--snapshot-chase") && i+1<argc)chase_snapshot=argv[++i];
        else if(argv[i][0]!=L'-' && !rom_path)rom_path=argv[i];
        else goto usage;
    }
    if(!rom_path || (chase_snapshot && !is_smoke))goto usage;
    v=(Viewer *)calloc(1,sizeof(*v));if(!v)return 1;v->level=level;v->smoke=is_smoke;
    if(!initialize(v,rom_path))goto done;
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=window_proc;wc.hInstance=instance;
    wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.lpszClassName=CLASS_NAME;
    if(!RegisterClassExW(&wc))goto done;
    v->window=CreateWindowExW(0,CLASS_NAME,TITLE,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,900,700,NULL,NULL,instance,v);
    if(!v->window)goto registered;
    if(!is_smoke) {
        raw.usUsagePage=1;raw.usUsage=2;raw.hwndTarget=v->window;
        if(!RegisterRawInputDevices(&raw,1,sizeof(raw)))goto window_done;
        v->raw_registered=1;
        fprintf(stderr,"INCOMPLETE native viewer. Original actors, H Sam body, HUD and idle starting weapon; no AI, firing/combat or audio.\n"
            "Click to capture. WASD move, mouse horizontal yaw, Space jump, H camera, Esc release.\n");
    }
    if((is_smoke?smoke(v,chase_snapshot):interactive(v)) && snapshot(v,snapshot_path))result=0;
window_done:
    release_controls(v);
    if(v->raw_registered) {
        raw.dwFlags=RIDEV_REMOVE;raw.hwndTarget=NULL;
        if(!RegisterRawInputDevices(&raw,1,sizeof(raw)))result=1;
    }
    if(IsWindow(v->window))DestroyWindow(v->window);
registered:
    UnregisterClassW(CLASS_NAME,instance);
done:
    teardown(v);free(v);return result;
usage:
    fwprintf(stderr,L"Usage: ssa_native_viewer.exe ROM.gba [--level 0..15] [--smoke] [--snapshot output.bmp] [--snapshot-chase smoke.bmp]\n");
    return 2;
}
