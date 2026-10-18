/* Exercise the actual frontend message handler on a hidden test window.
 * No physical input injection, mouse capture or cursor confinement. */
#define wWinMain ssa_game_fixture_entry
#include "../platform/win32/game.c"
#undef wWinMain
static unsigned wheel_checks;
#define CHECK(x) do{++wheel_checks;if(!(x)){fprintf(stderr,"wheel line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void wheel(HWND window,int delta)
{SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)(short)delta),0);}
static void requested(App *a,unsigned id)
{CHECK(a->game->gameplay.weapon.requested==id);}
static void last_weapon(HWND window,int repeat)
{SendMessageW(window,WM_KEYDOWN,'Q',repeat?((LPARAM)1<<30):0);SendMessageW(window,WM_KEYUP,'Q',0);}
static void finish_switch(App *a,unsigned id)
{
    unsigned i;
    for(i=0;i<180 && (a->game->gameplay.weapon.current!=id || a->game->weapon.object.y!=0x5000);++i)
        CHECK(ssa_native_game_tick(a->game)==1);
    CHECK(i<180 && a->game->gameplay.weapon.current==id && a->game->weapon.object.y==0x5000);
}
int main(int argc,char **argv)
{
    App a={0};WNDCLASSW wc={0};unsigned i;FILE *f;uint32_t flags;
    CHECK(argc==2);a.rom=malloc(0x800000);a.game=calloc(1,sizeof(*a.game));CHECK(a.rom && a.game);
    f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(a.rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    CHECK(ssa_native_game_init(a.game,a.rom,0x800000,0)==1);a.smoke=1;
    wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"SSA weapon wheel test";
    CHECK(RegisterClassW(&wc));
    a.window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,64,64,NULL,NULL,wc.hInstance,&a);CHECK(a.window);
    ssa_pc_input_runtime_focus(&a.game->input_runtime,1);ssa_pc_input_runtime_enable(&a.game->input_runtime,1);
    CHECK(ssa_native_game_tick(a.game)==1);a.game->damage.god_mode=1;
    a.game->players[0].inventory.weapons=1u|4u|16u|512u;
    a.game->players[0].inventory.ammo[0]=999;a.game->players[0].inventory.ammo[2]=20;
    a.game->players[0].inventory.ammo[4]=0;a.game->players[0].inventory.ammo[9]=20;
    wheel(a.window,-120);requested(&a,1); /* Uncaptured wheels do nothing. */
    a.captured=1;
    SendMessageW(a.window,WM_KEYDOWN,'R',0);SendMessageW(a.window,WM_KEYUP,'R',0);
    CHECK(!a.reload && !a.advance); /* R is unbound during gameplay. */
    wheel(a.window,-60);requested(&a,1);CHECK(a.wheel_remainder==-60);
    wheel(a.window,-60);requested(&a,3);CHECK(!a.wheel_remainder);
    CHECK(a.game->gameplay.weapon.current==1); /* Still lowering: next event uses requested3. */
    wheel(a.window,-120);requested(&a,10); /* Skip unowned and empty5. */
    wheel(a.window,-120);requested(&a,1); /* Wrap forward. */
    wheel(a.window,120);requested(&a,10); /* Wrap backward. */
    wheel(a.window,240);requested(&a,1); /* Multiple notches in one message. */
    wheel(a.window,-360);requested(&a,1); /* A full cycle must not restart a switch. */
    wheel(a.window,-120);requested(&a,3);
    for(i=0;i<180 && (a.game->gameplay.weapon.current!=3 || a.game->weapon.object.y!=0x5000);++i)
        CHECK(ssa_native_game_tick(a.game)==1);
    CHECK(i<180 && a.game->gameplay.weapon.current==3 && a.game->weapon.object.y==0x5000);
    CHECK(!a.game->error_line);
    CHECK(a.game->previous_weapon==1);
    last_weapon(a.window,1);requested(&a,3); /* OS autorepeat cannot swap. */
    last_weapon(a.window,0);requested(&a,1);finish_switch(&a,1);CHECK(a.game->previous_weapon==3);
    last_weapon(a.window,0);requested(&a,3);finish_switch(&a,3);CHECK(a.game->previous_weapon==1);
    wheel(a.window,-60);CHECK(a.wheel_remainder==-60);
    a.captured=0; /* Test release without affecting another app's physical cursor. */
    SendMessageW(a.window,WM_KILLFOCUS,0,0);CHECK(!a.wheel_remainder);
    ssa_pc_input_runtime_focus(&a.game->input_runtime,1);ssa_pc_input_runtime_enable(&a.game->input_runtime,1);a.captured=1;
    wheel(a.window,-60);requested(&a,3);CHECK(a.wheel_remainder==-60);
    wheel(a.window,60);requested(&a,3);CHECK(!a.wheel_remainder); /* Opposing partial deltas cancel. */
    a.menu_active=1;wheel(a.window,-120);last_weapon(a.window,0);requested(&a,3);a.menu_active=0;
    a.map_active=1;wheel(a.window,-120);last_weapon(a.window,0);requested(&a,3);a.map_active=0;
    a.game->frame_game.flags|=1;wheel(a.window,-120);last_weapon(a.window,0);requested(&a,3);a.game->frame_game.flags&=UINT16_C(0xfffe);
    a.game->players[0].inventory.health=0;wheel(a.window,-120);requested(&a,3);a.game->players[0].inventory.health=100;
    ssa_pc_input_runtime_enable(&a.game->input_runtime,0);wheel(a.window,-120);last_weapon(a.window,0);requested(&a,3);
    ssa_pc_input_runtime_enable(&a.game->input_runtime,1);
    a.game->players[0].inventory.weapons=4;a.game->players[0].inventory.ammo[2]=20;
    a.game->gameplay.weapon.flags=0;a.game->players[0].motion.movement.weapon_flags=0;
    wheel(a.window,-120);requested(&a,3);CHECK(!a.game->gameplay.weapon.flags);
    a.game->players[0].inventory.ammo[2]=0;wheel(a.window,120);requested(&a,3);
    CHECK(ssa_native_game_cycle_weapon(a.game,0)==0);
    a.game->players[0].inventory.weapons=1|4|512;a.game->players[0].inventory.ammo[0]=999;
    a.game->players[0].inventory.ammo[2]=20;a.game->players[0].inventory.ammo[9]=20;
    CHECK(ssa_native_game_cycle_weapon(a.game,INT_MAX)==1);requested(&a,10);
    CHECK(ssa_native_game_cycle_weapon(a.game,INT_MIN)==1);requested(&a,1);
    a.game->gameplay.weapon.requested=5; /* Current selection can become empty. */
    CHECK(ssa_native_game_cycle_weapon(a.game,1)==1);requested(&a,10);
    a.game->gameplay.weapon.requested=5;
    CHECK(ssa_native_game_cycle_weapon(a.game,-1)==1);requested(&a,3);
    flags=a.game->gameplay.weapon.flags;a.game->game.updating=1;
    CHECK(ssa_native_game_cycle_weapon(a.game,1)==-1);requested(&a,3);CHECK(a.game->gameplay.weapon.flags==flags);
    a.game->game.updating=0;CHECK(ssa_native_game_cycle_weapon(NULL,1)==-1);
    CHECK(ssa_native_game_alternate_weapons(a.game,1,1)==1);
    a.game->alternates.dual_colts=1;a.game->pickup.repeat_weapon=0;
    SendMessageW(a.window,WM_KEYDOWN,'2',0);SendMessageW(a.window,WM_KEYUP,'2',0);
    requested(&a,2);CHECK(a.game->alternates.requested_variant==1);
    SendMessageW(a.window,WM_KEYDOWN,'2',(LPARAM)1<<30); /* Repeat must not toggle. */
    CHECK(a.game->alternates.requested_variant==1);
    SendMessageW(a.window,WM_KEYDOWN,'2',0);SendMessageW(a.window,WM_KEYUP,'2',0);
    CHECK(!a.game->alternates.requested_variant);
    wheel(a.window,-120);requested(&a,2);CHECK(a.game->alternates.requested_variant==1);
    wheel(a.window,-120);requested(&a,3);
    wheel(a.window,120);requested(&a,2);CHECK(a.game->alternates.requested_variant==1);
    wheel(a.window,120);requested(&a,2);CHECK(!a.game->alternates.requested_variant);
    CHECK(ssa_native_game_alternate_weapons(a.game,0,0)==1);
    a.captured=0;DestroyWindow(a.window);CHECK(UnregisterClassW(wc.lpszClassName,wc.hInstance));
    ssa_native_game_free(a.game);free(a.game);free(a.rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"physical_input\":false,\"real_window_messages\":true}\n",wheel_checks);
    return 0;
}
