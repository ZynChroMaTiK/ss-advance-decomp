/* Real file persistence plus a normal-input level completion through the exact
 * frontend tick/save hook. Files are explicitly supplied test paths only. */
#define wmain embedded_game_entry
#include "../platform/win32/game.c"
#undef wmain
#undef near
static App *profile_bot;
static int profile_bot_step(SsaNativeGame *g){
 profile_bot->game=g;profile_bot->level=g->level;
 return game_tick(profile_bot)==1?ssa_native_game_draw(g):-1;
}
#define ssa_native_game_step profile_bot_step
#define main profile_normal_input_run
#include "test_native_playability.c"
#undef main
#undef ssa_native_game_step
static unsigned profile_checks;
#define VERIFY(x) do{++profile_checks;if(!(x)){fprintf(stderr,"disk profile line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int wmain(int argc,wchar_t **argv){
 App *a=calloc(1,sizeof(*a));SsaWin32Profile *b=calloc(1,sizeof(*b));SsaMenu *menu=calloc(1,sizeof(*menu));
 HANDLE f;DWORD written;uint8_t damaged[7]={0};char *bot_args[]={"profile-normal-input",NULL};
 VERIFY(argc==3 && a && b && menu);VERIFY(ssa_win32_profile_open(&a->profile,argv[2],1));
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_CLEAR,0));
 VERIFY(ssa_win32_profile_open(b,argv[2],1));
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_COMPLETE,0));
 VERIFY(ssa_win32_profile_store(b,SSA_SAVE_LANGUAGE,2) && b->data.completed==1);
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_COMPLETE,5) && a->profile.data.completed==33 && a->profile.data.language==2);
 VERIFY(ssa_win32_profile_store(b,SSA_SAVE_CLEAR,0));
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_COMPLETE,2) && a->profile.data.completed==4 && !a->profile.data.language);
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_CLEAR,0));
 VERIFY(SetFileAttributesW(argv[2],FILE_ATTRIBUTE_READONLY));
 VERIFY(!ssa_win32_profile_store(&a->profile,SSA_SAVE_COMPLETE,3));
 VERIFY(SetFileAttributesW(argv[2],FILE_ATTRIBUTE_NORMAL));
 VERIFY(ssa_win32_profile_open(b,argv[2],1) && !b->data.completed);
 f=CreateFileW(argv[2],GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);VERIFY(f!=INVALID_HANDLE_VALUE);
 VERIFY(WriteFile(f,damaged,sizeof damaged,&written,NULL) && written==sizeof damaged);CloseHandle(f);
 VERIFY(!ssa_win32_profile_open(b,argv[2],1) && !b->data.completed && !b->data.language);
 VERIFY(ssa_win32_profile_store(b,SSA_SAVE_LANGUAGE,2));
 VERIFY(ssa_win32_profile_store(b,SSA_SAVE_COMPLETE,0));
 VERIFY(read_rom(a,argv[1]));VERIFY(ssa_menu_init(menu,a->rom,a->rom_size,1)==1);
 menu->language=2;menu->completed_levels=1;menu->page=menu->draw_page=SSA_MENU_OPTIONS;menu->selection=3;
 VERIFY(ssa_menu_step(menu,SSA_MENU_CONFIRM)==1 && menu->selection==1);
 VERIFY(ssa_menu_step(menu,SSA_MENU_CONFIRM)==1 && !menu->clear_requested);
 VERIFY(ssa_win32_profile_open(&a->profile,argv[2],1) && a->profile.data.completed==1 && a->profile.data.language==2);
 VERIFY(ssa_menu_step(menu,SSA_MENU_CONFIRM)==1);VERIFY(ssa_menu_step(menu,SSA_MENU_UP)==1);
 VERIFY(ssa_menu_step(menu,SSA_MENU_BACK)==1 && !menu->clear_requested);
 VERIFY(ssa_menu_step(menu,SSA_MENU_CONFIRM)==1);VERIFY(ssa_menu_step(menu,SSA_MENU_UP)==1);
 VERIFY(ssa_menu_step(menu,SSA_MENU_CONFIRM)==1 && menu->clear_requested);
 save_change(a,SSA_SAVE_CLEAR,0);VERIFY(!a->profile.last_error);
 VERIFY(ssa_win32_profile_open(b,argv[2],1) && !b->data.language && !b->data.completed);
 VERIFY(ssa_win32_profile_store(&a->profile,SSA_SAVE_LANGUAGE,1));
 profile_bot=a;VERIFY(profile_normal_input_run(1,bot_args)==0);a->game=NULL;
 VERIFY(a->completion_recorded && a->profile.data.completed==1 && a->profile.data.language==1);
 VERIFY(ssa_win32_profile_open(b,argv[2],1) && b->data.completed==1 && b->data.language==1);
 VERIFY(ssa_profile_unlocked(&b->data,1)==1 && ssa_profile_unlocked(&b->data,2)==0);
 free(menu);free(a->rom);free(a);free(b);
 printf("{\"result\":\"pass\",\"disk_checks\":%u,\"real_level_completions\":1,\"saved_language\":1,\"saved_completed\":1}\n",profile_checks);return 0;
}
