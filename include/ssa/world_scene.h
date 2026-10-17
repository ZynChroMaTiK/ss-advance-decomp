#ifndef SSA_WORLD_SCENE_H
#define SSA_WORLD_SCENE_H
#include "ssa/world_visibility.h"
#include "ssa/wall_renderer.h"

enum {SSA_SCENE_COVERAGE_LIMIT=124,SSA_SCENE_FRAME_PREFIX=120,
      SSA_SCENE_FRAME_BYTES=14400,SSA_SCENE_OPENING_BYTES=24000};
typedef struct SsaSceneCoverage {
    SsaWallRange ranges[SSA_SCENE_COVERAGE_LIMIT];
    unsigned count;
} SsaSceneCoverage;
/* Original00A84/00E3C secondary coverage used by later actor passes. Reset
 * retains spare storage. Merge includes original compaction's trailing word;
 * stop at the first upper sentinel, not the count, when observing coverage. */
int ssa_scene_coverage_reset(SsaSceneCoverage *);
int ssa_scene_coverage_merge(SsaSceneCoverage *,unsigned left,unsigned right);

typedef struct SsaWorldScene {
    SsaWorldVisibility visibility;
    SsaPlanePool planes;
    SsaPlaneRenderer plane_renderer;
    SsaWallRenderer wall_renderer;
    SsaWallClip columns[120];
    SsaWallRenderResult results[SSA_VISIBILITY_WALL_LIMIT];
    SsaWallRange draw_ranges[SSA_VISIBILITY_RANGE_LIMIT];
    SsaSceneCoverage coverage;
    const SsaSkyAssets *sky;
    uint8_t framebuffer[SSA_SCENE_FRAME_PREFIX+SSA_SCENE_FRAME_BYTES];
    uint8_t openings[SSA_SCENE_OPENING_BYTES];
    unsigned level,updating,loaded;
} SsaWorldScene;

/* Native B6B0 visibility and017CC walls/planes composition. Borrowed map,
 * bindings and all lookup/pixel arrays stay stable until free. The sky buffers
 * retain original sampling windows. Zero-init; 1/-1, atomic init failure. */
int ssa_world_scene_init(SsaWorldScene *,const SsaMapTextureBindings *,
    const SsaMathTables *,const SsaVisibilityTables *,const SsaWallRenderTables *,
    const SsaSkyAssets *,unsigned level);
int ssa_world_scene_free(SsaWorldScene *);
/* Ordinary07C94 clear only (no automap): preserve explicit preceding120bytes. */
int ssa_world_scene_clear(SsaWorldScene *,uint8_t index);
/* Draw world geometry after clear. Resets clipping/plane queues, consumes
 * ordered native visible walls with original portals/mips, then nativeAB10.
 * Heading supplies original localplayer+B8 for sky. No nested draw. 1/-1;
 * earlier pixels and queue effects survive failure. This intentionally ends
 * BEFORE D2D0/01828 actors; it cannot claim a complete game frame or scene. */
int ssa_world_scene_draw(SsaWorldScene *,const SsaRenderCamera *,
    uint8_t game_counter,uint16_t player_heading);
/* World pixels are the14400bytes AFTER the prefix, row-major palette indices.
 * Prefix is explicit owned readable storage for original one-pixel column
 * correction at row0. Init zeroes it; clears/draws never synthesize wrapping. */
const uint8_t *ssa_world_scene_pixels(const SsaWorldScene *);
#endif
