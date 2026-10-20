#ifndef SSA_WATER_H
#define SSA_WATER_H
#include "ssa/surface_decals.h"
#include "ssa/map_texture_bindings.h"
#include "ssa/plane_renderer.h"
#include "ssa/movement.h"
typedef struct SsaWaterScene {
    uint8_t glints[256];
    uint8_t ripples[256];
} SsaWaterScene;
typedef struct SsaWaterRender {
    const SsaWaterScene *scene;
    const SsaSurfaceDecals *decals;
    const uint16_t *palette;
    double eye_x,eye_y,eye_z;
    uint32_t frame;
    unsigned animated_decals,reflections;
    /* Visible water stencil: height16, original color8, blend strength8.
     * A zero strength excludes dry pixels, distant water and animated decals. */
    uint32_t *mask;
    size_t capacity;
    unsigned width,height,passes;
    /* RGB555 cache: nearest in the first half, reflection dither alternate
     * in the second. Validity halves follow the same layout. */
    uint8_t blended[65536],valid[8192],reflection_weight[32768];
    uint64_t actor_pixels;
} SsaWaterRender;
int ssa_water_scene_init(SsaWaterScene *,const SsaMapTextureBindings *,const SsaMathTables *,const SsaSkyAssets *,const uint16_t palette[256]);
void ssa_water_scene_free(SsaWaterScene *);
int ssa_water_begin(SsaWaterRender *,const SsaWaterScene *,const SsaSurfaceDecals *,const uint16_t palette[256],double x,double y,double z,uint32_t frame,unsigned glints,unsigned reflections,unsigned width,unsigned height);
void ssa_water_free(SsaWaterRender *);
/* Sample animated decals and record water for a separate mirrored scene pass.
 * No reflection rays or gameplay queries/RNG. */
uint16_t ssa_water_rgb(SsaWaterRender *,uint8_t color,double x,double y,double height,double distance,size_t pixel);
uint8_t ssa_water_index(SsaWaterRender *,uint8_t color,double x,double y,double height,double distance,size_t pixel);
uint16_t ssa_water_blend(const SsaWaterRender *,uint32_t mask,uint16_t reflected);
uint8_t ssa_water_palette_index(SsaWaterRender *,uint16_t rgb);
uint8_t ssa_water_reflection_index(SsaWaterRender *,uint16_t rgb,unsigned x,unsigned y);
int ssa_water_glint(const SsaSurfaceDecals *,double x,double y,double distance,uint32_t frame);
int ssa_water_ripple(const SsaSurfaceDecals *,double x,double y,double distance,uint32_t frame);
int ssa_water_reference(SsaWaterRender *,const SsaPlanePool *,const struct SsaRenderCamera *,uint8_t *,size_t);
#endif
