#ifndef SSA_TEXTURE_ASSETS_H
#define SSA_TEXTURE_ASSETS_H
#include <stddef.h>
#include <stdint.h>

enum { SSA_TEXTURE_FAMILIES=2, SSA_TEXTURE_MIPS=3, SSA_TEXTURE_MISSING=65535 };
typedef struct SsaTextureImage {
    char name[8];                 /* Fixed bytes; not necessarily terminated. */
    uint16_t width,height;
    uint32_t rom_offset;          /* Pixel payload offset, not descriptor offset. */
    uint8_t *pixels;              /* Owned column-major bytes: pixels[x*height+y]. */
} SsaTextureImage;
typedef struct SsaTextureArchive {
    uint32_t rom_offset;
    uint16_t count;
    SsaTextureImage *images;
} SsaTextureArchive;
typedef struct SsaTextureFamily {
    SsaTextureArchive archives[SSA_TEXTURE_MIPS];
    uint16_t (*mip_indices)[SSA_TEXTURE_MIPS]; /* Base order; FFFF falls back to base. */
} SsaTextureFamily;
typedef struct SsaTextureAssets {
    SsaTextureFamily families[SSA_TEXTURE_FAMILIES];
    unsigned loaded;
} SsaTextureAssets;

/* Native AFF4/AF18 data recovery for AENP resource entries16..21. Zero-init;
 * successful load owns all copies and does not retain ROM. Return1/-1, atomic
 * on failure. Free before reuse; keep structure and owned storage unmodified. */
int ssa_texture_assets_load(SsaTextureAssets *,const void *rom,size_t size);
void ssa_texture_assets_free(SsaTextureAssets *);
/* Original case-insensitive eight-byte name comparison, first match. Returns
 * index or -1. A NUL terminates comparison; high bytes follow original table. */
int ssa_texture_archive_find(const SsaTextureArchive *,const char name[8]);
/* Borrowed image; AF18's missing lower-mip fallback returns the base image.
 * NULL on invalid arguments. Dimensions remain the actual selected image's. */
const SsaTextureImage *ssa_texture_assets_image(const SsaTextureAssets *,
    unsigned family,unsigned base_index,unsigned mip);
/* Original level-family choice:0 for0..5,12,13;1 for6..11,14,15; -1 invalid. */
int ssa_texture_assets_family(unsigned level);
/* Static asset raster conversion only: row-major palette indices, no palette
 * effects, wrapping, projection or mip choice. Writes width bytes per row and
 * retains padding. dst must not alias image storage. Return1/-1, no writes on
 * invalid image/output span. Evidence: BE60 column-copy block C9DA..CA4A. */
int ssa_texture_image_indices(const SsaTextureImage *,uint8_t *dst,
    size_t stride,size_t capacity);
#endif
