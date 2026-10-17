#ifndef SSA_WORLD_VISIBILITY_H
#define SSA_WORLD_VISIBILITY_H
#include "ssa/render_view.h"
#include "ssa/plane_renderer.h"

enum { SSA_VISIBILITY_WALL_LIMIT=100, SSA_VISIBILITY_RANGE_LIMIT=4096,
       SSA_VISIBILITY_SOLID_LIMIT=122 };
typedef struct SsaVisibilityTables {
    uint32_t reciprocal[65536];
    int32_t tangent[16384];
    uint8_t angle_x[32768],bbox_corners[44];
    unsigned loaded;
} SsaVisibilityTables;
typedef struct SsaVisibilityRange { int16_t left,right; } SsaVisibilityRange;
typedef struct SsaVisibleWall {
    uint16_t previous,next;
    int32_t depth_left,depth_right,u_left,u_right;
    uint8_t left,right;
    uint16_t segment_flags,range_count,range_start,front_sector;
    SsaPlane *ceiling,*floor;
    int32_t distance;
} SsaVisibleWall;
typedef struct SsaSegmentProjection {
    uint16_t left,right,original_left,original_right,angle_left,angle_right;
} SsaSegmentProjection;
typedef struct SsaWorldVisibility {
    const SsaMapAssets *map;
    const SsaMathTables *math;
    const SsaVisibilityTables *tables;
    SsaPlanePool *planes;
    SsaRenderCamera camera;
    uint16_t *vertex_angles,*vertex_valid,*sector_visited,*stack;
    size_t vertex_words,sector_words,stack_capacity;
    SsaVisibleWall walls[SSA_VISIBILITY_WALL_LIMIT];
    SsaVisibilityRange ranges[SSA_VISIBILITY_RANGE_LIMIT];
    SsaVisibilityRange solid[SSA_VISIBILITY_SOLID_LIMIT];
    uint16_t head,tail,wall_count,range_count,solid_count,high_water;
    int16_t front_sector,back_sector,front_ceiling,front_floor,back_ceiling,back_floor;
    SsaPlane *ceiling,*floor;
    uint8_t game_counter;
    unsigned updating;
} SsaWorldVisibility;

/* Lookup bytes only, no execution of ROM instructions. All success returns1,
 * failures-1 unless otherwise specified. Zero initialize workspace before init.
 * Keep map layout, borrowed tables and plane-pool addresses stable until free;
 * live sector heights, side textures and line flags are read each build. */
int ssa_visibility_tables_load(SsaVisibilityTables *,const void *rom,size_t size);
int ssa_world_visibility_init(SsaWorldVisibility *,const SsaMapAssets *,
    const SsaMathTables *,const SsaVisibilityTables *,SsaPlanePool *);
int ssa_world_visibility_free(SsaWorldVisibility *);
/* Original B574/B6B0 through B7B0/B818/BA04 and native A888. This stops at the
 * ordered wall list before017CC wall/plane drawing and D2D0/01828 actor passes.
 * Resets plane pool and clipping each frame; previous prefix effects survive
 * an error. Exhaustion replaces original out-of-bounds access with -1.
 * Only follow head/next: a near-wall depth rejection can consume an unlinked
 * wall slot and clipping ranges. Range endpoints are inclusive. */
int ssa_world_visibility_build(SsaWorldVisibility *,const SsaRenderCamera *,uint8_t game_counter);
/* Isolated original helpers for renderer composition and differential tests.
 * Do not call during build. Box/segment return1visible,0rejected,-1invalid;
 * rejected segment leaves output unchanged, but may cache vertex angles. */
int ssa_visibility_box(SsaWorldVisibility *,const int16_t bounds[4]);
int ssa_visibility_segment(SsaWorldVisibility *,unsigned segment,SsaSegmentProjection *);
/* Original AC8/C10: append visible inclusive spans; solid!=0 also merges
 * horizontal coverage. Returns emitted span count or-1. */
int ssa_visibility_clip(SsaWorldVisibility *,int left,int right,int solid);
int ssa_visibility_reset(SsaWorldVisibility *,const SsaRenderCamera *,uint8_t game_counter);
#endif
