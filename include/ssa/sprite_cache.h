#ifndef SSA_SPRITE_CACHE_H
#define SSA_SPRITE_CACHE_H
#include "ssa/sprite_oam.h"

enum { SSA_SPRITE_CACHE_BLOCKS=50, SSA_SPRITE_CACHE_REQUESTS=50,
       SSA_SPRITE_CACHE_TRANSFERS=10, SSA_SPRITE_OBJ_BYTES=32768 };
typedef struct SsaSpriteTileBlock { uint16_t start,count; } SsaSpriteTileBlock;
typedef struct SsaSpriteTransfer { uint16_t image,destination,count,reserved; } SsaSpriteTransfer;
typedef struct SsaSpriteCache {
    SsaSpriteCacheEntry entries[SSA_SPRITE_IMAGE_COUNT];
    /* Extra records preserve original stale reads during shift/merge loops. */
    SsaSpriteTileBlock free_blocks[SSA_SPRITE_CACHE_BLOCKS+2];
    SsaSpriteTileBlock used_blocks[SSA_SPRITE_CACHE_BLOCKS+2];
    uint16_t used_images[SSA_SPRITE_CACHE_BLOCKS+2];
    uint16_t requests[SSA_SPRITE_CACHE_REQUESTS];
    SsaSpriteTransfer transfers[SSA_SPRITE_CACHE_TRANSFERS];
    uint8_t tiles[SSA_SPRITE_OBJ_BYTES];
    uint16_t free_count,used_count,image_count,read_cursor,write_cursor;
    uint16_t allocation_failures,transfer_count;
    uint32_t budget;
    unsigned loaded,updating;
} SsaSpriteCache;

/* Native D9F8 defaults: free tile64 range {160,512}, 50-entry request ring,
 * empty cache and transfer list. Owns all scratch and physical OBJ tile bytes.
 * Zero-init; success1/-1. No original ROM instructions execute. */
int ssa_sprite_cache_init(SsaSpriteCache *);
int ssa_sprite_cache_free(SsaSpriteCache *);
/* DAF0 display-player handoff/reset: retain physical tile bytes and stale
 * block/request tails, clear residency and queue cursors, restore free range and reset every
 * active object's previous_frame to0. Use after constructing the first view.
 * This supplies a valid retained-frame baseline for newly allocated sprites. */
int ssa_sprite_cache_reset(SsaSpriteCache *,SsaSpriteDispatch *);
/* DBF4: original owner/type filters, frame culling and flag4 update. Uses
 * original unscaled signed16 bounds, including strict >240/>160 edges.
 * Return1/-1; validates all visited frames before mutating any object. */
int ssa_sprite_visibility(SsaSpriteDispatch *,const SsaSpriteAssets *,unsigned local_index);
/* Complete DBDC: visibility, DDA8 requests/weapon transfer, DEF8 aging/release,
 * DF80 eight-request/10000-byte budget and E0A8/E2EC allocation. Uses original
 * previous_frame retention, ring overwrite and removal-skips-next semantics.
 * Native safety: invalid previous-frame sentinel3FFF retains no image; the
 * original probes beyond its cache allocation. Other out-of-range IDs fail.
 * Appends real transfers; flush separately before displaying the new OAM.
 * Return1/-1, guarded reentry; earlier effects survive late failure. */
int ssa_sprite_cache_prepare(SsaSpriteCache *,SsaSpriteDispatch *,
    const SsaSpriteAssets *,unsigned local_index);
/* E358 normal AGB! transfers, max160 tiles per descriptor. Copies owned assets
 * into physical OBJ bytes, preserving VRAM's32KiB mirror. Consumes prefix;
 * copied pixels remain after failure. No BIOS/DMA/emulation at runtime. */
int ssa_sprite_cache_flush(SsaSpriteCache *,const SsaSpriteAssets *);
/* Original allocator/release exposed for differential proof. Return tile64 or
 * -1 on exhaustion/invalid; release1/-1 (unknown start is a successful no-op).
 * Stale records beyond counts persist, as in original. Not during prepare. */
int ssa_sprite_cache_allocate(SsaSpriteCache *,uint16_t tiles);
int ssa_sprite_cache_release(SsaSpriteCache *,uint16_t tile);
#endif
