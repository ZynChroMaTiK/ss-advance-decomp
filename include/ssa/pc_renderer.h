#ifndef SSA_PC_RENDERER_H
#define SSA_PC_RENDERER_H
#include <stddef.h>
#include <stdint.h>
#include "ssa/water.h"
#include "ssa/pc_hardware.h"
struct SsaNativeGame;
struct SsaPcRayHit;
struct SsaPcRayLine;
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
    struct SsaPcRayLine *ray_lines;
    size_t ray_line_capacity;
    unsigned width,height,updating;
    uint64_t rays,wall_pixels,actor_pixels;
    uint64_t godray_triangles; /* Visible light-curtain triangles this draw. */
    struct SsaPcGeometry *geometry;
    uint64_t geometry_builds,sky_builds;
    SsaWaterRender water;
    struct SsaPcRenderer *reflection;
    const SsaPcHardware *hardware; /* Borrowed, survives until free/detach. */
    int hardware_error;
} SsaPcRenderer;
/* Zero-init. Atomic resize: previous buffers remain valid on failure. */
int ssa_pc_renderer_resize(SsaPcRenderer *,unsigned width,unsigned height);
void ssa_pc_renderer_free(SsaPcRenderer *);
/* Read-only gameplay view; never advances simulation or changes its owners. */
int ssa_pc_renderer_draw(SsaPcRenderer *,const struct SsaNativeGame *);
/* Shared second scene pass: mirrored eye, opposite pitch, water-plane clipping,
 * then vertical flip into the visible-water stencil. No HUD/weapon or recursion.
 * GBA output uses240x160 projection sampled into its120x120 indexed scene. */
int ssa_pc_renderer_reflect(SsaPcRenderer **,const struct SsaNativeGame *,
    const struct SsaRenderCamera *,int pitch,SsaWaterRender *,uint32_t *rgb,uint8_t *indexed);
#endif
