#ifndef SSA_PC_RENDERER_H
#define SSA_PC_RENDERER_H
#include <stddef.h>
#include <stdint.h>
struct SsaNativeGame;
struct SsaPcRayHit;
struct SsaPcGeometry;
/* PC extension, separate from the bit-exact GBA renderer. Dynamic world rays,
 * per-pixel depth and original highest-resolution textures/actor assets.
 * HUD remains original pixel art. No preset resolution ceiling: integer/span
 * validation and allocation failure bound the available sizes. Stable owner. */
typedef struct SsaPcRenderer {
    uint32_t *pixels;
    float *depth;
    struct SsaPcRayHit *hits;
    size_t hit_capacity;
    unsigned width,height,updating;
    uint64_t rays,wall_pixels,actor_pixels;
    struct SsaPcGeometry *geometry;
    uint64_t geometry_builds,sky_builds;
} SsaPcRenderer;
/* Zero-init. Atomic resize: previous buffers remain valid on failure. */
int ssa_pc_renderer_resize(SsaPcRenderer *,unsigned width,unsigned height);
void ssa_pc_renderer_free(SsaPcRenderer *);
/* Read-only gameplay view; never advances simulation or changes its owners. */
int ssa_pc_renderer_draw(SsaPcRenderer *,const struct SsaNativeGame *);
#endif
