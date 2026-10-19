/* Actual frontend messages and files; explicit test directory only. */
#define wmain embedded_scores_game_entry
#include "../platform/win32/game.c"
#undef wmain
static unsigned checks,paths;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Windows scores line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void tick_menu(App *a){
    CHECK(ssa_menu_step_input(a->menu,a->menu_pressed,0)==1);a->menu_pressed=0;score_change(a);
}
static void key(App *a,unsigned k){SendMessageW(a->window,WM_KEYDOWN,k,0);SendMessageW(a->window,WM_KEYUP,k,0);tick_menu(a);}
int wmain(int argc,wchar_t **argv){
    App *a=calloc(1,sizeof(*a));SsaWin32Scores *other=calloc(1,sizeof(*other));WNDCLASSW wc={0};
    wchar_t path[32768],shot[32768];unsigned language,mode,i,counter;HANDLE f;DWORD n;uint8_t bad[7]={0};SsaScores saved;
    CHECK(argc==3 && a && other && read_rom(a,argv[1]));
    CHECK(swprintf(path,32768,L"%ls/scores-test.dat",argv[2])>0);
    CHECK(ssa_win32_scores_open(&a->scores,a->rom,a->rom_size,path,1));
    CHECK(ssa_win32_scores_store(&a->scores,1,0,NULL));
    CHECK(ssa_win32_scores_open(other,a->rom,a->rom_size,path,1));
    CHECK(ssa_win32_scores_store(&a->scores,0,3000000,"FIRST_______"));
    CHECK(ssa_win32_scores_store(other,0,2500000,"SECOND______"));
    CHECK(ssa_win32_scores_open(&a->scores,a->rom,a->rom_size,path,1));
    CHECK(a->scores.data.entries[0].score==3000000 && a->scores.data.entries[1].score==2500000);
    CHECK(!memcmp(a->scores.data.entries[0].name,"FIRST       ",12) && !memcmp(a->scores.data.entries[1].name,"SECOND      ",12));
    /* Atomic replacement failure preserves the last good file. */
    saved=a->scores.data;CHECK(SetFileAttributesW(path,FILE_ATTRIBUTE_READONLY));
    CHECK(!ssa_win32_scores_store(&a->scores,0,4000000,"UNSAVED_____"));
    CHECK(a->scores.data.entries[0].score==4000000);
    CHECK(SetFileAttributesW(path,FILE_ATTRIBUTE_NORMAL));
    CHECK(ssa_win32_scores_open(other,a->rom,a->rom_size,path,1));
    CHECK(!memcmp(&other->data,&saved,sizeof saved));
    /* Loading corrupt data never rewrites it; an explicit submission repairs. */
    f=CreateFileW(path,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(f!=INVALID_HANDLE_VALUE);
    CHECK(WriteFile(f,bad,sizeof bad,&n,NULL) && n==sizeof bad);CloseHandle(f);
    CHECK(!ssa_win32_scores_open(other,a->rom,a->rom_size,path,1) && other->data.entries[0].score==2000000);
    f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(f!=INVALID_HANDLE_VALUE);
    CHECK(GetFileSize(f,NULL)==sizeof bad);CloseHandle(f);
    CHECK(ssa_win32_scores_store(other,0,2100000,"REPAIRED____"));
    CHECK(ssa_win32_scores_open(&a->scores,a->rom,a->rom_size,path,1) && a->scores.data.entries[0].score==2100000);
    a->game=calloc(1,sizeof(*a->game));CHECK(a->game && ssa_native_game_init(a->game,a->rom,a->rom_size,0)==1);
    a->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;a->running=1;
    wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA Scores test";CHECK(RegisterClassW(&wc));
    a->window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,50,50,960,640,NULL,NULL,wc.hInstance,a);CHECK(a->window);
    a->bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);a->bitmap.bmiHeader.biPlanes=1;a->bitmap.bmiHeader.biBitCount=32;
    for(mode=0;mode<2;++mode)for(language=0;language<3;++language){RECT client;int left,top,width,height;LPARAM point;
        CHECK(ssa_win32_scores_store(&a->scores,1,0,NULL));a->language=language;a->original_renderer=mode;
        a->game->score=1750000;a->tally_recorded=0;counter=a->game->frame_game.counter;
        if(!a->intermission)a->intermission=calloc(1,sizeof(*a->intermission));CHECK(a->intermission);
        a->intermission->completion=0;a->intermission->pause.input_state=5;
        CHECK(finish_intermission(a) && a->menu_active && a->menu->page==SSA_MENU_SCORE_NAME);
        a->menu->fade_volume=a->menu->fading=0;
        /* WASD/M/Space are characters here, never navigation or mute. */
        for(i=0;i<5;++i){static const char name[]="sam w";unsigned vk=name[i]>='a' && name[i]<='z'?(unsigned)name[i]-32:(unsigned)name[i];
            SendMessageW(a->window,WM_KEYDOWN,vk,0);
            SendMessageW(a->window,WM_CHAR,(WPARAM)name[i],0);SendMessageW(a->window,WM_KEYUP,vk,0);tick_menu(a);}
        CHECK(!memcmp(a->menu->scores.name,"SAM_W_______",12));
        key(a,VK_HOME);CHECK(!a->menu->score_cursor);
        key(a,VK_DELETE);CHECK(!memcmp(a->menu->scores.name,"AM_W________",12));
        SendMessageW(a->window,WM_CHAR,'s',0);tick_menu(a);key(a,VK_END);
        SendMessageW(a->window,WM_CHAR,8,0);tick_menu(a);SendMessageW(a->window,WM_CHAR,'x',0);tick_menu(a);
        CHECK(!memcmp(a->menu->scores.name,"SAM_X_______",12));
        CHECK(render(a));CHECK(swprintf(shot,32768,L"%ls/name-%u-%u.bmp",argv[2],mode,language)>0 && snapshot(a,shot));
        CHECK(GetClientRect(a->window,&client) && ssa_ui_viewport(client.right,client.bottom,240,160,&left,&top,&width,&height));
        point=MAKELPARAM(left+120*width/240,top+120*height/160);
        SendMessageW(a->window,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(a->window,WM_LBUTTONUP,0,point);tick_menu(a);
        CHECK(a->menu->page==SSA_MENU_SCORES && !a->menu->score_submitted && a->menu->score_highlight==3);
        CHECK(ssa_win32_scores_open(other,a->rom,a->rom_size,path,1));
        CHECK(other->data.entries[2].score==1750000 && !memcmp(other->data.entries[2].name,"SAM X       ",12));
        CHECK(!memcmp(other->data.name,"SAM_X_______",13));
        for(i=0;i<80;++i)tick_menu(a);CHECK(render(a));
        CHECK(swprintf(shot,32768,L"%ls/record-%u-%u.bmp",argv[2],mode,language)>0 && snapshot(a,shot));
        key(a,VK_ESCAPE);tick_menu(a);CHECK(a->menu->page==SSA_MENU_MAIN && !a->menu->done);
        CHECK(a->game->frame_game.counter==counter && !IsWindowVisible(a->window));++paths;
        /* Completed ending offers the bonus-adjusted final score. */
        if(!a->story)a->story=calloc(1,sizeof(*a->story));CHECK(a->story);
        a->story->next_level=12;a->story_active=1;a->tally_recorded=1;a->completed_carry.score=3500000;
        CHECK(finish_story(a) && a->menu->page==SSA_MENU_SCORE_NAME && a->menu->score_candidate==3500000);
        key(a,VK_RETURN);CHECK(a->menu->page==SSA_MENU_SCORES && a->scores.data.entries[0].score==3500000);
        saved=a->scores.data;tick_menu(a);tick_menu(a);CHECK(!memcmp(&saved,&a->scores.data,sizeof saved));
        /* A fresh run must not inherit the previous ending/tally latches. */
        a->menu->level=a->menu->loadout=0;a->menu->difficulty=1;
        CHECK(finish_menu(a) && !a->tally_recorded && !a->completion_recorded && !a->reload && !a->advance);
        CHECK(!a->game->score && a->story_active && a->story->next_level==0);
        a->story_active=0;
    }
    /* Clear Data's existing confirmed action clears the separate leaderboard. */
    a->profile.enabled=0;save_change(a,SSA_SAVE_CLEAR,0);
    CHECK(ssa_win32_scores_open(other,a->rom,a->rom_size,path,1) && !memcmp(&other->data,&other->defaults,sizeof other->data));
    CHECK(a->menu->scores.entries[0].score==2000000);
    DestroyWindow(a->window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    ssa_native_game_free(a->game);free(a->game);free(a->menu);free(a->story);free(a->intermission);free(a->rom);free(other);free(a);
    printf("{\"result\":\"pass\",\"checks\":%u,\"name_paths\":%u,\"quit_run_paths\":6,\"ending_paths\":6,\"new_run_paths\":6,\"renderers\":2,\"languages\":3,\"hidden_window\":true}\n",checks,paths);return 0;
}
