#ifndef SSA_NATIVE_PICKUP_ART_H
#define SSA_NATIVE_PICKUP_ART_H
#include "ssa/actor_assets.h"

/* Requested PC artwork, applied once after decoding and before borrowing assets.
 * Rome palette maps6..11,14..15 replace only the ankh's full/distant image pixels.
 * Dimensions, origins, animation, palette indices and gameplay stay native.
 * Returns1 for a replacement,0 for other maps,-1 for incompatible assets.
 * Validation failure leaves assets unchanged. */
int ssa_native_pickup_art_apply(SsaActorAssets *,unsigned level);
#endif
