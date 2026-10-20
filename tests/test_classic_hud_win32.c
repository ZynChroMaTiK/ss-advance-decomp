#define wmain embedded_classic_hud_entry
#include "../platform/win32/game.c"
#undef wmain
static unsigned checks,paths,music_cycles;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows Classic HUD line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void menu_step(App *a){
    CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;settings_change(a);
    CHECK(menu_action(a) && !a->failed && render(a));
}
static void same_music(const SsaAudio *a,const SsaAudio *reference)
{
    CHECK(a->music_command==reference->music_command && a->music_level==reference->music_level);
    CHECK(!memcmp(&a->players[0],&reference->players[0],sizeof a->players[0]));
    CHECK(!memcmp(&a->music,&reference->music,sizeof a->music) && a->master_gain==reference->master_gain);
}
static void netricsa_music_continuity(App *a)
{
    SsaAudio *reference=malloc(sizeof(*reference));unsigned pass,i,counter=a->game->frame_game.counter;
    int16_t actual[1600],expected[1600];
    CHECK(reference && ssa_win32_audio_open(&a->audio,a->rom,a->rom_size,a->level,1,0));
    CHECK(ssa_win32_audio_begin_level(&a->audio,a->level));
    /* Hold only the test worker while advancing both mixers explicitly. UI
     * sounds may differ; music must follow uninterrupted reference playback. */
    EnterCriticalSection(&a->audio.mutex);*reference=a->audio.mixer;
    CHECK(reference->players[0].active && !reference->synth.context);
    for(pass=0;pass<2;++pass){
        CHECK(begin_netricsa(a,0));same_music(&a->audio.mixer,reference);
        for(i=0;i<60 && a->net_active;++i){
            uint32_t input=i==20 || i==22?SSA_NET_FOCUS:i==24?SSA_NET_DOWN:i==30?SSA_NET_CLOSE:0;
            CHECK(netricsa_tick(a,input));
            CHECK(ssa_audio_render(&a->audio.mixer,actual,800)==1 && ssa_audio_render(reference,expected,800)==1);
            same_music(&a->audio.mixer,reference);
            CHECK(a->game->frame_game.counter==counter);
        }
        CHECK(i<60 && !a->net_active);++music_cycles;
    }
    LeaveCriticalSection(&a->audio.mutex);ssa_win32_audio_close(&a->audio);
    memset(&a->audio,0,sizeof a->audio);free(reference);
}
int wmain(int argc,wchar_t **argv)
{
    App *a=calloc(1,sizeof(*a));WNDCLASSW wc={0};unsigned mode,i,alternate;wchar_t path[32768];
    CHECK(argc==3 && a && read_rom(a,argv[1]));a->game=calloc(1,sizeof(*a->game));CHECK(a->game);
    CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
    wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA Classic HUD check";
    CHECK(RegisterClassW(&wc));
    a->window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,wc.hInstance,a);CHECK(a->window);
    a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a->bitmap.bmiHeader.biPlanes=1;a->bitmap.bmiHeader.biBitCount=32;
    a->render_width=480;a->render_height=320;ssa_native_game_free(a->game);
    CHECK(swprintf(a->settings_path,32768,L"%ls/prelevel.settings",argv[2])>0);a->profile.enabled=1;
    for(mode=0;mode<2;++mode)for(alternate=0;alternate<2;++alternate){SsaSettings saved;RECT rect;int x,y,w,h;
        CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
        a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->settings.alternate_weapons=alternate;
        a->settings.renderer=mode?SSA_RENDER_GBA:SSA_RENDER_SOFTWARE;a->original_renderer=mode;
        level_options(a,a->game);CHECK(ssa_native_game_alternate_weapons(a->game,alternate,SSA_ALTERNATE_NEW_GAME)==1);
        CHECK(begin_menu(a) && !a->game->frame_game.counter);
        a->menu->page=a->menu->draw_page=SSA_MENU_EXTRA;a->menu->selection=0;
        SendMessageW(a->window,WM_KEYDOWN,VK_RIGHT,0);SendMessageW(a->window,WM_KEYUP,VK_RIGHT,0);menu_step(a);
        CHECK(a->settings.hud_style==SSA_HUD_CLASSIC && a->game->hud_style==SSA_HUD_CLASSIC);
        CHECK(ssa_win32_settings_read(&saved,a->settings_path) && saved.hud_style==SSA_HUD_CLASSIC && saved.alternate_weapons==alternate);
        /* The same row must also switch safely with a real pointer click. */
        CHECK(GetClientRect(a->window,&rect) && ssa_ui_viewport(rect.right,rect.bottom,240,160,&x,&y,&w,&h));
        {LPARAM point=MAKELPARAM(x+120*w/240,y+20*h/160);
            SendMessageW(a->window,WM_MOUSEMOVE,0,point);
            SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(a->window,WM_LBUTTONUP,0,point);menu_step(a);}
        CHECK(a->settings.hud_style==SSA_HUD_ORIGINAL && a->game->hud_style==SSA_HUD_ORIGINAL);
        CHECK(ssa_win32_settings_read(&saved,a->settings_path) && saved.hud_style==SSA_HUD_ORIGINAL);
        SendMessageW(a->window,WM_KEYDOWN,VK_LEFT,0);SendMessageW(a->window,WM_KEYUP,VK_LEFT,0);menu_step(a);
        CHECK(a->settings.hud_style==SSA_HUD_CLASSIC && a->game->hud_style==SSA_HUD_CLASSIC && !a->game->frame_game.counter);
        ssa_native_game_free(a->game);
    }
    a->profile.enabled=0;
    for(mode=0;mode<2;++mode){uint32_t counter;size_t count,changed;unsigned health;
        CHECK(ssa_scores_valid(&a->scores.data));a->scores.data.entries[0].score=98765432;
        CHECK(ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
        a->original_renderer=mode;a->level=0;a->settings.renderer=mode?SSA_RENDER_GBA:SSA_RENDER_SOFTWARE;
        level_options(a,a->game);CHECK(a->game->hud_style==SSA_HUD_CLASSIC);
        CHECK(a->game->high_score==98765432);
        CHECK(ssa_native_game_carry_capture(&a->entry,a->game)==1);
        a->menu_active=a->screen_active=a->net_active=a->map_active=a->story_active=0;
        for(i=0;i<24;++i)CHECK(game_tick(a)==1);
        CHECK(ssa_native_game_netricsa_unread(a->game)==0 && !a->game->hud_netricsa_ticks);
        CHECK(ssa_inventory_queue_weapon(&a->game->players[0].inventory,3)==1 && game_tick(a)==1);
        CHECK(render(a));counter=a->game->frame_game.counter;health=(unsigned)a->game->players[0].inventory.health;
        CHECK(swprintf(path,32768,L"%ls/classic-windows-%u.bmp",argv[2],mode)>0 && snapshot(a,path));
        netricsa_music_continuity(a);
        CHECK(begin_netricsa(a,0) && a->net_active && a->net_hud_removed>100);
        CHECK(ssa_native_game_netricsa_unread(a->game)==1 && !a->netricsa->has_enemies);
        CHECK(a->netricsa->category==2 && a->netricsa->weapon==2);
        CHECK(!a->game->hud_suppressed && a->game->hud_style==SSA_HUD_CLASSIC && a->game->frame_game.counter==counter);
        /* An early close does not consume the automatically selected entry. */
        CHECK(ssa_netricsa_pc_step(a->netricsa,SSA_NET_CLOSE)>=0);
        {HDC dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);}
        CHECK(ssa_native_game_netricsa_unread(a->game)==1 && finish_netricsa(a));
        CHECK(begin_netricsa(a,0) && a->netricsa->category==2 && a->netricsa->weapon==2);
        for(i=0;i<30;++i)CHECK(ssa_netricsa_pc_step(a->netricsa,0)>=0);
        CHECK(render(a));
        {HDC dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);}
        CHECK(ssa_native_game_netricsa_unread(a->game)==0);
        CHECK(swprintf(path,32768,L"%ls/classic-netricsa-%u.bmp",argv[2],mode)>0 && render(a) && snapshot(a,path));
        CHECK(finish_netricsa(a) && !a->net_active && !a->game->hud_suppressed);
        {unsigned type=12,n;int expected[3];
            a->game->damage.god_mode=1;
            CHECK(ssa_inventory_queue_weapon(&a->game->players[0].inventory,5)==1);
            for(i=0;i<180 && !(a->game->players[0].inventory.weapons&16);++i)CHECK(game_tick(a)==1);
            CHECK(ssa_native_game_netricsa_latest(a->game)==5);
            for(i=0;i<100;++i){SsaScriptActor *actor=&a->game->spawn.actors[i];
                if(actor->owned && actor->entity.type>=0xcd && actor->entity.type<=0xd6 && actor->activation.health>0){
                    type=actor->entity.type-0xcd;actor->entity.body.flags&=(uint16_t)~1u;
                    CHECK(ssa_combat_runtime_damage(&a->game->combat,&actor->entity,&a->game->players[0].entity,NULL,10000,&a->game->combat_ops)==0);break;}}
            CHECK(type<12 && game_tick(a)==1 && ssa_native_game_netricsa_latest(a->game)==(int)(11+type));
            CHECK(ssa_inventory_queue_weapon(&a->game->players[0].inventory,4)==1);
            for(i=0;i<180 && !(a->game->players[0].inventory.weapons&8);++i)CHECK(game_tick(a)==1);
            expected[0]=4;expected[1]=(int)(11+type);expected[2]=5;
            for(n=0;n<3;++n){unsigned id=(unsigned)expected[n];
                CHECK(ssa_native_game_netricsa_latest(a->game)==expected[n] && ssa_native_game_netricsa_unread(a->game)==(int)(3-n));
                CHECK(begin_netricsa(a,0) && a->netricsa->category==(id<=10?2u:1u) && a->netricsa->image==id-1);
                for(i=0;i<15;++i)CHECK(ssa_netricsa_pc_step(a->netricsa,0)>=0);
                {HDC dc=GetDC(a->window);CHECK(dc && present(a,dc));ReleaseDC(a->window,dc);}
                CHECK(ssa_native_game_netricsa_unread(a->game)==(int)(2-n) && finish_netricsa(a));
            }
            CHECK(begin_netricsa(a,0) && a->netricsa->category==4 && finish_netricsa(a));
            counter=a->game->frame_game.counter;health=(unsigned)a->game->players[0].inventory.health;
        }
        CHECK(begin_intermission(a,0) && !a->intermission->completion);
        for(i=0;i<24;++i)CHECK(intermission_tick(a));
        CHECK(swprintf(path,32768,L"%ls/classic-pause-%u.bmp",argv[2],mode)>0 && render(a) && snapshot(a,path));
        SendMessageW(a->window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(a->window,WM_KEYUP,VK_ESCAPE,0);
        CHECK(intermission_tick(a) && !a->screen_active);
        CHECK(a->game->frame_game.counter==counter && (unsigned)a->game->players[0].inventory.health==health);
        a->game->finished=1;a->game->gameplay.life_game.input_state=a->game->game.life_game.input_state=3;
        CHECK(begin_intermission(a,1) && a->intermission->completion && !a->game->hud_suppressed);
        /* A clean independent world/OAM draw must equal completion's capture.
         * Classic must be absent from this background, just like retail HUD. */
        {SsaSpriteOam saved=a->game->oam,empty={0};const uint32_t *clean;
            CHECK(ssa_sprite_oam_init(&empty,a->rom,a->rom_size)==1);
            for(i=0;i<128;++i)empty.entries[i].attribute0=0x200;
            a->game->oam=empty;a->screen_active=0;a->game->hud_suppressed=1;CHECK(render(a));
            clean=display_pixels(a);count=(size_t)a->screen_width*a->screen_height;
            CHECK(!memcmp(clean,a->screen_backdrop,count*sizeof(*clean)));
            a->game->hud_suppressed=0;CHECK(render(a));clean=display_pixels(a);changed=0;
            for(i=0;i<count;++i)changed+=clean[i]!=a->screen_backdrop[i];CHECK(changed>100);
            a->screen_active=1;a->game->oam=saved;}
        CHECK(swprintf(path,32768,L"%ls/classic-completion-%u.bmp",argv[2],mode)>0 && render(a) && snapshot(a,path));
        a->screen_active=0;a->advance=0;
        CHECK(reload(a) && a->net_active && a->net_prelevel);
        CHECK(ssa_native_game_netricsa_unread(a->game)==0 && a->game->journal.read_weapons==29 && a->netricsa->category==4);
        ssa_pc_renderer_free(&a->renderer);ssa_native_game_free(a->game);++paths;
    }
    DestroyWindow(a->window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    free(a->netricsa);free(a->intermission);free(a->net_backdrop);free(a->net_hud_frame);free(a->net_pixels);
    free(a->screen_backdrop);free(a->screen_pixels);free(a->menu);free(a->game);free(a->rom);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"paths\":%u,\"music_continuity_cycles\":%u,\"hidden_window\":true}\n",checks,paths,music_cycles);return 0;
}
