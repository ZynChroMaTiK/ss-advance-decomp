/* Hidden-window test of the real frontend routes; controlled completion
 * boundaries are not campaign playthrough evidence. Never grabs user input. */
#define wmain embedded_game_entry
#include "../platform/win32/game.c"
#undef wmain
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows intermission line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void key(App *a,unsigned k){SendMessageW(a->window,WM_KEYDOWN,k,0);SendMessageW(a->window,WM_KEYUP,k,0);CHECK(!a->failed);}
static void menu_tick(App *a){
    CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;CHECK(menu_action(a));
    if(!a->menu->quit_requested)CHECK(ssa_menu_step(a->menu,0)==1);
}
static void preview(App *a,const wchar_t *directory,const wchar_t *name,unsigned renderer){
    wchar_t path[32768];if(!directory)return;
    CHECK(swprintf(path,32768,L"%ls/%ls-%u.bmp",directory,name,renderer)>0);
    CHECK(render(a) && snapshot(a,path));
}
static void pointer(App *a,int x,int y){
    RECT bounds;int left,top,width,height,px,py;CHECK(GetClientRect(a->window,&bounds));
    CHECK(ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&left,&top,&width,&height));
    px=left+x*width/240;py=top+y*height/160;
    SendMessageW(a->window,WM_MOUSEMOVE,0,MAKELPARAM(px,py));
    SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));
    SendMessageW(a->window,WM_LBUTTONUP,0,MAKELPARAM(px,py));CHECK(intermission_tick(a));
}
static void clean_capture(App *a){
    SsaSpriteOam saved=a->game->oam,empty={0};const uint32_t *clean;size_t count;unsigned i;
    CHECK(ssa_sprite_oam_init(&empty,a->rom,a->rom_size)==1);
    for(i=0;i<128;++i)empty.entries[i].attribute0=0x200;
    a->game->oam=empty;a->screen_active=0;CHECK(render(a));
    clean=a->original_renderer?a->game->rgb888:a->renderer.pixels;count=(size_t)a->screen_width*a->screen_height;
    CHECK(!memcmp(clean,a->screen_backdrop,count*sizeof(*clean)));
    a->screen_active=1;a->game->oam=saved;CHECK(render(a));
}
int wmain(int argc,wchar_t **argv){
    App *a=calloc(1,sizeof(*a));WNDCLASSEXW wc={0};unsigned original,route,t,starts=0,pause_paths=0,menu_quits=0;
    CHECK((argc==2 || argc==3) && a);CHECK(read_rom(a,argv[1]));
    a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->difficulty=1;a->mute=1;a->render_width=480;a->render_height=320;
    a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a->bitmap.bmiHeader.biWidth=240;a->bitmap.bmiHeader.biHeight=-160;
    a->bitmap.bmiHeader.biPlanes=1;a->bitmap.bmiHeader.biBitCount=32;a->bitmap.bmiHeader.biCompression=BI_RGB;
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=CLASS_NAME;
    CHECK(RegisterClassExW(&wc));
    a->game=calloc(1,sizeof(*a->game));CHECK(a->game);
    a->window=CreateWindowExW(0,CLASS_NAME,L"Intermission test",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,wc.hInstance,a);
    CHECK(a->window);
    /* The original completion gate reads music player0, not sound7's
     * independent effect player. No physical audio device is opened here. */
    CHECK(screen_completion_music_finished(a)==1);
    InitializeCriticalSection(&a->audio.mutex);a->audio.device=(HWAVEOUT)(uintptr_t)1;
    a->audio.mixer.players[1].song=7;a->audio.mixer.players[1].active=1;
    CHECK(screen_completion_music_finished(a)==1);
    a->audio.mixer.players[0].active=1;
    CHECK(screen_completion_music_finished(a)==0);
    a->audio.mixer.players[1].active=0;CHECK(screen_completion_music_finished(a)==0);
    a->audio.device=NULL;memset(&a->audio.mixer,0,sizeof(a->audio.mixer));DeleteCriticalSection(&a->audio.mutex);
    for(original=0;original<2;++original)for(route=0;route<3;++route){uint32_t counter,elapsed;unsigned level=route==0?0:route==1?5:11;
        a->original_renderer=original;a->level=level;a->language=route;
        CHECK(ssa_native_game_init_campaign(a->game,a->rom,a->rom_size,level,1,NULL)==1);
        level_options(a,a->game);CHECK(ssa_native_game_carry_capture(&a->entry,a->game)==1);
        CHECK(ssa_native_game_tick(a->game)==1);CHECK(render(a));counter=a->game->frame_game.counter;elapsed=a->game->frame_game.elapsed;
        key(a,VK_ESCAPE);CHECK(a->screen_active && !a->intermission->completion && !a->captured);
        for(t=0;t<24;++t)CHECK(intermission_tick(a));
        CHECK(a->game->frame_game.counter==counter && a->game->frame_game.elapsed==elapsed && render(a));
        CHECK((unsigned)a->bitmap.bmiHeader.biWidth==(original?240u:480u));
        if(!route)preview(a,argc==3?argv[2]:NULL,L"pause",original);
        key(a,VK_DOWN);CHECK(intermission_tick(a));key(a,VK_RETURN);CHECK(intermission_tick(a));
        CHECK(a->intermission->pause.selection==3);key(a,VK_ESCAPE);CHECK(intermission_tick(a) && !a->intermission->pause.selection);
        key(a,VK_ESCAPE);CHECK(intermission_tick(a) && !a->screen_active);++pause_paths;
        key(a,'P');CHECK(a->screen_active);pointer(a,120,96);CHECK(a->intermission->pause.selection==3);
        pointer(a,120,80);CHECK(!a->screen_active && a->menu_active && a->menu->page==SSA_MENU_MAIN);++pause_paths;
        a->running=1;
        for(t=0;t<40;++t){CHECK(menu_frozen(a));CHECK(ssa_menu_step(a->menu,0)==1);CHECK(menu_action(a));}
        CHECK(a->running && IsWindow(a->window) && a->game->frame_game.counter==counter);
        CHECK(!a->menu->quit_requested && !a->menu->done);
        if(!route)preview(a,argc==3?argv[2]:NULL,L"main-menu",original);
        /* Both routes for the separate application-level Quit. */
        if(original){RECT bounds;int left,top,width,height;LPARAM point;
            CHECK(GetClientRect(a->window,&bounds) && ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&left,&top,&width,&height));
            point=MAKELPARAM(left+120*width/240,top+146*height/160);
            SendMessageW(a->window,WM_MOUSEMOVE,0,point);SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);
            SendMessageW(a->window,WM_LBUTTONUP,0,point);
        }else{
            for(t=0;t<4;++t){key(a,VK_DOWN);CHECK(ssa_menu_step(a->menu,a->menu_pressed)==1);a->menu_pressed=0;}
            key(a,VK_RETURN);
        }
        menu_tick(a);CHECK(a->running && a->menu->page==SSA_MENU_QUIT && a->menu->selection==1 && !a->menu->quit_requested);
        if(!route)preview(a,argc==3?argv[2]:NULL,L"quit-confirmation",original);
        key(a,VK_RETURN);menu_tick(a); /* No is the default. */
        CHECK(a->running && a->menu->page==SSA_MENU_MAIN && a->menu->selection==4 && !a->menu->quit_requested);
        key(a,VK_RETURN);menu_tick(a);key(a,VK_UP);menu_tick(a);key(a,VK_ESCAPE);menu_tick(a);
        CHECK(a->running && a->menu->page==SSA_MENU_MAIN && a->menu->selection==4 && !a->menu->quit_requested);
        key(a,VK_RETURN);menu_tick(a);
        CHECK(a->menu->page==SSA_MENU_QUIT && a->menu->selection==1);
        if(original){RECT bounds;int left,top,width,height;LPARAM point;
            CHECK(GetClientRect(a->window,&bounds) && ssa_ui_viewport(bounds.right,bounds.bottom,240,160,&left,&top,&width,&height));
            point=MAKELPARAM(left+120*width/240,top+120*height/160);
            SendMessageW(a->window,WM_MOUSEMOVE,0,point);SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);
            SendMessageW(a->window,WM_LBUTTONUP,0,point);
        }else{key(a,VK_UP);menu_tick(a);key(a,VK_RETURN);}
        menu_tick(a);CHECK(a->menu->quit_requested && !a->running && !a->reload && !a->advance);
        ++menu_quits;
        a->running=1;
        a->menu_active=0;
        /* Drive the actual game_tick completion hook with a controlled exit. */
        a->game->game.life_game.input_state=3;a->game->score=1234;a->game->frame_game.elapsed=120*60;
        a->game->players[0].inventory.ammo[2]=5;
        a->game->players[0].inventory.health=100;a->tally_recorded=0;
        CHECK(game_tick(a)==1 && a->screen_active && a->intermission->completion);counter=a->game->frame_game.counter;
        clean_capture(a);
        CHECK(!a->profile.enabled); /* No user save touched. */
        for(t=0;t<30000 && a->screen_active;++t){
            unsigned stage=a->intermission->tally.stage,skip=(stage==1 || stage==4) && a->intermission->tally.hold==30;
            uint32_t pending=a->intermission->tally.pending,score=a->intermission->tally.score;
            if(skip){
                if(!route){ /* A short click must work even after button-up. */
                    SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,0);SendMessageW(a->window,WM_LBUTTONUP,0,0);
                }else key(a,route==1?VK_RETURN:VK_SPACE);
                CHECK(a->screen_pressed==1 && !a->screen_held);
            }
            CHECK(intermission_tick(a));
            if(skip)CHECK(!a->intermission->tally.pending && a->intermission->tally.score==score+pending);
            if(a->intermission->final_visible_ticks<180)CHECK(a->screen_active);
            CHECK(a->game->frame_game.counter==counter);
            if(a->screen_active && !route && (t==40 || (a->intermission->tally.stage==4 && a->intermission->tally.hold==30) ||
               (a->intermission->tally.stage==6 && a->intermission->final_visible_ticks==90)))
                preview(a,argc==3?argv[2]:NULL,t==40?L"time":a->intermission->tally.stage==4?L"ammo":L"sam",original);
        }
        CHECK(t<30000 && a->tally_recorded && a->intermission->tally.recorded);
        CHECK(a->completed_carry.score==a->intermission->tally.score && a->statistics.score==a->completed_carry.score);
        CHECK(a->statistics.score>1234);++starts;
        if(level==11){CHECK(a->story_active && a->story->next_level==12);a->story_active=0;}
        else{
            CHECK(a->advance && a->reload && reload(a));
            CHECK(a->level==level+1 && a->game->score==a->completed_carry.score && a->entry.score==a->game->score);
            CHECK(a->game->players[0].inventory.health==100);
            CHECK(level==5?a->story_active && a->story->next_level==6:a->net_active && a->net_prelevel);
        }
        a->story_active=a->net_active=a->net_prelevel=a->map_active=a->screen_active=0;
        a->reload=a->advance=0;ssa_pc_renderer_free(&a->renderer);ssa_native_game_free(a->game);
    }
    DestroyWindow(a->window);UnregisterClassW(CLASS_NAME,wc.hInstance);
    if(a->back_dc){if(a->back_original)SelectObject(a->back_dc,a->back_original);if(a->back_bitmap)DeleteObject(a->back_bitmap);DeleteDC(a->back_dc);}
    free(a->intermission);free(a->game);free(a->menu);free(a->story);free(a->netricsa);free(a->map_view);
    free(a->screen_pixels);free(a->screen_backdrop);free(a->net_backdrop);free(a->net_hud_frame);free(a->net_pixels);free(a->rom);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"completion_routes\":%u,\"pause_paths\":%u,\"main_menu_quits\":%u,\"renderers\":2,\"languages\":3,\"hidden_window\":true}\n",checks,starts,pause_paths,menu_quits);return 0;
}
