#define wmain embedded_automap_game_entry
#include "../platform/win32/game.c"
#undef wmain
static unsigned checks,paths;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows automap line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void key(App *a,unsigned k){SendMessageW(a->window,WM_KEYDOWN,k,0);SendMessageW(a->window,WM_KEYUP,k,0);CHECK(!a->failed);}
int wmain(int argc,wchar_t **argv){
    App *a=calloc(1,sizeof(*a));WNDCLASSW wc={0};unsigned mode,chapter,i,zoom,weapon;wchar_t path[32768];
    CHECK(argc==3 && a && read_rom(a,argv[1]));a->game=calloc(1,sizeof(*a->game));CHECK(a->game);
    CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
    wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA automap test";CHECK(RegisterClassW(&wc));
    a->window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,50,50,960,640,NULL,NULL,wc.hInstance,a);CHECK(a->window);
    a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a->bitmap.bmiHeader.biPlanes=1;a->bitmap.bmiHeader.biBitCount=32;
    a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->render_width=960;a->render_height=640;
    ssa_native_game_free(a->game);
    for(mode=0;mode<2;++mode)for(chapter=0;chapter<2;++chapter){
        CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,chapter*6)==1);a->level=chapter*6;a->original_renderer=mode;
        a->menu_active=a->net_active=a->screen_active=a->map_active=a->story_active=0;
        a->captured=1;ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
        CHECK(game_tick(a)==1);weapon=a->game->gameplay.weapon.requested;
        key(a,'F');CHECK(!(a->game->players[0].motion.movement.flags&8));
        a->settings.automap_enabled=0;key(a,'M');CHECK(!(a->game->players[0].motion.movement.flags&8));
        a->settings.automap_enabled=1;a->audio.mixer.muted=1;
        key(a,'M');CHECK((a->game->players[0].motion.movement.flags&8) && a->audio.mixer.muted);
        CHECK(a->bitmap.bmiHeader.biWidth==240 && display_pixels(a)==a->game->rgb888);
        CHECK(game_tick(a)==1 && a->game->automap_zoom==8192 && render(a));
        CHECK(swprintf(path,32768,L"%ls/automap-default-%u-%u.bmp",argv[2],mode,chapter)>0 && snapshot(a,path));
        SendMessageW(a->window,WM_KEYDOWN,'M',(LPARAM)1<<30);CHECK(a->game->players[0].motion.movement.flags&8);
        zoom=a->game->automap_zoom;
        SendMessageW(a->window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA/2),0);CHECK(a->game->automap_zoom==zoom);
        SendMessageW(a->window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA/2),0);CHECK(a->game->automap_zoom>zoom && a->game->gameplay.weapon.requested==weapon);
        SendMessageW(a->window,WM_KEYDOWN,VK_OEM_MINUS,0);
        for(i=0;i<110;++i)CHECK(game_tick(a)==1);
        SendMessageW(a->window,WM_KEYUP,VK_OEM_MINUS,0);CHECK(a->game->automap_zoom==4096);
        SendMessageW(a->window,WM_KEYDOWN,VK_OEM_PLUS,0);
        for(i=0;i<110;++i)CHECK(game_tick(a)==1);
        SendMessageW(a->window,WM_KEYUP,VK_OEM_PLUS,0);CHECK(a->game->automap_zoom==65536);
        CHECK(render(a) && a->bitmap.bmiHeader.biWidth==240 && a->bitmap.bmiHeader.biHeight==-160 && display_pixels(a)==a->game->rgb888);
        CHECK(swprintf(path,32768,L"%ls/automap-%u-%u.bmp",argv[2],mode,chapter)>0 && snapshot(a,path));
        key(a,'2');key(a,'Q');CHECK(a->game->gameplay.weapon.requested==weapon);
        a->captured=0; /* This hidden test never owns the physical cursor. */
        key(a,VK_ESCAPE);CHECK(a->screen_active && !a->intermission->completion);
        key(a,VK_ESCAPE);CHECK(intermission_tick(a) && !a->screen_active && (a->game->players[0].motion.movement.flags&8));
        key(a,VK_TAB);CHECK(a->net_active && !a->net_prelevel && render(a));
        key(a,VK_TAB);
        for(i=0;i<20 && !a->netricsa->done;++i){CHECK(ssa_netricsa_pc_step(a->netricsa,a->net_pressed)>=0);a->net_pressed=0;}
        CHECK(i<20 && finish_netricsa(a) && !a->net_active && (a->game->players[0].motion.movement.flags&8));
        a->captured=1;ssa_pc_input_runtime_focus(&a->game->input_runtime,1);ssa_pc_input_runtime_enable(&a->game->input_runtime,1);
        key(a,'M');CHECK(!(a->game->players[0].motion.movement.flags&8));
        CHECK(a->bitmap.bmiHeader.biWidth==(mode?240:960));
        CHECK(game_tick(a)==1 && render(a));
        CHECK(a->bitmap.bmiHeader.biWidth==(mode?240:960));
        key(a,'M');CHECK(a->game->players[0].motion.movement.flags&8);
        SendMessageW(a->window,WM_KEYDOWN,VK_OEM_PLUS,0);CHECK(a->automap_keys==1);
        a->captured=0;release(a);CHECK(!a->automap_keys); /* Focus/pause cannot leave zoom held. */
        a->game->players[0].inventory.health=a->game->players[0].entity_health=0;
        CHECK(game_tick(a)==1 && !(a->game->players[0].motion.movement.flags&8));
        key(a,'M');CHECK(!(a->game->players[0].motion.movement.flags&8));
        ssa_pc_renderer_free(&a->renderer);ssa_native_game_free(a->game);++paths;
    }
    DestroyWindow(a->window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    free(a->netricsa);free(a->intermission);free(a->screen_backdrop);free(a->screen_pixels);free(a->game);free(a->rom);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"paths\":%u,\"renderers\":2,\"chapters\":2,\"hidden_window\":true}\n",checks,paths);return 0;
}
