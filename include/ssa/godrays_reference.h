#ifndef SSA_GODRAYS_REFERENCE_H
#define SSA_GODRAYS_REFERENCE_H
#include "ssa/godrays.h"
#include "ssa/actor_scene.h"
typedef struct SsaGodrayReference {
    SsaGodrayPalette palette;
    uint16_t actor_depth[120*120]; /* Four world units/step;0 fixed-screen art. */
    unsigned triangles;
} SsaGodrayReference;
/* Call after original world+actor drawing, before display/HUD composition.
 * Borrows completed wall openings and opaque sprite coverage. No second scene
 * render or floating point. Workspace lives in EWRAM on a GBA target. Prepare
 * palette at level load; indices then follow live fades without rebuilding LUTs.
 * The colors argument initializes an unprepared workspace only. */
int ssa_godrays_reference(SsaGodrayReference *,const SsaActorScene *,const SsaRenderCamera *,const uint16_t colors[256]);
#endif
