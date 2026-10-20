#ifndef SSA_PC_SCENE_H
#define SSA_PC_SCENE_H
#include "ssa/pc_renderer.h"
/* Shared world/actor renderer and mirrored scene backend. Reads the prepared
 * native view but calls no game lifecycle or HUD composition functions. */
int ssa_pc_scene_draw(SsaPcRenderer *,const struct SsaNativeGame *);
double ssa_pc_scene_fov_scale(unsigned offset);
#endif
