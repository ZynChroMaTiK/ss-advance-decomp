#include "ssa/native_game.h"
#include "ssa/settings.h"
#include "ssa/pc_hardware.h"
int ssa_native_game_apply_settings(SsaNativeGame *g,const SsaSettings *s)
{
    if(!g || !g->loaded || g->game.updating || g->error_line || !ssa_settings_valid(s))return -1;
    if(ssa_pc_input_sensitivity(&g->input,
        (int32_t)((64*(s->mouse_sensitivity+1)+5)/10))!=1)return -1;
    g->gba_view=s->renderer==SSA_RENDER_GBA;
    g->vertical_aim=s->vertical_aim;
    g->fov_offset=s->fov_offset;
    g->mipmaps=g->gba_view || s->mipmaps;
    g->vulkan_features=s->renderer==SSA_RENDER_VULKAN?
        (s->vulkan_antialiasing?SSA_PC_AA:0)|(s->vulkan_anisotropic?SSA_PC_AF:0)|
        (s->vulkan_ambient_occlusion?SSA_PC_AO:0)|(s->vulkan_motion_blur?SSA_PC_MOTION:0):0;
    if(g->gba_view || !g->vertical_aim){
        g->player.pitch=0;g->player.free_aim=0;g->input.pending_pitch=0;
    }
    g->weapon_bobbing=s->weapon_bobbing;
    g->surface_decals_enabled=s->surface_decals;
    g->godrays_enabled=s->godrays;
    g->sprite_shadows_enabled=s->sprite_shadows;
    g->water_reflections=s->water_reflections;
    if(ssa_native_game_hud_style(g,s->hud_style)!=1)return -1;
    /* Volume/display changes must not reset an in-progress weapon action. */
    if(g->alternates.enabled!=s->alternate_weapons &&
       ssa_native_game_alternate_weapons(g,s->alternate_weapons,0)!=1)return -1;
    if(!s->automap_enabled && (g->players[0].motion.movement.flags&8) &&
       ssa_native_game_automap(g,0)<0)return -1;
    return 1;
}
