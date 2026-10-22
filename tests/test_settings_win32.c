/* Actual frontend input/save/audio adapter with a hidden window. */
#include "../platform/win32/audio_win32.h"
static unsigned frontend_audio_applies;
static int tracked_audio_settings(SsaWin32Audio *a,const SsaSettings *settings)
{++frontend_audio_applies;return ssa_win32_audio_settings(a,settings);}
#define ssa_win32_audio_settings tracked_audio_settings
#define wmain embedded_settings_game_entry
#include "../platform/win32/game.c"
#undef wmain
#undef ssa_win32_audio_settings
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows settings line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void key(App *a,unsigned k){
    SendMessageW(a->window,WM_KEYDOWN,k,0);SendMessageW(a->window,WM_KEYUP,k,0);
    CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;settings_change(a);
    CHECK(ssa_menu_step(a->menu,0)==1);
}
static void menu_select(App *a,unsigned row){
    unsigned guard=0;
    while(a->menu->selection!=row && guard++<11)key(a,a->menu->selection<row?VK_DOWN:VK_UP);
    CHECK(a->menu->selection==row);
}
static void game_h(App *a){
    unsigned menu=a->menu_active,captured=a->captured;
    /* Exercise the real captured-key route without grabbing physical input. */
    a->menu_active=0;a->captured=1;
    SendMessageW(a->window,WM_KEYDOWN,'H',0);SendMessageW(a->window,WM_KEYUP,'H',0);
    a->captured=captured;a->menu_active=menu;CHECK(game_tick(a)==1);
}
static void aspect_checks(App *a){
    static const unsigned sizes[3][2]={{854,480},{640,480},{480,640}};
    RECT saved;unsigned i,mode,aspect,counter=a->game->frame_game.counter;
    unsigned rw=a->render_width,rh=a->render_height;
    CHECK(GetWindowRect(a->window,&saved));
    a->menu_active=0;a->render_width=a->render_height=0;
    for(i=0;i<3;++i){RECT client;
        CHECK(SetWindowPos(a->window,NULL,80,80,(int)sizes[i][0],(int)sizes[i][1],SWP_NOZORDER|SWP_NOACTIVATE));
        CHECK(GetClientRect(a->window,&client));
        for(mode=0;mode<2;++mode)for(aspect=0;aspect<2;++aspect){int x,y,w,h;HDC dc;unsigned xx,yy,bad=0,lit=0;wchar_t path[128];
            a->original_renderer=mode;a->game->gba_view=mode;a->settings.aspect_ratio=aspect;
            CHECK(render(a));
            CHECK(ssa_ui_viewport(client.right,client.bottom,240,160,&x,&y,&w,&h));
            CHECK(a->bitmap.bmiHeader.biWidth==(mode?240:aspect?w:client.right));
            CHECK(-a->bitmap.bmiHeader.biHeight==(mode?160:aspect?h:client.bottom));
            CHECK(ssa_ui_viewport(client.right,client.bottom,a->bitmap.bmiHeader.biWidth,
                -a->bitmap.bmiHeader.biHeight,&x,&y,&w,&h));
            dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);
            for(yy=0;yy<(unsigned)client.bottom;++yy)for(xx=0;xx<(unsigned)client.right;++xx){
                unsigned c=a->back_pixels[yy*(unsigned)client.right+xx]&0xffffff;
                if(xx<(unsigned)x || xx>=(unsigned)(x+w) || yy<(unsigned)y || yy>=(unsigned)(y+h))bad+=c!=0;
                else lit+=c!=0;}
            CHECK(!bad && lit>100 && a->game->frame_game.counter==counter && !IsWindowVisible(a->window));
            if(!mode){CHECK(swprintf(path,128,L"build/audio-settings/aspect-%u-%u.bmp",i,aspect)>0);
                CHECK(snapshot_surface(path,&a->back_info,a->back_pixels));}
        }
    }
    CHECK(SetWindowPos(a->window,NULL,saved.left,saved.top,saved.right-saved.left,saved.bottom-saved.top,SWP_NOZORDER|SWP_NOACTIVATE));
    a->render_width=rw;a->render_height=rh;a->menu_active=1;a->original_renderer=0;a->game->gba_view=0;
    a->settings.aspect_ratio=SSA_ASPECT_NATIVE;
}
static void scores_checks(App *a){
    SsaMenu *saved=malloc(sizeof(*saved));unsigned language,mode,page_index;
    unsigned original=a->original_renderer,counter=a->game->frame_game.counter;
    CHECK(saved);*saved=*a->menu;
    a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a->bitmap.bmiHeader.biPlanes=1;
    a->bitmap.bmiHeader.biBitCount=32;a->bitmap.bmiHeader.biCompression=BI_RGB;
    for(mode=0;mode<2;++mode)for(language=0;language<3;++language){
        RECT client;int left,top,width,height;LPARAM point;
        a->original_renderer=mode;memset(a->menu,0,sizeof(*a->menu));
        CHECK(ssa_menu_init(a->menu,a->rom,a->rom_size,1)==1);
        a->menu->language=language;a->menu->page=a->menu->draw_page=SSA_MENU_MAIN;
        a->menu->selection=3;a->menu->offset=0;a->menu->marker=0;
        a->menu->fading=a->menu->fade_volume=0;a->menu->logo_y=8;
        key(a,VK_RETURN);CHECK(a->menu->page==SSA_MENU_SCORES && !a->menu->unsupported);
        for(page_index=0;page_index<3;++page_index){wchar_t path[128];
            while(a->menu->score_speed)CHECK(ssa_menu_step(a->menu,0)==1);
            CHECK(a->menu->score_y==(16-(int)page_index*176)*65536);
            CHECK(render(a) && !IsWindowVisible(a->window));
            CHECK(swprintf(path,128,L"build/audio-settings/scores-%u-%u-%u.bmp",mode,language,page_index)>0);
            CHECK(snapshot(a,path));
            if(page_index<2)key(a,VK_DOWN);
        }
        /* Actual letterboxed mouse input on the original upper-page arrow. */
        CHECK(GetClientRect(a->window,&client));
        CHECK(ssa_ui_viewport(client.right,client.bottom,240,160,&left,&top,&width,&height));
        point=MAKELPARAM(left+232*width/240,top+8*height/160);
        SendMessageW(a->window,WM_MOUSEMOVE,0,point);
        SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(a->window,WM_LBUTTONUP,0,point);
        CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;
        CHECK(a->menu->score_speed==-2*65536);
        while(a->menu->score_speed)CHECK(ssa_menu_step(a->menu,0)==1);
        CHECK(a->menu->score_y==-160*65536);
        key(a,VK_ESCAPE);CHECK(a->menu->page==SSA_MENU_MAIN && !a->menu->done);
        CHECK(a->game->frame_game.counter==counter && !a->game->error_line);
    }
    *a->menu=*saved;free(saved);a->original_renderer=original;
}
static void midi_checks(App *a,const wchar_t *font_path){
    SsaWin32Midi *m=a->audio.midi;SsaAudio *mix=&a->audio.mixer;
    SsaAudioSynth synth=ssa_win32_midi_synth(m);SsaAudioTrack track={0};SsaAudioVoice voice={0};
    SsaSettings settings={SSA_MUSIC_MIDI,100,100,SSA_RENDER_SOFTWARE,1,1,1,SSA_DISPLAY_FULLSCREEN,0,1,SSA_HUD_ORIGINAL,SSA_ASPECT_NATIVE,0,0,0};int16_t pcm[1600];unsigned song,block,i;uint64_t notes;
    /* Remove only a test-created copy, proving an already loaded bank is no
     * longer selectable once its file is absent. The user's file is untouched. */
    CHECK(CopyFileW(font_path,L"build/audio-settings/availability-test.sf2",FALSE));
    CHECK(ssa_win32_audio_load_soundfont(&a->audio,L"build/audio-settings/availability-test.sf2"));
    CHECK(ssa_win32_audio_soundfont_available(&a->audio));
    CHECK(DeleteFileW(L"build/audio-settings/availability-test.sf2"));
    CHECK(!ssa_win32_audio_soundfont_available(&a->audio));
    settings.music_style=SSA_MUSIC_SOUNDFONT;CHECK(!ssa_win32_audio_settings(&a->audio,&settings));
    settings.music_style=SSA_MUSIC_MIDI;CHECK(ssa_win32_audio_settings(&a->audio,&settings));
    a->menu->settings=settings;a->settings=settings;settings_change(a);
    CHECK(!a->menu->soundfont_available && a->menu->midi_available);
    a->menu->page=a->menu->draw_page=SSA_MENU_SETTINGS;a->menu->selection=4;
    key(a,VK_RIGHT);CHECK(a->settings.music_style==SSA_MUSIC_ORIGINAL);
    key(a,VK_RIGHT);CHECK(a->settings.music_style==SSA_MUSIC_MIDI);
    /* Finish real worker playback before faster-than-realtime muted device
     * checks. No nonzero expression is sent while any note is active. */
    ssa_win32_audio_quiesce(&a->audio);EnterCriticalSection(&a->audio.mutex);
    CHECK(m->delay_frames==8880 && synth.mix_music);
    {double frame[2];unsigned n;
        synth.reset(m);
        for(n=0;n<=m->delay_frames;++n){frame[0]=n?0:1234;frame[1]=n?0:-617;
            synth.mix_music(m,frame);
            CHECK(frame[0]==(n==m->delay_frames?617:0) && frame[1]==(n==m->delay_frames?-308.5:0));}
        frame[0]=99;frame[1]=-99;synth.mix_music(m,frame);synth.reset(m);
        for(n=0;n<=m->delay_frames;++n){frame[0]=frame[1]=0;synth.mix_music(m,frame);CHECK(!frame[0] && !frame[1]);}
        CHECK(!ssa_win32_midi_set_delay(m,501) && m->delay_frames==8880);
        CHECK(ssa_win32_midi_set_delay(m,0));frame[0]=123;frame[1]=-123;synth.mix_music(m,frame);
        CHECK(frame[0]==61.5 && frame[1]==-61.5 && ssa_win32_midi_set_delay(m,SSA_MIDI_DELAY_MS));
    }
    /* Music compensation must not delay even one sound-effect sample. */
    {SsaAudio *original=calloc(1,sizeof(*original));int16_t reference[1600];unsigned heard=0;
        CHECK(original && ssa_audio_init(original,a->rom,a->rom_size,48000)==1);
        ssa_audio_reset(mix);mix->muted=0;
        CHECK(ssa_audio_play(mix,13)==1 && ssa_audio_play(original,13)==1);
        CHECK(ssa_audio_render(mix,pcm,800)==1 && ssa_audio_render(original,reference,800)==1);
        CHECK(!memcmp(pcm,reference,sizeof pcm));for(i=0;i<1600;++i)heard|=pcm[i]!=0;
        CHECK(heard);mix->muted=1;ssa_win32_midi_volume(m);free(original);
    }
    ssa_audio_reset(mix);mix->muted=0;ssa_win32_midi_volume(m);CHECK(m->expression==127);
    CHECK(ssa_audio_set_volume(mix,50,100)==1);ssa_win32_midi_volume(m);CHECK(m->expression==64);
    mix->master_gain=0.5;ssa_win32_midi_volume(m);CHECK(m->expression==32);
    ssa_win32_audio_toggle_mute(&a->audio);CHECK(m->expression==0);
    ssa_win32_audio_toggle_mute(&a->audio);CHECK(m->expression==32);
    mix->music_enabled=0;ssa_win32_midi_volume(m);CHECK(m->expression==0);
    mix->music_enabled=1;mix->muted=1;mix->master_gain=1;
    CHECK(ssa_audio_set_volume(mix,100,100)==1);ssa_win32_midi_volume(m);
    track.voice=5;track.volume=127;track.pan=64;voice.type=3;voice.note=60;voice.velocity=100;
    CHECK(synth.note(m,0,&track,&voice)==1 && m->keys[0]==48 && m->channels[0]!=9);
    track.bend=32;track.bend_range=12;track.pan=25;track.volume=70;synth.control(m,0,&track);
    CHECK(m->pitches[m->channels[0]]==10240 && m->pans[m->channels[0]]==25 && m->volumes[m->channels[0]]==126);
    track.volume=100;synth.control(m,0,&track);CHECK(m->volumes[m->channels[0]]==127);
    track.volume=127;synth.control(m,0,&track);CHECK(m->volumes[m->channels[0]]==127);
    track.volume=0;synth.control(m,0,&track);CHECK(m->volumes[m->channels[0]]==0);
    track.volume=70;
    synth.release(m,0);CHECK(m->keys[0]<0);
    track.voice=127;voice.type=4;voice.note=24;
    CHECK(synth.note(m,1,&track,&voice)==1 && m->keys[1]==42 && m->channels[1]==9);
    notes=m->notes;voice.type=0;CHECK(synth.note(m,1,&track,&voice)==1 && m->notes==notes && m->keys[1]<0);
    synth.reset(m);for(i=0;i<SSA_AUDIO_TRACKS;++i)CHECK(m->keys[i]<0);
    for(song=0x6e;song<SSA_AUDIO_SONGS;++song){
        ssa_audio_reset(mix);CHECK(ssa_audio_play(mix,song)==1);
        for(block=0;block<120;++block)CHECK(ssa_audio_render(mix,pcm,800)==1 && !mix->errors && !m->error && m->expression==0);
    }
    CHECK(m->notes>notes && !m->error);ssa_audio_reset(mix);
    for(i=0;i<SSA_AUDIO_TRACKS;++i)CHECK(m->keys[i]<0);
    LeaveCriticalSection(&a->audio.mutex);
}
int wmain(int argc,wchar_t **argv){
    App *a=calloc(1,sizeof(*a));WNDCLASSW wc={0};SsaSettings loaded=SSA_SETTINGS_DEFAULT;
    SsaAudioPlayer before;unsigned starts;uint64_t frames;HANDLE file;DWORD written;uint8_t bad[3]={0};
    int midi_only=argc==5 && !wcscmp(argv[4],L"--midi-only");
    CHECK((argc==4 || midi_only) && a);CHECK(read_rom(a,argv[1]));
    a->game=calloc(1,sizeof(*a->game));a->menu=calloc(1,sizeof(*a->menu));CHECK(a->game && a->menu);
    CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
    a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->profile.enabled=1;
    CHECK(wcscpy_s(a->settings_path,32768,argv[3])==0);
    CHECK(ssa_win32_settings_write(&a->settings,a->settings_path));
    CHECK(ssa_menu_init(a->menu,a->rom,a->rom_size,1)==1);a->menu_active=1;
    a->menu->page=a->menu->draw_page=SSA_MENU_OPTIONS;a->menu->selection=4;
    a->menu->settings=a->settings;
    CHECK(ssa_win32_audio_open(&a->audio,a->rom,a->rom_size,0,1,0));
    CHECK(a->audio.midi && !wcscmp(a->audio.midi->name,L"Microsoft GS Wavetable Synth"));
    a->menu->midi_available=1;
    CHECK(!ssa_win32_audio_load_soundfont(&a->audio,L"build/audio-settings/missing.sf2"));
    CHECK(!ssa_win32_audio_soundfont_available(&a->audio));
    loaded.music_style=SSA_MUSIC_SOUNDFONT;CHECK(!ssa_win32_audio_settings(&a->audio,&loaded));
    loaded.music_style=SSA_MUSIC_MIDI;CHECK(ssa_win32_audio_settings(&a->audio,&loaded));
    CHECK(a->audio.mixer.synth.context==a->audio.midi && a->audio.midi->expression==0);
    loaded.music_style=SSA_MUSIC_ORIGINAL;CHECK(ssa_win32_audio_settings(&a->audio,&loaded));
    CHECK(ssa_win32_audio_load_soundfont(&a->audio,argv[2]));a->menu->soundfont_available=1;
    ssa_win32_audio_menu_command(&a->audio,8,0,1);
    wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA settings test";
    CHECK(RegisterClassW(&wc));
    a->window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,80,80,720,480,NULL,NULL,wc.hInstance,a);CHECK(a->window);
    if(midi_only)goto midi_validation;
    /* Saved/default Vulkan must fall back and disappear when no device opened. */
    CHECK(!a->vulkan && a->settings.renderer==SSA_RENDER_VULKAN);
    a->menu->vulkan_available=1;settings_change(a);
    CHECK(a->settings.renderer==SSA_RENDER_SOFTWARE && a->menu->settings.renderer==SSA_RENDER_SOFTWARE && !a->menu->vulkan_available);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.renderer==SSA_RENDER_SOFTWARE);
    scores_checks(a);
    CHECK(game_tick(a)==1); /* Populate sprite cache/OAM before a world render. */
    key(a,VK_DOWN);key(a,VK_RETURN);CHECK(a->menu->page==SSA_MENU_GRAPHICS);
    {RECT windowed,full,restored,client;MONITORINFO info={0};LONG_PTR style;int left,top,width,height;LPARAM point;
        CHECK(GetWindowRect(a->window,&windowed));
        CHECK(display_mode_apply(a,SSA_DISPLAY_WINDOWED) && !IsWindowVisible(a->window));
        CHECK(GetWindowRect(a->window,&restored) && EqualRect(&windowed,&restored));
        CHECK(display_mode_apply(a,a->settings.display_mode) && !IsWindowVisible(a->window));
        info.cbSize=sizeof(info);CHECK(GetMonitorInfoW(MonitorFromWindow(a->window,MONITOR_DEFAULTTONEAREST),&info));
        CHECK(GetWindowRect(a->window,&full) && EqualRect(&full,&info.rcMonitor));
        style=GetWindowLongPtrW(a->window,GWL_STYLE);CHECK((style&WS_POPUP) && !(style&WS_OVERLAPPEDWINDOW));
        key(a,VK_RIGHT);CHECK(a->settings.display_mode==SSA_DISPLAY_WINDOWED && !IsWindowVisible(a->window));
        CHECK(GetWindowRect(a->window,&restored) && EqualRect(&windowed,&restored));
        CHECK((GetWindowLongPtrW(a->window,GWL_STYLE)&WS_OVERLAPPEDWINDOW)==WS_OVERLAPPEDWINDOW);
        CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.display_mode==SSA_DISPLAY_WINDOWED);
        key(a,VK_LEFT);CHECK(a->settings.display_mode==SSA_DISPLAY_FULLSCREEN);
        CHECK(GetWindowRect(a->window,&restored) && EqualRect(&restored,&full));
        CHECK(GetClientRect(a->window,&client) && client.right==full.right-full.left && client.bottom==full.bottom-full.top);
        /* Real mouse routing on the top Settings item. */
        CHECK(ssa_ui_viewport(client.right,client.bottom,240,160,&left,&top,&width,&height));
        point=MAKELPARAM(left+120*width/240,top+22*height/160);
        SendMessageW(a->window,WM_MOUSEMOVE,0,point);SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);
        SendMessageW(a->window,WM_LBUTTONUP,0,point);
        CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;settings_change(a);
        CHECK(a->settings.display_mode==SSA_DISPLAY_WINDOWED && !IsWindowVisible(a->window));
        CHECK(GetWindowRect(a->window,&restored) && EqualRect(&windowed,&restored));
        key(a,VK_LEFT);CHECK(a->settings.display_mode==SSA_DISPLAY_FULLSCREEN);
        CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.display_mode==SSA_DISPLAY_FULLSCREEN);
        /* F11 is global, before the gameplay/menu/overlay key routers. */
        {unsigned context,renderer,counter=a->game->frame_game.counter;
            for(renderer=0;renderer<2;++renderer)for(context=0;context<5;++context){
                a->original_renderer=renderer;a->menu_active=context==0;a->net_active=context==2;
                a->screen_active=context==3;a->story_active=context==4;
                SendMessageW(a->window,WM_KEYDOWN,VK_F11,0);
                CHECK(a->settings.display_mode==SSA_DISPLAY_WINDOWED && a->window_mode==SSA_DISPLAY_WINDOWED);
                CHECK(a->menu->settings.display_mode==SSA_DISPLAY_WINDOWED && !IsWindowVisible(a->window));
                CHECK(GetWindowRect(a->window,&restored) && EqualRect(&restored,&windowed));
                CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.display_mode==SSA_DISPLAY_WINDOWED);
                SendMessageW(a->window,WM_KEYDOWN,VK_F11,(LPARAM)1<<30);
                SendMessageW(a->window,WM_KEYUP,VK_F11,0);
                CHECK(a->settings.display_mode==SSA_DISPLAY_WINDOWED); /* No auto-repeat/key-up toggle. */
                SendMessageW(a->window,WM_KEYDOWN,VK_F11,0);SendMessageW(a->window,WM_KEYUP,VK_F11,0);
                CHECK(a->settings.display_mode==SSA_DISPLAY_FULLSCREEN && a->window_mode==SSA_DISPLAY_FULLSCREEN);
                CHECK(a->menu->settings.display_mode==SSA_DISPLAY_FULLSCREEN && !IsWindowVisible(a->window));
                CHECK(GetWindowRect(a->window,&restored) && EqualRect(&restored,&full));
                CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.display_mode==SSA_DISPLAY_FULLSCREEN);
                CHECK(a->game->frame_game.counter==counter && !a->failed && !a->menu_pressed && !a->net_pressed && !a->screen_pressed && !a->story_pressed);
            }
            a->original_renderer=0;a->menu_active=1;a->net_active=a->screen_active=a->story_active=0;
        }
    }
    CHECK(!frontend_audio_applies); /* Display changes do not reapply audio. */
    key(a,VK_DOWN);CHECK(a->menu->selection==1);
    key(a,VK_RIGHT);CHECK(a->original_renderer && a->settings.renderer==SSA_RENDER_GBA);
    CHECK(a->game->gba_view);
    a->game->player.pitch=5440;a->game->player.free_aim=1;
    CHECK(game_tick(a)==1 && !a->game->player.pitch && !a->game->player.free_aim);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.renderer==SSA_RENDER_GBA);
    a->menu_active=0;CHECK(render(a) && a->bitmap.bmiHeader.biWidth==240 && a->bitmap.bmiHeader.biHeight==-160);a->menu_active=1;
    key(a,VK_LEFT);CHECK(!a->original_renderer);
    CHECK(!a->game->gba_view);
    a->render_width=854;a->render_height=480;a->menu_active=0;
    CHECK(render(a) && a->bitmap.bmiHeader.biWidth==854 && a->bitmap.bmiHeader.biHeight==-480);a->menu_active=1;
    key(a,VK_DOWN);CHECK(a->menu->selection==2);
    key(a,VK_RIGHT);CHECK(a->settings.aspect_ratio==SSA_ASPECT_GBA);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.aspect_ratio==SSA_ASPECT_GBA);
    a->menu_active=0;CHECK(render(a) && a->renderer.width==720 && a->renderer.height==480);a->menu_active=1;
    key(a,VK_UP);key(a,VK_RIGHT);CHECK(a->original_renderer);
    key(a,VK_DOWN);CHECK(a->menu->selection==4 && a->settings.aspect_ratio==SSA_ASPECT_GBA);
    key(a,VK_UP);key(a,VK_LEFT);key(a,VK_DOWN);CHECK(a->menu->selection==2);
    key(a,VK_LEFT);CHECK(a->settings.aspect_ratio==SSA_ASPECT_NATIVE);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.aspect_ratio==SSA_ASPECT_NATIVE);
    key(a,VK_DOWN);CHECK(a->menu->selection==3);
    key(a,VK_LEFT);CHECK(!a->settings.mipmaps && !a->game->mipmaps);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.mipmaps);
    key(a,VK_UP);key(a,VK_UP);key(a,VK_RIGHT);
    CHECK(a->original_renderer && a->game->mipmaps && !a->settings.mipmaps);
    key(a,VK_DOWN);CHECK(a->menu->selection==4);
    key(a,VK_UP);key(a,VK_LEFT);
    CHECK(!a->original_renderer && !a->game->mipmaps);
    key(a,VK_DOWN);key(a,VK_DOWN);key(a,VK_RIGHT);
    CHECK(a->settings.mipmaps && a->game->mipmaps);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.mipmaps);
    aspect_checks(a);
    menu_select(a,4);CHECK(a->menu->selection==4 && !a->settings.surface_decals && !a->game->surface_decals_enabled);
    key(a,VK_RIGHT);CHECK(a->settings.surface_decals && a->game->surface_decals_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.surface_decals);
    key(a,VK_LEFT);CHECK(!a->settings.surface_decals && !a->game->surface_decals_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.surface_decals);
    menu_select(a,5);CHECK(a->menu->selection==5 && !a->settings.water_reflections && !a->game->water_reflections);
    key(a,VK_RIGHT);CHECK(a->settings.water_reflections && a->game->water_reflections);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.water_reflections);
    key(a,VK_LEFT);CHECK(!a->settings.water_reflections && !a->game->water_reflections);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.water_reflections);
    menu_select(a,6);CHECK(a->menu->selection==6 && !a->settings.godrays && !a->game->godrays_enabled);
    key(a,VK_RIGHT);CHECK(a->settings.godrays && a->game->godrays_enabled && !a->game->surface_decals_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.godrays && !loaded.surface_decals);
    menu_select(a,4);key(a,VK_RIGHT);CHECK(a->game->surface_decals_enabled && a->game->godrays_enabled);
    menu_select(a,6);key(a,VK_LEFT);CHECK(!a->game->godrays_enabled && a->game->surface_decals_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.godrays && loaded.surface_decals);
    menu_select(a,4);key(a,VK_LEFT);
    menu_select(a,7);CHECK(!a->settings.sprite_shadows && !a->game->sprite_shadows_enabled);
    key(a,VK_RIGHT);CHECK(a->settings.sprite_shadows && a->game->sprite_shadows_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.sprite_shadows);
    key(a,VK_LEFT);CHECK(!a->settings.sprite_shadows && !a->game->sprite_shadows_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.sprite_shadows);
    key(a,VK_ESCAPE);key(a,VK_DOWN);key(a,VK_RETURN);CHECK(a->menu->page==SSA_MENU_EXTRA);
    menu_select(a,3);
    CHECK(!a->settings.jump_enabled);
    key(a,VK_RIGHT);CHECK(a->settings.jump_enabled);
    key(a,VK_LEFT);CHECK(!a->settings.jump_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.jump_enabled && !loaded.renderer);
    ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
    CHECK(ssa_pc_input_runtime_key(&a->game->input_runtime,' ',1,0)==1 && game_tick(a)==1);
    CHECK(!a->game->input_runtime.sample.jump_down && !a->game->player.jump.was_down);
    CHECK(ssa_pc_input_runtime_key(&a->game->input_runtime,' ',0,0)==1);
    key(a,VK_RIGHT);CHECK(a->settings.jump_enabled);
    CHECK(ssa_pc_input_runtime_key(&a->game->input_runtime,' ',1,0)==1 && game_tick(a)==1);
    CHECK(a->game->input_runtime.sample.jump_down && a->game->player.jump.was_down && a->game->players[0].motion.velocity_z>0);
    CHECK(ssa_pc_input_runtime_key(&a->game->input_runtime,' ',0,0)==1);
    menu_select(a,2);CHECK(a->menu->selection==2);
    game_h(a);CHECK(a->game->player.view.third_person);
    key(a,VK_LEFT);CHECK(!a->settings.thirdperson_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.thirdperson_enabled);
    game_h(a);CHECK(!a->game->player.view.third_person && !a->game->player.pose.third_person && !a->game->input_runtime.sample.view_toggle);
    CHECK(game_tick(a)==1 && !a->game->player.view.third_person);
    key(a,VK_RIGHT);CHECK(a->settings.thirdperson_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.thirdperson_enabled);
    CHECK(game_tick(a)==1 && !a->game->player.view.third_person);
    game_h(a);CHECK(a->game->player.view.third_person);
    game_h(a);CHECK(!a->game->player.view.third_person);
    menu_select(a,5);key(a,VK_LEFT);CHECK(!a->settings.ingame_netricsa);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.ingame_netricsa);
    a->menu_active=0;
    SendMessageW(a->window,WM_KEYDOWN,VK_TAB,0);SendMessageW(a->window,WM_KEYUP,VK_TAB,0);
    SendMessageW(a->window,WM_KEYDOWN,'N',0);SendMessageW(a->window,WM_KEYUP,'N',0);
    CHECK(!a->net_active && !a->failed);
    CHECK(begin_netricsa(a,0)==0 && !a->net_active);
    CHECK(begin_netricsa(a,1) && a->net_active && a->net_prelevel); /* Briefings are unaffected. */
    a->net_prelevel=0;CHECK(finish_netricsa(a) && !a->net_active);a->menu_active=1;
    key(a,VK_RIGHT);CHECK(a->settings.ingame_netricsa);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.ingame_netricsa);
    a->menu_active=0;
    SendMessageW(a->window,WM_KEYDOWN,VK_TAB,0);SendMessageW(a->window,WM_KEYUP,VK_TAB,0);
    CHECK(a->net_active && !a->net_prelevel && !a->failed);
    CHECK(finish_netricsa(a) && !a->net_active);a->menu_active=1;
    menu_select(a,6);CHECK(a->menu->selection==6 && !a->settings.alternate_weapons);
    key(a,VK_RIGHT);CHECK(a->settings.alternate_weapons && a->game->alternates.enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.alternate_weapons);
    key(a,VK_LEFT);CHECK(!a->settings.alternate_weapons && !a->game->alternates.enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.alternate_weapons);
    menu_select(a,4);CHECK(a->menu->selection==4 && !a->settings.automap_enabled);
    key(a,VK_RIGHT);CHECK(a->settings.automap_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.automap_enabled);
    menu_select(a,0);CHECK(a->menu->selection==0 && a->settings.hud_style==SSA_HUD_ORIGINAL);
    key(a,VK_RIGHT);CHECK(a->settings.hud_style==SSA_HUD_CLASSIC && a->game->hud_style==SSA_HUD_CLASSIC);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.hud_style==SSA_HUD_CLASSIC);
    key(a,VK_LEFT);CHECK(a->settings.hud_style==SSA_HUD_ORIGINAL && a->game->hud_style==SSA_HUD_ORIGINAL);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.hud_style==SSA_HUD_ORIGINAL);
    menu_select(a,7);CHECK(a->menu->selection==7 && !a->settings.weapon_bobbing && !a->game->weapon_bobbing);
    key(a,VK_RIGHT);CHECK(a->settings.weapon_bobbing && a->game->weapon_bobbing);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.weapon_bobbing);
    key(a,VK_LEFT);CHECK(!a->settings.weapon_bobbing && !a->game->weapon_bobbing);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.weapon_bobbing);
    menu_select(a,1);CHECK(a->menu->selection==1 && a->settings.vertical_aim && a->game->vertical_aim);
    key(a,VK_RIGHT);CHECK(!a->settings.vertical_aim && !a->game->vertical_aim);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.vertical_aim);
    key(a,VK_LEFT);CHECK(a->settings.vertical_aim && a->game->vertical_aim);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.vertical_aim);
    menu_select(a,8);CHECK(a->menu->selection==8);
    menu_select(a,7);CHECK(a->menu->selection==7);
    menu_select(a,0);CHECK(a->menu->selection==0);
    menu_select(a,4);CHECK(a->menu->selection==4);
    CHECK(ssa_native_game_automap(a->game,1)==1);a->automap_keys=1;a->wheel_remainder=30;
    key(a,VK_RIGHT);CHECK(!a->settings.automap_enabled && !(a->game->players[0].motion.movement.flags&8));
    CHECK(!a->automap_keys && !a->wheel_remainder);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && !loaded.automap_enabled);
    a->menu_active=0;
    SendMessageW(a->window,WM_KEYDOWN,'M',0);SendMessageW(a->window,WM_KEYUP,'M',0);
    CHECK(!(a->game->players[0].motion.movement.flags&8) && !a->failed);
    a->menu_active=1;key(a,VK_LEFT);CHECK(a->settings.automap_enabled);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.automap_enabled);
    CHECK(!frontend_audio_applies); /* Graphics/Extras never touch the music bus. */
    key(a,VK_ESCAPE);key(a,VK_UP);key(a,VK_UP);key(a,VK_RETURN);CHECK(a->menu->page==SSA_MENU_SETTINGS);
    key(a,VK_RIGHT);CHECK(a->menu->language==1);
    key(a,VK_LEFT);CHECK(a->menu->language==0);
    key(a,VK_DOWN);CHECK(a->menu->selection==8);
    CHECK(a->settings.mouse_sensitivity==SSA_MOUSE_DEFAULT && a->game->input.yaw_per_count==64);
    key(a,VK_RIGHT);CHECK(a->settings.mouse_sensitivity==10 && a->game->input.yaw_per_count==70);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.mouse_sensitivity==10);
    {SsaPcInputSample sample;SsaPcInput *input=&a->game->input;
        ssa_pc_input_focus(input,1);ssa_pc_input_enable(input,1);
        CHECK(ssa_pc_input_sample(input,&sample)==1);
        CHECK(ssa_pc_input_relative(input,10,10)==1);
        CHECK(ssa_pc_input_sample(input,&sample)==1 && sample.yaw_delta==700 && sample.pitch_delta==-700);
        ssa_pc_input_enable(input,0);
    }
    key(a,VK_DOWN);CHECK(a->menu->selection==3);
    CHECK(a->settings.fov_offset==10 && a->game->fov_offset==10);
    key(a,VK_RIGHT);CHECK(a->settings.fov_offset==11 && a->game->fov_offset==11);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.fov_offset==11);
    key(a,VK_LEFT);CHECK(a->settings.fov_offset==10 && a->game->fov_offset==10);
    key(a,VK_DOWN);CHECK(a->menu->selection==4);
    EnterCriticalSection(&a->audio.mutex);before=a->audio.mixer.players[0];starts=(unsigned)a->audio.mixer.starts;
    key(a,VK_RIGHT);CHECK(a->settings.music_style==1 && a->audio.mixer.synth.context);
    CHECK(frontend_audio_applies==1);
    CHECK(a->audio.mixer.starts==starts && !memcmp(&before,&a->audio.mixer.players[0],sizeof before));
    LeaveCriticalSection(&a->audio.mutex);
    key(a,VK_DOWN);key(a,VK_LEFT);CHECK(a->settings.music_volume==75);
    key(a,VK_DOWN);key(a,VK_LEFT);key(a,VK_LEFT);CHECK(a->settings.sound_volume==90);
    CHECK(frontend_audio_applies==4);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.music_style==1 && loaded.music_volume==75 && loaded.sound_volume==90);
    EnterCriticalSection(&a->audio.mutex);frames=a->audio.mixer.frames;
    CHECK(a->audio.mixer.music_volume==75 && a->audio.mixer.sound_volume==90);LeaveCriticalSection(&a->audio.mutex);
    Sleep(200);
    EnterCriticalSection(&a->audio.mutex);CHECK(a->audio.mixer.frames>frames && !a->audio.mixer.errors);LeaveCriticalSection(&a->audio.mutex);
    CHECK(ssa_win32_audio_restart(&a->audio,0));
    CHECK(ssa_win32_audio_begin_level(&a->audio,0));
    EnterCriticalSection(&a->audio.mutex);CHECK(a->audio.mixer.synth.context && a->audio.mixer.music_volume==75 && a->audio.mixer.sound_volume==90);LeaveCriticalSection(&a->audio.mutex);
    key(a,VK_UP);key(a,VK_UP);key(a,VK_LEFT);CHECK(!a->settings.music_style);
    /* Reverse cycling reaches MIDI. Switching must retain the sequencer and
     * settings must persist MIDI without depending on the SF2 loader. */
    EnterCriticalSection(&a->audio.mutex);before=a->audio.mixer.players[0];
    key(a,VK_LEFT);CHECK(a->settings.music_style==SSA_MUSIC_MIDI && a->audio.mixer.synth.context==a->audio.midi);
    CHECK(!memcmp(&before,&a->audio.mixer.players[0],sizeof before));
    CHECK(a->audio.midi->expression==0 && !a->audio.midi->error);
    LeaveCriticalSection(&a->audio.mutex);
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.music_style==SSA_MUSIC_MIDI);
    CHECK(ssa_win32_audio_begin_level(&a->audio,0));Sleep(200);
    EnterCriticalSection(&a->audio.mutex);
    CHECK(a->audio.midi->notes>0 && !a->audio.midi->error && !a->audio.mixer.errors);
    key(a,VK_RIGHT);CHECK(!a->settings.music_style);
    {unsigned track;for(track=0;track<SSA_AUDIO_TRACKS;++track)CHECK(a->audio.midi->keys[track]<0);}
    LeaveCriticalSection(&a->audio.mutex);
    key(a,VK_ESCAPE);CHECK(a->menu->page==SSA_MENU_OPTIONS && a->menu->selection==4);
    /* Corrupt preferences cannot overwrite caller defaults; failed atomic
     * replacement leaves the last good file intact. */
    CHECK(SetFileAttributesW(a->settings_path,FILE_ATTRIBUTE_READONLY));
    loaded.music_volume=10;CHECK(!ssa_win32_settings_write(&loaded,a->settings_path));
    CHECK(SetFileAttributesW(a->settings_path,FILE_ATTRIBUTE_NORMAL));
    CHECK(ssa_win32_settings_read(&loaded,a->settings_path) && loaded.music_volume==75);
    file=CreateFileW(a->settings_path,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(file!=INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file,bad,sizeof bad,&written,NULL));CloseHandle(file);
    loaded=(SsaSettings)SSA_SETTINGS_DEFAULT;
    CHECK(!ssa_win32_settings_read(&loaded,a->settings_path) && loaded.music_volume==80 && loaded.sound_volume==100 && !loaded.music_style);
    CHECK(ssa_win32_settings_write(&a->settings,a->settings_path));
midi_validation:
    midi_checks(a,argv[2]);
    ssa_win32_audio_close(&a->audio);DestroyWindow(a->window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    if(a->back_dc){if(a->back_original)SelectObject(a->back_dc,a->back_original);
        if(a->back_bitmap)DeleteObject(a->back_bitmap);DeleteDC(a->back_dc);}
    ssa_pc_renderer_free(&a->renderer);ssa_native_game_free(a->game);free(a->game);free(a->menu);free(a->rom);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"score_paths\":%u,\"midi_only\":%s,\"hidden_window\":true,\"output_muted\":true,\"midi_device\":\"Microsoft GS Wavetable Synth\",\"midi_sequences\":26}\n",checks,midi_only?0:6,midi_only?"true":"false");return 0;
}
