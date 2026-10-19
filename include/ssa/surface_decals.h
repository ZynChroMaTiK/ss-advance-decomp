#ifndef SSA_SURFACE_DECALS_H
#define SSA_SURFACE_DECALS_H
#include <stddef.h>
#include <stdint.h>
/* Optional native art, not recovered ROM content. Seamless128x128 stain masks
 * sample at2 world units/texel, matching wall density. No gameplay state. */
enum { SSA_SURFACE_STAIN_TILES=48, SSA_SURFACE_DECAL_TILES=49,
    SSA_SURFACE_DECAL_VARIATIONS=4,
    SSA_SURFACE_STAIN_SIZE=128, SSA_SURFACE_STAIN_WORLD=256,
    SSA_SURFACE_STAIN_FULL=47, SSA_SURFACE_NOISE_TILE=48,
    SSA_SURFACE_STAIN_GRID=64, SSA_SURFACE_STAIN_NORTH=1,
    SSA_SURFACE_STAIN_EAST=2, SSA_SURFACE_STAIN_SOUTH=4, SSA_SURFACE_STAIN_WEST=8,
    SSA_SURFACE_STAIN_NW=1, SSA_SURFACE_STAIN_NE=2,
    SSA_SURFACE_STAIN_SE=4, SSA_SURFACE_STAIN_SW=8 };
typedef struct SsaSurfaceDecalAtlas {
    /*0 empty,1 single,47 full,48 faint noise. Stored texels are0 or255. */
    uint8_t masks[2][SSA_SURFACE_DECAL_VARIATIONS][SSA_SURFACE_DECAL_TILES][SSA_SURFACE_STAIN_SIZE*SSA_SURFACE_STAIN_SIZE];
    uint16_t thresholds[2][SSA_SURFACE_DECAL_VARIATIONS][SSA_SURFACE_STAIN_SIZE*SSA_SURFACE_STAIN_SIZE];
} SsaSurfaceDecalAtlas;
typedef struct SsaSurfaceDecals {
    const SsaSurfaceDecalAtlas *atlas; /* Immutable process-owned storage; never free. */
    uint8_t shades[256],connections[SSA_SURFACE_DECAL_TILES],corners[SSA_SURFACE_DECAL_TILES];
    uint8_t water_floor[256]; /* Classified from the original, unfaded palette. */
    uint8_t tiles[2][SSA_SURFACE_STAIN_GRID*SSA_SURFACE_STAIN_GRID];
    uint8_t variations[2][SSA_SURFACE_STAIN_GRID*SSA_SURFACE_STAIN_GRID];
    unsigned loaded,seed,level;
} SsaSurfaceDecals;
/* Slave Compound's pale pool shares its color with two dry stair treads.
 * Height distinguishes the large pool without broadening the blue heuristic. */
static inline int ssa_surface_decals_water(const SsaSurfaceDecals *d,unsigned color,double height)
{return d && color<256 && (d->water_floor[color] || (d->level==3 && color==120 && height==96));}
struct SsaPlanePool;
struct SsaRenderCamera;
int ssa_surface_decals_init(SsaSurfaceDecals *,const uint16_t palette[256],unsigned level);
/* Direct stored tile texel (0 or255), before distance fading. Texels are two
 * world units; no coordinate warping or generated coverage. Invalid returns0. */
uint8_t ssa_surface_decals_texel(const SsaSurfaceDecals *,unsigned ceiling,int32_t texel_x,int32_t texel_y);
uint8_t ssa_surface_decals_sample(const SsaSurfaceDecals *,unsigned ceiling,uint8_t color,
    double world_x,double world_y,double distance);
/* Reference renderer overlay: use original plane visibility intervals before
 * drawing actors; sky is excluded and wall/plane geometry remains unchanged. */
int ssa_surface_decals_reference(const SsaSurfaceDecals *,const struct SsaPlanePool *,
    const struct SsaRenderCamera *,uint8_t *framebuffer,size_t capacity);
#endif
