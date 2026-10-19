#ifndef SSA_ACTOR_PROJECTION_H
#define SSA_ACTOR_PROJECTION_H
#include "ssa/actor_assets.h"
#include "ssa/actor_queue.h"
#include "ssa/world.h"
/* PC presentation-only flag on fixed-screen muzzle snapshots. */
enum { SSA_ACTOR_SCREEN_MIRROR=0x1000 };

/* Borrow stable world bodies and snapshot their render metadata. flags is
 * original entity+30; it is explicit so a native chase-view extension may
 * expose the local body without changing gameplay/collision flags. */
typedef struct SsaActorSource {
    const SsaEntity *entity;
    uint16_t animation,frame,flags;
    uint8_t facing,owner;
} SsaActorSource;
typedef struct SsaActorProjector {
    const SsaActorAssets *assets;
    const SsaMathTables *math;
    const SsaRenderCamera *camera;
    SsaActorQueue *queue;
    uint32_t game_flags;
    unsigned updating;
} SsaActorProjector;
/* Complete D490 billboard and D884 flag2000 fixed-screen projection.
 * Returns1 (including original visibility/capacity rejection), -1 invalid.
 * Projection allocation precedes actor allocation and is not rolled back.
 * Output reserved word remains untouched. Calls native1238 queue insertion.
 * All borrowed storage stays stable; no nested projection/queue update. */
int ssa_actor_project(SsaActorProjector *,const SsaActorSource *,int fixed_screen);
/* PC variant of collect_offsets: honor SCREEN_MIRROR on fixed-screen sources.
 * The original project/collect entry points continue to ignore that flag. */
int ssa_actor_collect_mirrored(SsaActorProjector *,const SsaWorld *,const SsaActorSource *,size_t,const int32_t *z_offsets);
/* PC screen presentation: mirror art optionally, then apply shared offsets; flag2000 only.
 * Coordinates are120x120 raster pixels. Original entry points stay unchanged. */
int ssa_actor_collect_screen(SsaActorProjector *,const SsaWorld *,const SsaActorSource *,size_t,const int32_t *z_offsets,unsigned mirror,int x,int y);
/* D2D0/D338: ascending visited sectors, then original sector-list order;
 * flag10 draws the second resource animation+5*(owner+1). Sources must cover
 * every encountered body exactly once. Unvisited bodies need no source.
 * Resets clip cursor, not projection/actor counts: queue_begin is a separate
 * once-per-frame operation after wall rendering. Earlier effects survive
 * errors. World/map/list ownership cannot change during this call. */
int ssa_actor_collect(SsaActorProjector *,const SsaWorld *,
                       const SsaActorSource *,size_t count);
/* Optional per-source presentation Z offsets; never mutate registered bodies. */
int ssa_actor_collect_offsets(SsaActorProjector *,const SsaWorld *,
                       const SsaActorSource *,size_t count,const int32_t *z_offsets);
#endif
