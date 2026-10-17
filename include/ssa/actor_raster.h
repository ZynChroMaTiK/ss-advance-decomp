#ifndef SSA_ACTOR_RASTER_H
#define SSA_ACTOR_RASTER_H
#include "ssa/actor_assets.h"
#include "ssa/actor_queue.h"

/* Whole09258 plus ARM0638 (IWRAM03000110), native palette-index output.
 * Clip words are signed {top,bottom} pairs starting at the first clipped
 * column, with exclusive bottom. No hidden global framebuffer or scratch.
 * Zero source indices preserve destination. Mirrors and12-bit fractions use
 * original wrapped arithmetic, including separate vertical clipping floors.
 * Original pointer-index shift discards bit30 as well as mirror bit31.
 * Returns1 on success (also empty draw), -1 before any framebuffer change for
 * malformed projection/storage/source sample. Inputs must not alias output.
 * This bounded native API rejects original out-of-bounds/zero-count loops.
 */
int ssa_actor_raster_draw(const SsaActorAssets *,const SsaActorProjection *,
    const int16_t *clip_words,size_t clip_word_count,
    uint8_t *framebuffer,size_t framebuffer_size);
#endif
