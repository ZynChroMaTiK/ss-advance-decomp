/* Real lethal damage and wall-clock death/menu routes; muted device output.
 * No physical input, campaign replay or user save modifications. */
#define wmain embedded_run_exit_game_entry
#include "../platform/win32/game.c"
#undef wmain
static unsigned checks,frames,deaths,retries,menus,black_frames,clock_paths;
static unsigned clock_killed,clock_returned,clock_timeout;
static unsigned clock_retry_reveals;
static ULONGLONG clock_start;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows death menu line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void lethal(App *a)
{
    SsaEntity source={0};source.type=0xcd;
    a->game->players[0].entity.body.flags&=(uint16_t)~1u;
    CHECK(ssa_combat_runtime_damage(&a->game->combat,&a->game->players[0].entity,
        &source,NULL,10000,&a->game->combat_ops)==0);
    CHECK(a->game->players[0].inventory.health==0);
}
static LRESULT CALLBACK clock_proc(HWND w,UINT message,WPARAM wp,LPARAM lp)
{
    App *a=(App *)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(message==WM_TIMER && a){
        if(GetTickCount64()-clock_start>35000){clock_timeout=1;a->running=0;return 0;}
        if(clock_killed==1){
            CHECK(!a->net_active && !a->map_active && !a->story_active);
            if(a->level_revealing && !clock_retry_reveals){
                CHECK(a->level_fade && a->level_fade<=LEVEL_FADE_TICKS);
                ++clock_retry_reveals;
            }
        }
        if(clock_killed<2 && !a->menu_active && !a->map_active && !a->net_active && !a->death_active &&
           !a->story_active && !a->level_revealing && a->game->frame_game.counter>5 && a->game->players[0].inventory.health>0){
            CHECK(a->game->game.life_game.continue_flag==2-clock_killed);
            a->startup_test=0;lethal(a);a->game->frame_game.flags|=1;++clock_killed;
        }else if(clock_killed && a->menu_active && a->menu->counter>=30){
            CHECK(a->menu->page==SSA_MENU_MAIN && !a->reload && !a->advance && !a->death_active);
            clock_returned=1;a->running=0;
        }
        return 0;
    }
    return proc(w,message,wp,lp);
}
static void bind(App *a)
{
    SsaNativeAudioOps ops={ssa_win32_audio_play,ssa_win32_audio_stop,&a->audio,
        ssa_win32_audio_play_at,ssa_win32_audio_listen,ssa_win32_audio_move,ssa_win32_audio_music,ssa_win32_audio_source};
    CHECK(ssa_native_game_bind_audio(a->game,&ops)==1);
}
int wmain(int argc,wchar_t **argv)
{
    App *a=calloc(1,sizeof(*a));WNDCLASSW wc={0};unsigned mode,lang,hud,last,t,i;
    CHECK(argc==3 && a && read_rom(a,argv[1]));a->game=calloc(1,sizeof(*a->game));CHECK(a->game);
    a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->render_width=320;a->render_height=240;
    wc.lpfnWndProc=clock_proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA death menu test";
    CHECK(RegisterClassW(&wc));
    a->window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,wc.hInstance,a);
    CHECK(a->window);a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    a->bitmap.bmiHeader.biPlanes=1;a->bitmap.bmiHeader.biBitCount=32;
    CHECK(ssa_win32_audio_open(&a->audio,a->rom,a->rom_size,0,1,0));
    for(hud=0;hud<2;++hud)for(mode=0;mode<2;++mode)for(lang=0;lang<3;++lang)for(last=0;last<2;++last){
        unsigned counter,lives;wchar_t path[32768];
        CHECK(ssa_native_game_init_campaign(a->game,a->rom,a->rom_size,0,lang,NULL)==1);
        a->original_renderer=mode;a->language=lang;a->difficulty=lang;a->level=0;
        a->settings.hud_style=hud;a->settings.renderer=mode?SSA_RENDER_GBA:SSA_RENDER_SOFTWARE;
        a->death_active=a->menu_active=a->reload=a->advance=a->failed=0;
        a->level_revealing=a->level_fade=0;
        a->net_active=a->net_prelevel=a->map_active=a->story_active=0;
        level_options(a,a->game);bind(a);
        a->game->score=123456;CHECK(ssa_native_game_carry_capture(&a->entry,a->game)==1);
        for(t=0;t<24;++t)CHECK(game_tick(a)==1);
        if(last)a->game->game.life_game.continue_flag=a->game->gameplay.life_game.continue_flag=1;
        lives=a->game->game.life_game.continue_flag;
        a->game->score=3500000;lethal(a);
        if(last)a->game->frame_game.flags|=1; /* A stale pause must not hold death forever. */
        for(t=1;t<=400;++t){
            CHECK(game_tick(a)==1 && !a->reload && !a->menu_active);++frames;
            if(last && t==120){CHECK(a->game->hud_notice.id==0xb4 && a->game->music_mode==10);
                CHECK(swprintf(path,32768,L"%ls/gameover-%u-%u-%u.bmp",argv[2],hud,mode,lang)>0 && render(a) && snapshot(a,path));}
            if(t<=20 || !(t%30))CHECK(render(a));
            if(a->death_active)break;
        }
        CHECK(t==(last?392u:301u));++deaths;counter=a->game->frame_game.counter;
        for(t=1;t<=60;++t)CHECK(death_tick(a) && a->death_active && a->death_fade==t*64 && !a->reload);
        CHECK(death_tick(a) && !a->menu_active && render(a));
        for(i=0;i<(unsigned)(a->bitmap.bmiHeader.biWidth*-a->bitmap.bmiHeader.biHeight);++i)CHECK(!display_pixels(a)[i]);
        {HDC dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);}++black_frames;
        CHECK(a->death_black && death_tick(a) && !a->advance);
        if(last){
            CHECK(a->menu_active && !a->death_active && !a->reload && a->menu->page==SSA_MENU_MAIN && a->game->frame_game.counter==counter);++menus;
            CHECK(!a->game->game.life_game.continue_flag);
            for(t=0;t<60;++t)CHECK(ssa_menu_step(a->menu,0)==1 && menu_action(a));
            CHECK(ssa_menu_step(a->menu,SSA_MENU_DOWN)==1 && menu_action(a) && a->menu->selection==1);
            CHECK(swprintf(path,32768,L"%ls/death-main-%u-%u-%u.bmp",argv[2],hud,mode,lang)>0 && render(a) && snapshot(a,path));
        }else{
            unsigned briefings=a->net_briefings,maps=a->map_views,starts=a->map_starts,reveals=a->level_fade_starts;
            CHECK(a->reload && a->death_active && !a->menu_active && a->game->game.life_game.continue_flag==lives-1);
            CHECK(death_tick(a) && a->game->game.life_game.continue_flag==lives-1 && a->game->frame_game.counter==counter);
            a->reveal_presented_at.QuadPart=1; /* A previous reveal must not shorten this fade. */
            CHECK(reload(a) && !a->reload && !a->menu_active && !a->net_active && !a->net_prelevel && !a->map_active);
            CHECK(a->level_revealing && a->level_fade==LEVEL_FADE_TICKS && !a->reveal_presented_at.QuadPart);
            CHECK(a->net_briefings==briefings && a->map_views==maps && a->map_starts==starts && a->level_fade_starts==reveals+1);
            CHECK(a->game->frame_game.counter==1 && a->game->weapon.object.y==0x5000);
            CHECK(a->game->game.life_game.continue_flag==lives-1 && a->game->gameplay.life_game.continue_flag==lives-1);
            CHECK(a->game->players[0].inventory.health==100 && a->game->score==a->entry.score && a->game->score==123456);
            CHECK(!memcmp(&a->game->players[0].inventory,&a->entry.inventory,sizeof a->entry.inventory));
            {HDC dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);}
            CHECK(a->reveal_presented_at.QuadPart>1);
            for(i=0;i<(unsigned)(a->back_width*a->back_height);++i)CHECK(!(a->back_pixels[i]&0xffffff));
            {int32_t x=a->game->players[0].entity.body.x,y=a->game->players[0].entity.body.y;
                ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
                ssa_pc_input_runtime_key(&a->game->input_runtime,'W',1,0);
                for(i=0;i<6;++i)CHECK(game_tick(a)==1);
                CHECK(a->game->players[0].entity.body.x!=x || a->game->players[0].entity.body.y!=y);release(a);}
            ++retries;
        }
        ssa_native_game_free(a->game);ssa_pc_renderer_free(&a->renderer);
    }
    /* Exercise run(), direct retry reveal and final Game Over. Initial startup
     * retains ordinary briefings/maps; the retry must not show either. */
    for(hud=0;hud<2;++hud)for(mode=0;mode<2;++mode){
        unsigned briefings=a->net_briefings,maps=a->map_views,starts=a->map_starts,reveals=a->level_fade_starts;
        CHECK(ssa_native_game_init_campaign(a->game,a->rom,a->rom_size,0,1,NULL)==1);
        a->settings.hud_style=hud;a->settings.renderer=mode?SSA_RENDER_GBA:SSA_RENDER_SOFTWARE;
        a->original_renderer=mode;a->language=0;a->difficulty=1;a->direct_level=1;
        a->startup_test=1;a->startup_restarts=a->menu_active=a->map_active=a->net_active=0;
        a->death_active=a->reload=a->advance=a->failed=a->level_revealing=0;
        level_options(a,a->game);CHECK(ssa_native_game_carry_capture(&a->entry,a->game)==1);bind(a);
        a->game->game.life_game.continue_flag=a->game->gameplay.life_game.continue_flag=2;
        clock_killed=clock_returned=clock_timeout=0;clock_start=GetTickCount64();
        clock_retry_reveals=0;
        CHECK(SetTimer(a->window,1,20,NULL));CHECK(run(a));KillTimer(a->window,1);
        CHECK(clock_killed==2 && clock_returned && clock_retry_reveals==1 && !clock_timeout);
        CHECK(a->net_briefings==briefings+1 && a->map_views==maps+1 && a->map_starts==starts+1 && a->level_fade_starts==reveals+2);++clock_paths;
        ShowWindow(a->window,SW_HIDE);ssa_native_game_free(a->game);ssa_pc_renderer_free(&a->renderer);
    }
    ssa_win32_audio_close(&a->audio);DestroyWindow(a->window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    if(a->back_dc){if(a->back_original)SelectObject(a->back_dc,a->back_original);if(a->back_bitmap)DeleteObject(a->back_bitmap);DeleteDC(a->back_dc);}
    free(a->netricsa);free(a->menu);free(a->story);free(a->game);free(a->rom);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"native_frames\":%u,\"deaths\":%u,\"retries\":%u,\"main_returns\":%u,\"black_presentations\":%u,\"clock_paths\":%u,\"renderers\":2,\"hud_styles\":2,\"languages\":3,\"output_muted\":true}\n",
        checks,frames,deaths,retries,menus,black_frames,clock_paths);return 0;
}
