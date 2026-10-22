/* Exercise the actual frontend message handler on a hidden test window.
 * No physical input injection, mouse capture or cursor confinement. */
#define wWinMain ssa_game_fixture_entry
#include "../platform/win32/game.c"
#undef wWinMain
static unsigned wheel_checks;
#define CHECK(x) do{++wheel_checks;if(!(x)){fprintf(stderr,"wheel line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned sync_calls;
static BOOL sync_composed;
static HRESULT sync_result;
static HRESULT WINAPI mock_composition(BOOL *enabled){*enabled=sync_composed;return S_OK;}
static HRESULT WINAPI mock_flush(void){++sync_calls;return sync_result;}
static void vsync_checks(void)
{
    App *a=calloc(1,sizeof(*a));CHECK(a);a->menu=calloc(1,sizeof(*a->menu));CHECK(a->menu);
    a->dwm_composition=mock_composition;a->dwm_flush=mock_flush;sync_composed=TRUE;sync_result=S_OK;
    presentation_sync_refresh(a);CHECK(a->vsync_available && a->menu->vsync_available);
    presentation_sync_wait(a);CHECK(!sync_calls); /* Off preserves existing pacing. */
    a->settings.vsync=1;
    for(unsigned renderer=0;renderer<SSA_RENDERERS;++renderer){a->settings.renderer=renderer;
        presentation_sync_wait(a);CHECK(sync_calls==renderer+1);}
    sync_composed=FALSE;presentation_sync_refresh(a);presentation_sync_wait(a);
    CHECK(sync_calls==3 && !a->vsync_available && !a->menu->vsync_available && a->settings.vsync);
    sync_composed=TRUE;presentation_sync_refresh(a);sync_result=E_FAIL;presentation_sync_wait(a);
    CHECK(sync_calls==4 && !a->vsync_available && !a->menu->vsync_available && a->settings.vsync);
    presentation_sync_wait(a);CHECK(sync_calls==4);
    sync_result=S_OK;presentation_sync_refresh(a);presentation_sync_wait(a);CHECK(sync_calls==5);
    a->dwm_flush=NULL;presentation_sync_refresh(a);CHECK(!a->vsync_available);
    /* Exercise the actual optional Windows API without displaying a window. */
    presentation_sync_init(a);
    if(a->vsync_available){presentation_sync_wait(a);CHECK(a->vsync_available);}
    if(a->dwm)FreeLibrary(a->dwm);
    free(a->menu);free(a);
}
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
static void selection_priority(const SsaInventoryRules *rules)
{
    static const unsigned order[10]={1,2,3,4,5,6,7,9,10,8};
    unsigned mask,k,j,d;
    for(mask=1;mask<1024;++mask)for(k=0;k<10;++k){
        SsaInventory inv={0};SsaWeaponControl c={0};unsigned expected=1;
        inv.weapons=(uint16_t)mask;
        for(j=0;j<10;++j)inv.ammo[j]=20;
        for(d=0;d<2;++d){int direction=d?-1:1;
            c.requested=(uint16_t)order[k];
            for(j=1;j<=10;++j){expected=order[(k+(d?10-j:j))%10];
                if(mask&(1u<<(expected-1)))break;}
            CHECK(ssa_weapon_cycle_pc(&inv,&c,direction)==(int)expected);
        }
        /* A newly acquired gun outranks only lower-priority owned guns. */
        c.current=c.requested=1;c.flags=0;c.keys=0;
        inv.pending_count=1;inv.pending_weapons[0]=(uint8_t)order[k];
        expected=order[k];
        for(j=k;j<10;++j)if(mask&(1u<<(order[j]-1))){expected=1;break;}
        CHECK(ssa_weapon_update_pc(&inv,&c,rules,1)==0 && c.requested==expected);
    }
    {SsaInventory inv={0};SsaWeaponControl c={0};
        inv.weapons=(1u<<7)|(1u<<9);inv.ammo[9]=20;c.current=c.requested=8;
        CHECK(ssa_weapon_update_pc(&inv,&c,rules,1)==0 && c.requested==10);
        /* The original ROM API keeps its original priority. */
        inv.pending_count=1;inv.pending_weapons[0]=8;inv.weapons=1u<<9;c.current=c.requested=10;c.flags=0;
        CHECK(ssa_weapon_update(&inv,&c,rules,1)==0 && c.requested==10);
    }
}
int main(int argc,char **argv)
{
    vsync_checks();
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
    CHECK(!a.reload && !a.advance); /* R must never restart the level. */
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
    for(i=0;i<70;++i)CHECK(ssa_native_game_tick(a.game)==1);
    a.game->alternates.rounds[0]=2;
    SendMessageW(a.window,WM_KEYDOWN,'R',0);SendMessageW(a.window,WM_KEYUP,'R',0);
    CHECK(ssa_native_game_tick(a.game)==1);
    CHECK(a.game->alternates.reload_phase==2 && !a.reload && !a.advance);
    for(i=0;i<70;++i)CHECK(ssa_native_game_tick(a.game)==1);
    CHECK(!a.game->alternates.rounds[0] && !a.game->alternates.reload_phase);
    CHECK(ssa_native_game_alternate_weapons(a.game,0,0)==1);
    selection_priority(&a.game->inventory_rules);
    CHECK(a.game->weapon.pc_weapon_order==1);
    a.game->players[0].inventory.weapons=0x3ff;
    for(i=0;i<10;++i)a.game->players[0].inventory.ammo[i]=20;
    {static const unsigned order[10]={1,2,3,4,5,6,7,9,10,8};
        a.game->gameplay.weapon.requested=1;
        for(i=1;i<=10;++i){wheel(a.window,-120);requested(&a,order[i%10]);}
        for(i=10;i>0;--i){wheel(a.window,120);requested(&a,order[i-1]);}
        for(i=7;i<10;++i){unsigned key=i==9?'0':'1'+i;
            SendMessageW(a.window,WM_KEYDOWN,key,0);SendMessageW(a.window,WM_KEYUP,key,0);
            requested(&a,order[i]);}
    }
    a.game->players[0].inventory.ammo[7]=0;
    wheel(a.window,120);requested(&a,10); /* Empty UMT falls back to cannon. */
    a.game->gameplay.weapon.requested=8;
    wheel(a.window,-120);requested(&a,1);
    /* Native runtime uses the same priority when the last UMT round is gone. */
    a.game->players[0].inventory.ammo[7]=20;
    CHECK(ssa_native_game_select_weapon(a.game,8)==1);finish_switch(&a,8);
    a.game->players[0].inventory.ammo[7]=0;
    for(i=0;i<120 && a.game->gameplay.weapon.requested==8;++i)CHECK(ssa_native_game_tick(a.game)==1);
    requested(&a,10);
    a.captured=0;DestroyWindow(a.window);CHECK(UnregisterClassW(wc.lpszClassName,wc.hInstance));
    ssa_native_game_free(a.game);free(a.game);free(a.rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"physical_input\":false,\"real_window_messages\":true}\n",wheel_checks);
    return 0;
}
