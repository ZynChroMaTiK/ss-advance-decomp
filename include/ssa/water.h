#ifndef SSA_WATER_H
#define SSA_WATER_H
#include "ssa/surface_decals.h"
#include "ssa/map_texture_bindings.h"
#include "ssa/plane_renderer.h"
#include "ssa/movement.h"
typedef struct SsaWaterScene {
    uint8_t glints[256];
    uint8_t ripples[256]; /* Lighter water hue shared by all decal textures. */
    uint8_t foam[256];
    struct SsaWaterEdges *edges; /* Owned spatial index; borrows stable live map. */
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
    /* One nearest opaque palette entry per RGB555 color; no dither cache. */
    uint8_t blended[32768],valid[4096];
    uint64_t actor_pixels;
} SsaWaterRender;
/* Zero-init; free before reuse and before the borrowed map is freed. */
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
/* Glints:0 clear,1 white core,2 lighter water arms. No offset shadows. */
int ssa_water_glint(const SsaSurfaceDecals *,double x,double y,double distance,uint32_t frame);
/* Non-glint ripples/wavelets:0 clear,2 lighter water,3 base water. */
int ssa_water_ripple(const SsaSurfaceDecals *,double x,double y,double distance,uint32_t frame);
/* Shoreline:0 clear,1 white,2 lighter water,3 base water. Two ROM bitplanes. */
int ssa_water_edge_decal(const SsaWaterScene *,const SsaSurfaceDecals *,uint8_t color,double x,double y,double height,double distance,uint32_t frame);
/* Dry floor/ceiling boundary tiles reuse the same level-owned spatial index. */
int ssa_surface_edge_decal(const SsaWaterScene *,const SsaSurfaceDecals *,unsigned ceiling,
    uint8_t color,double x,double y,double height,double distance);
int ssa_water_reference(SsaWaterRender *,const SsaPlanePool *,const struct SsaRenderCamera *,uint8_t *,size_t);
uint16_t ssa_water_rgb_cached(SsaWaterRender *,SsaSurfaceSampleCache *,uint8_t color,
    double x,double y,double height,double distance,size_t pixel);
#endif
