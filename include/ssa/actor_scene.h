#ifndef SSA_ACTOR_SCENE_H
#define SSA_ACTOR_SCENE_H
#include "ssa/actor_projection.h"
#include "ssa/world_scene.h"

/* D2D0/01828 composition AFTER fresh world geometry. Borrows all dependencies,
 * owns projection/ordering/clipping scratch. Keep addresses stable; zero-init. */
typedef struct SsaActorScene {
    unsigned screen_mirrors; /* PC alternate-hand muzzle presentation;default0. */
    int screen_x,screen_y; /* PC fixed-screen offset in120x120 world-canvas pixels. */
    SsaActorQueue queue;
    SsaActorProjector projector;
    SsaWorldScene *geometry;
    const SsaWorld *world;
    const SsaActorAssets *assets;
    unsigned loaded,updating;
} SsaActorScene;
int ssa_actor_scene_init(SsaActorScene *,SsaWorldScene *,const SsaWorld *,
    const SsaActorAssets *,const SsaMathTables *);
int ssa_actor_scene_free(SsaActorScene *);
/* Map sources to actual stable world bodies. The native collector walks
 * ascending visible sectors and their current original sector-list order.
 * Sources carry original animation/displayed frame/facing/owner/flags. Every
 * visited body needs a source. No caller-owned body or gameplay state changes.
 * This updates the visibility mixed links and draws actual transparent actor
 * pixels onto geometry. Once per fresh geometry draw. Return1/-1; earlier
 * queue/pixel effects survive failure; guard clears. Camera borrowed this call.
 * Local H-body exposure is an explicit render-only flag snapshot extension. */
int ssa_actor_scene_draw(SsaActorScene *,const SsaRenderCamera *,uint32_t game_flags,
    const SsaActorSource *,size_t count);
int ssa_actor_scene_draw_offsets(SsaActorScene *,const SsaRenderCamera *,uint32_t game_flags,
    const SsaActorSource *,size_t count,const int32_t *z_offsets);
#endif
