#ifndef SSA_SPRITE_OAM_H
#define SSA_SPRITE_OAM_H
#include "ssa/sprite_assets.h"
#include "ssa/sprite_dispatch.h"
#include "ssa/movement.h"

typedef struct SsaSpriteCacheEntry {
    uint8_t resident,pending;
    uint16_t tile; /* Original64-byte tile units, not OAM32-byte units. */
} SsaSpriteCacheEntry;
typedef struct SsaSpriteOamEntry {
    uint16_t attribute0,attribute1,attribute2,affine;
} SsaSpriteOamEntry;
typedef struct SsaSpriteOam {
    SsaSpriteOamEntry entries[128];
    uint32_t layer_heads[17];
    uint8_t layer_bucket[16]; /* Actual0844BBEC table, supported layers0..15. */
    uint16_t count,affine_count;
    uint16_t local_index,mosaic,mosaic_register;
    uint32_t counter; /*030055B0: mask0x4 gates blinking objectflag0x2. */
    uint8_t flags,transfer_flag; /*03005614/03005618*/
    unsigned loaded,updating;
} SsaSpriteOam;

/* Zero-initialize before first init. Copies the16 original layer buckets.
 * No ownership or retained ROM pointer.
 * Return1/-1. Init outside updates; failure leaves destination unchanged. */
int ssa_sprite_oam_init(SsaSpriteOam *,const void *rom,size_t size);
/* WholeFCE4 including E648 reset and FC3C/FC58 list sorting. Uses previous_frame,
 * not current frame. Builds raw8-byte OAM entries, preserving matrix slots even
 * when later object entries occupy them. Does not draw pixels or load tiles.
 * Borrow stable objects/assets/cache/math for the call. Active slots must be
 * unique and valid; layers0..15, original84objects/128pieces/32matrix limits.
 * object.link becomes a native logical list reference:0=end,1..17=layer node,
 * 80000000|(slot+1)=object. Never a host/GBA pointer. Original links compare via
 * this normalization in tests. screen_x/y update only for visited objects.
 * Return1/-1. Invalid input is rejected before state/object mutation. No reentry
 * or aliasing of inputs with output; sprite dispatch must be outside an update.
 */
int ssa_sprite_oam_build(SsaSpriteOam *,SsaSpriteDispatch *,const SsaSpriteAssets *,
    const SsaSpriteCacheEntry cache[SSA_SPRITE_IMAGE_COUNT],const SsaMathTables *);
#endif
