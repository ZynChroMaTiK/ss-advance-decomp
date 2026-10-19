#include "ssa/native_game.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"native settings line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(int argc,char **argv)
{
    SsaSettings s=SSA_SETTINGS_DEFAULT;SsaNativeGame *g=calloc(1,sizeof(*g));
    uint8_t *rom=malloc(0x800000);FILE *f;
    CHECK(argc==2 && g && rom);f=fopen(argv[1],"rb");
    CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_apply_settings(g,&s)==-1);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    s.alternate_weapons=1;s.hud_style=SSA_HUD_CLASSIC;
    s.surface_decals=s.water_reflections=s.weapon_bobbing=1;s.renderer=SSA_RENDER_GBA;
    CHECK(ssa_native_game_apply_settings(g,&s)==1);
    CHECK(g->gba_view && g->hud_style==SSA_HUD_CLASSIC && g->alternates.enabled);
    CHECK(g->surface_decals_enabled && g->water_reflections && g->weapon_bobbing);
    g->weapon.fire_inhibited=g->weapon.switch_blocked=g->weapon.colt_pair_pending=1;
    g->alternates.rounds[0]=3;g->alternates.rounds[1]=4;
    s.music_volume=10;s.sound_volume=20;
    CHECK(ssa_native_game_apply_settings(g,&s)==1);
    CHECK(g->weapon.fire_inhibited && g->weapon.switch_blocked && g->weapon.colt_pair_pending);
    CHECK(g->alternates.rounds[0]==3 && g->alternates.rounds[1]==4);
    s.renderer=SSA_RENDER_SOFTWARE;s.surface_decals=2;
    CHECK(ssa_native_game_apply_settings(g,&s)==-1 && g->gba_view);
    s.surface_decals=0;g->game.updating=1;
    CHECK(ssa_native_game_apply_settings(g,&s)==-1 && g->gba_view);g->game.updating=0;
    s.alternate_weapons=0;s.hud_style=SSA_HUD_ORIGINAL;
    CHECK(ssa_native_game_apply_settings(g,&s)==1 && !g->alternates.enabled && !g->gba_view);
    CHECK(ssa_native_game_automap(g,1)==1);
    CHECK(ssa_native_game_apply_settings(g,&s)==1 && !(g->players[0].motion.movement.flags&8));
    ssa_native_game_free(g);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u}\n",checks);return 0;
}
