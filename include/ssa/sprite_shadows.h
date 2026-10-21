#ifndef SSA_SPRITE_SHADOWS_H
#define SSA_SPRITE_SHADOWS_H
#include "ssa/actor_scene.h"
#include "ssa/surface_decals.h"
enum {SSA_SHADOW_LIMIT=100,SSA_SHADOW_WIDTH=32,SSA_SHADOW_HEIGHT=8};
typedef struct SsaSpriteShadow {
    int32_t x,y;int16_t floor,sector;unsigned radius,strength,source;
    /* Column-major indexed billboard, transparent zero. Stable until draw ends. */
    uint8_t pixels[SSA_SHADOW_WIDTH*SSA_SHADOW_HEIGHT];
} SsaSpriteShadow;
typedef struct SsaSpriteShadows {
    SsaSpriteShadow sprites[SSA_SHADOW_LIMIT];int indices[SSA_SHADOW_LIMIT];unsigned count;
} SsaSpriteShadows;
void ssa_sprite_shadow_palette(uint8_t colors[4][256],const uint16_t palette[256]);
/* No gameplay entities or original actor queue slots are allocated. */
int ssa_sprite_shadows_prepare(SsaSpriteShadows *,const SsaMapAssets *,const SsaActorAssets *,
    const SsaActorSource *,unsigned count,const SsaEntity *player,const SsaSurfaceDecals *,const uint8_t *colors);
int ssa_sprite_shadows_reference(SsaSpriteShadows *,SsaActorScene *,const SsaRenderCamera *,uint32_t flags,
    const SsaActorSource *,unsigned count,const int32_t *z_offsets);
#endif
