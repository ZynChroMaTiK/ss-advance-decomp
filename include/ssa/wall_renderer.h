#ifndef SSA_WALL_RENDERER_H
#define SSA_WALL_RENDERER_H
#include "ssa/map_texture_bindings.h"
#include "ssa/plane_renderer.h"

typedef struct SsaWallRenderTables {
    uint32_t reciprocal[65536]; /* ROM08034934 */
    uint8_t mip_near[16],mip_far[8]; /* ROM0844AEDC / 0844AEEC */
    uint8_t closed_top[120],closed_bottom[120];
    unsigned loaded;
} SsaWallRenderTables;
typedef struct SsaWallClip { int16_t top,bottom; } SsaWallClip;
typedef struct SsaWallRange { int16_t left,right; } SsaWallRange;
typedef struct SsaWallRenderCommand {
    int32_t depth_left,depth_right,u_left,u_right;
    uint8_t left,right;
    uint16_t segment;
    const SsaWallRange *ranges;
    size_t range_count;
    SsaPlane *ceiling,*floor;
} SsaWallRenderCommand;
typedef struct SsaWallRenderResult {
    uint32_t sprite_clip_flags;
    int16_t bottom_height,top_height;
    const uint8_t *top_clip,*bottom_clip;
    SsaPlane *ceiling,*floor;
    int16_t top_clip_x,bottom_clip_x; /* Pointer element0's screen column. */
} SsaWallRenderResult;
typedef struct SsaWallRenderer {
    const SsaMapTextureBindings *bindings;
    const SsaWallRenderTables *tables;
    SsaPlanePool *planes;
    SsaWallClip *columns; /* Exactly120 entries; initial {-1,120}. */
    uint8_t *framebuffer;
    size_t capacity;
    int32_t eye_z; /* Original camera+08, 16.16. */
    unsigned updating;
    uint8_t *openings;
    size_t opening_capacity; /* Cursor is planes->opening_cursor. */
    size_t framebuffer_offset; /* World pixel(0,0) within framebuffer storage.
        * Reserve120 prefix bytes to preserve original y0/count1 predecessor
        * reads. Caller initializes them; this routine never guesses values. */
} SsaWallRenderer;

/* Copies only data. Zero-init output; return1/-1, unchanged on failure. */
int ssa_wall_render_tables_load(SsaWallRenderTables *,const void *rom,size_t size);
/* Complete original BE60 one-sided wall path, including native perspective
 * interpolation, mip selection, plane writes/checks and ARM-derived columns.
 * Lineflag4 portals return -2 BEFORE effects; malformed/unsupported inputs -1.
 * Success0. Pixel/plane prefix effects remain on later failure. Bindings,
 * allocations, map geometry/heights, camera and textures stay stable in a pass.
 * Result clip arrays borrow tables. Command is unchanged; result receives the
 * original mutable record/globals' host projection. No nested renderer calls.
 * Framebuffer/opening buffers are disjoint from metadata/map/texture storage.
 */
int ssa_wall_render_solid(SsaWallRenderer *,const SsaWallRenderCommand *,SsaWallRenderResult *);
/* Complete one-/two-sided BE60 paths. out must be initialized: unused clip
 * height fields retain their incoming values, as in the original record.
 * Portal clip pointers use explicit screen origins instead of pointers before
 * native allocations. Original clip-byte copies exclude command.right.
 * Return0/-1; effects before an error remain. No unsupported portal stub. */
int ssa_wall_render(SsaWallRenderer *,const SsaWallRenderCommand *,SsaWallRenderResult *);
#endif
