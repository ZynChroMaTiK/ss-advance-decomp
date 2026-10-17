#ifndef SSA_PLANE_RENDERER_H
#define SSA_PLANE_RENDERER_H
#include <stddef.h>
#include <stdint.h>

enum {SSA_PLANE_WIDTH=120,SSA_PLANE_HEIGHT=120,SSA_PLANE_LIMIT=80,
      SSA_SKY_ROW_BYTES=768,SSA_SKY_SAMPLE_BYTES=768*120};
/* Exact original252-byte record. Column x is top[x+1]/bottom[x+1]; indices
 * zero and121 represent the left/right sentinel columns -1 and120. */
typedef struct SsaPlane {
    int16_t height,min_x,max_x;
    uint16_t color;
    uint8_t top[122],bottom[122];
} SsaPlane;
typedef struct SsaPlanePool {
    SsaPlane records[SSA_PLANE_LIMIT];
    unsigned count,capacity;
    size_t opening_cursor; /* A870 resets WORLD+340 to opening-buffer start. */
} SsaPlanePool;
typedef struct SsaSkyAssets {
    /* Original A98C's complete readable sampling windows. They overlap in
     * ROM; these are not a claim of two independent120-row sky images. */
    uint8_t pixels[2][SSA_SKY_SAMPLE_BYTES];
    unsigned loaded;
} SsaSkyAssets;
typedef struct SsaPlaneRenderer {
    int32_t row_starts[SSA_PLANE_HEIGHT];
    uint32_t sky_offset,height_distance;
    uint8_t color;
    unsigned updating;
} SsaPlaneRenderer;

/* A844 capacity80 onlevel2,64otherwise. Init zeroes native storage; original
 * allocator contents were unspecified. Stable addresses required for handles. */
int ssa_plane_pool_init(SsaPlanePool *,unsigned level);
int ssa_plane_pool_reset(SsaPlanePool *); /* A870: count/cursor only. */
SsaPlane *ssa_plane_find(SsaPlanePool *,int32_t height,unsigned color); /* A888 */
SsaPlane *ssa_plane_check(SsaPlanePool *,SsaPlane *,int32_t left,int32_t right); /* A900 */
/* Color truncates tobyte; height truncates tosigned16. Find/check initialize
 * only original fields/top[1..120], retain sentinels and bottom bytes. Exhausted
 * pool or invalidhandle/range returnsNULL without changes. Validrange0..119. */
int ssa_sky_assets_load(SsaSkyAssets *,const void *rom,size_t size);
/* Standalone original A98C. Row>119 succeeds without writing; negative row,
 * invalid spans/source state fail. Color0 copies sky, allothercolors fill.
 * Explicit native errors replace original invalid-memory access/zero loops. */
int ssa_plane_span(SsaPlaneRenderer *,const SsaSkyAssets *,unsigned level,
    uint8_t *framebuffer,size_t capacity,int32_t row,int32_t left,int32_t right);
/* Original AA4C transition; caller supplies two consecutive columns' inclusive
 * vertical intervals. Empty columns have top255 and bottom0..119. */
int ssa_plane_edges(SsaPlaneRenderer *,const SsaSkyAssets *,unsigned level,
    uint8_t *framebuffer,size_t capacity,int32_t x,
    unsigned old_top,unsigned old_bottom,unsigned new_top,unsigned new_bottom);
/* Complete AB10 through nativeAA4C/A98C/raster_fill. Heading is localplayer+B8,
 * eye_z signedcamera+0A. Mutates plane sentinels,rowstarts and renderer globals.
 * On error previous effects remain, updating guard is cleared. Native pointers
 * and storage must remain stable; framebuffer must not alias context/assets. */
int ssa_plane_render(SsaPlaneRenderer *,SsaPlanePool *,const SsaSkyAssets *,
    unsigned level,uint16_t heading,int16_t eye_z,uint8_t *framebuffer,size_t capacity);
#endif
