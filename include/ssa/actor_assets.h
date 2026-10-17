#ifndef SSA_ACTOR_ASSETS_H
#define SSA_ACTOR_ASSETS_H
#include <stddef.h>
#include <stdint.h>

enum { SSA_ACTOR_RESOURCE_COUNT=142 };
typedef struct SsaActorImage {
    uint32_t rom_offset;
    int16_t origin_x,origin_y;
    uint16_t width,height;
    uint8_t *pixels; /* Owned column-major palette indices; zero transparent. */
} SsaActorImage;
typedef struct SsaActorResource {
    uint32_t table_offset,animation_offset;
    uint16_t count,directions,mip_flags;
    uint8_t frames; /* Original0890: count/directions/popcount(flags), byte. */
    SsaActorImage *images; /* count entries; original image1 is images[0]. */
} SsaActorResource;
typedef struct SsaActorAssets {
    SsaActorResource resources[SSA_ACTOR_RESOURCE_COUNT];
    uint32_t reciprocal[65536]; /* ROM08034934; used by09258. */
    uint8_t closed_top[121],closed_bottom[121]; /* 1238 inclusive-right tails. */
    unsigned loaded;
} SsaActorAssets;

/* Zero-initialize before first load. Atomic on failure; replaces an earlier
 * successful load on success. Copies all data, retains no ROM pointers.
 * Caller keeps assets stable while borrowed; no concurrent load/free/draw.
 * Metadata/payload bounds, nonzero RLE packet counts and exact column heights
 * are checked. Limits3000 images/resource,1024 dimensions,64MiB decoded data.
 * Resource IDs0..3 contain no image table in the actual game. */
int ssa_actor_assets_load(SsaActorAssets *,const void *rom,size_t rom_size);
void ssa_actor_assets_free(SsaActorAssets *);
const SsaActorImage *ssa_actor_assets_image(const SsaActorAssets *,unsigned resource,
                                          uint32_t one_based_image);
/* Row-major preview/export. Capacity/stride failure leaves output unchanged. */
int ssa_actor_image_indices(const SsaActorImage *,uint8_t *,size_t stride,size_t capacity);
#endif
