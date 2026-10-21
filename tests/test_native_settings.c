#include "ssa/native_game.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"native settings line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void aiming(const uint8_t *rom)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaSettings s=SSA_SETTINGS_DEFAULT;
    int32_t slopes[120],markers[120];uint16_t headings[120];unsigned targets[120],mode,i;
    CHECK(g);
    s.vertical_aim=0; /* Explicitly exercise the optional original aiming mode. */
    for(mode=0;mode<2;++mode){
        CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
        s.renderer=mode;CHECK(ssa_native_game_apply_settings(g,&s)==1);
        ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
        ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
        for(i=0;i<120;++i){
            if(i==40){ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);}
            CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,i%2?1:-1,i%2?17:-23)>=0);
            CHECK(ssa_native_game_step(g)==1);
            CHECK(!g->player.pitch && !g->player.free_aim && !g->render_pitch);
            if(!mode){slopes[i]=g->weapon.controller.aim_slope;headings[i]=g->players[0].motion.movement.heading;
                markers[i]=g->weapon.controller.aim.marker;targets[i]=g->weapon.controller.aim.target!=NULL;}
            else {CHECK(slopes[i]==g->weapon.controller.aim_slope);
                CHECK(headings[i]==g->players[0].motion.movement.heading);
                CHECK(markers[i]==g->weapon.controller.aim.marker);
                CHECK(targets[i]==(unsigned)(g->weapon.controller.aim.target!=NULL));}
        }
        s.vertical_aim=1;CHECK(ssa_native_game_apply_settings(g,&s)==1);
        CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,-20)>=0);
        CHECK(ssa_native_game_step(g)==1);
        CHECK(mode?(!g->player.pitch && !g->player.free_aim):(g->player.pitch>0 && g->player.free_aim));
        CHECK(ssa_pc_input_runtime_relative(&g->input_runtime,0,-50)>=0);
        s.vertical_aim=0;CHECK(ssa_native_game_apply_settings(g,&s)==1);
        CHECK(!g->player.pitch && !g->player.free_aim && !g->input.pending_pitch);
        CHECK(ssa_native_game_step(g)==1 && !g->player.pitch && !g->player.free_aim);
        ssa_native_game_free(g);
    }
    free(g);
}
int main(int argc,char **argv)
{
    SsaSettings s=SSA_SETTINGS_DEFAULT;SsaNativeGame *g=calloc(1,sizeof(*g));
    uint8_t *rom=malloc(0x800000);FILE *f;
    CHECK(argc==2 && g && rom);f=fopen(argv[1],"rb");
    CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_apply_settings(g,&s)==-1);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(g->fov_offset==SSA_FOV_DEFAULT_OFFSET);
    {SsaPcInputSample input;
        CHECK(ssa_native_game_apply_settings(g,&s)==1);
        CHECK(g->input.yaw_per_count==64);
        ssa_pc_input_enable(&g->input,1);ssa_pc_input_focus(&g->input,1);
        CHECK(ssa_pc_input_relative(&g->input,10,10)==1);
        CHECK(ssa_pc_input_sample(&g->input,&input)==1 && input.yaw_delta==640 && input.pitch_delta==-640);
        s.mouse_sensitivity=0;
        CHECK(ssa_native_game_apply_settings(g,&s)==1);
        CHECK(ssa_pc_input_relative(&g->input,10,10)==1);
        CHECK(ssa_pc_input_sample(&g->input,&input)==1 && input.yaw_delta==60 && input.pitch_delta==-60);
        s.mouse_sensitivity=SSA_MOUSE_RANGE;
        CHECK(ssa_native_game_apply_settings(g,&s)==1);
        CHECK(ssa_pc_input_relative(&g->input,-10,-10)==1);
        CHECK(ssa_pc_input_sample(&g->input,&input)==1 && input.yaw_delta==-1920 && input.pitch_delta==1920);
        CHECK(ssa_pc_input_sensitivity(&g->input,0)==-1 && g->input.yaw_per_count==192);
        s.mouse_sensitivity=SSA_MOUSE_DEFAULT;
        CHECK(ssa_native_game_apply_settings(g,&s)==1);
    }
    s.fov_offset=60;
    s.alternate_weapons=1;s.hud_style=SSA_HUD_CLASSIC;
    s.sprite_shadows=s.godrays=s.surface_decals=s.water_reflections=s.weapon_bobbing=1;s.renderer=SSA_RENDER_GBA;
    CHECK(ssa_native_game_apply_settings(g,&s)==1);
    CHECK(g->gba_view && g->hud_style==SSA_HUD_CLASSIC && g->alternates.enabled);
    CHECK(g->surface_decals_enabled && g->water_reflections && g->weapon_bobbing && g->godrays_enabled && g->sprite_shadows_enabled);
    CHECK(g->fov_offset==60);
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
    ssa_native_game_free(g);free(g);aiming(rom);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u}\n",checks);return 0;
}
