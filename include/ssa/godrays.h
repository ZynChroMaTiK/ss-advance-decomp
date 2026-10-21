#ifndef SSA_GODRAYS_H
#define SSA_GODRAYS_H
#include "ssa/map_assets.h"

enum { SSA_GODRAY_STEPS=16, SSA_GODRAY_HEIGHT=96, SSA_GODRAY_GAP=16 };
typedef struct SsaGodrayPalette {
    uint16_t colors[256];
    uint8_t blend[SSA_GODRAY_STEPS+1][256];
    unsigned loaded;
} SsaGodrayPalette;
/* Coordinates/focal lengths24.8, yaw/pitch sine/cosine16.16. Positive yaw.
 * GBA uses focal_y=1.5*focal_x and zero pitch on its120x120 world canvas. */
typedef struct SsaGodrayCamera {
    int32_t x,y,z,sine,cosine,pitch_sine,pitch_cosine,focal_x,focal_y;
    unsigned width,height;
    int32_t clip_z; /* Optional reflection floor, whole world units. */
    unsigned clip;
} SsaGodrayCamera;
typedef struct SsaGodraySpan {
    unsigned x,y,count,line;
    int32_t bottom,top; /* World units, for original wall-opening clipping. */
    int64_t q,vq,dq,dvq; /* Reciprocal depth Q24 and height-weighted reciprocal. */
    int64_t uq,duq; /* World-anchored two-unit texture U, perspective weighted. */
} SsaGodraySpan;
typedef struct SsaGodrayOps {
    void (*span)(void *,const SsaGodraySpan *);
    void *context;
} SsaGodrayOps;
/* Immutable during draw. Call on palette changes, outside renderer workers. */
int ssa_godray_palette(SsaGodrayPalette *,const uint16_t colors[256]);
/* Streaming fixed-point projection/clipping/rasterization; no allocation,
 * floating point, Z buffer, gameplay RNG or far cutoff.
 * Return emitted triangle count, or-1 for invalid input. */
int ssa_godrays_draw(const SsaMapAssets *,const SsaGodrayCamera *,const SsaGodrayOps *);
/* Span sample: depth24.8 and intensity0..16. */
unsigned ssa_godray_sample(int64_t q,int64_t vq,uint32_t *depth);
/* Tileable8x8 checkerboard at two world units/texel. Blends paired gradient
 * steps over twice the former height; stable world phase, no frame/RNG. */
unsigned ssa_godray_dither(int64_t q,int64_t vq,int64_t uq,int bottom,int top);
#endif
