#ifndef SSA_SPRITE_ASSETS_H
#define SSA_SPRITE_ASSETS_H
#include <stddef.h>
#include <stdint.h>

enum { SSA_SPRITE_IMAGE_COUNT = 311, SSA_SPRITE_PALETTE_COLORS = 256 };
typedef struct SsaSpritePart {
    uint16_t attribute0, attribute1, tile;
    int16_t x, y;                 /* Signed nine-bit offsets from image origin. */
    uint8_t width, height;
} SsaSpritePart;
typedef struct SsaSpriteImage {
    uint32_t rom_offset;
    uint16_t part_count, tile_count;
    int8_t bounds[4];             /* Original DBF4 culling bounds, not allocation. */
    int16_t min_x, min_y;
    uint16_t width, height;       /* Union of full tile pieces, including zero pixels. */
    SsaSpritePart *parts;
    uint8_t *tiles;               /* Owned 64-byte 8x8 tiles, original cache order. */
} SsaSpriteImage;
typedef struct SsaSpriteAssets {
    SsaSpriteImage images[SSA_SPRITE_IMAGE_COUNT];
    uint16_t palette[SSA_SPRITE_PALETTE_COLORS]; /* RGB555; index0 cleared by07014. */
    uint16_t background_palette[2][SSA_SPRITE_PALETTE_COLORS];
    unsigned loaded;
} SsaSpriteAssets;

/* AENP table0844B710, real311 AGB! images and palettes. Copies all data; no ROM
 * instruction executes and ROM storage may be freed after loading. Zero-init;
 * free before reuse. Return1/-1, output unchanged on failure. Compressed AGB*
 * images are not present in this table and are explicitly unsupported here. */
int ssa_sprite_assets_load(SsaSpriteAssets *, const void *rom, size_t size);
void ssa_sprite_assets_free(SsaSpriteAssets *);
/* Original1F44 background palette selection: family0 for levels0..5,12,13;
 * family1 for6..11,14,15. Return0/1, or-1 for invalidlevel. */
int ssa_sprite_palette_family(unsigned level);
/* Decode static asset pieces into a row-major palette-index image. Writes only
 * width bytes per row; untouched padding. Index0 is transparent. Earlier OAM
 * pieces win overlapping nonzero pixels. Does not implement runtime flipping,
 * scaling, clipping, blending, cache timing, world projection or rendering.
 * dst must not alias image storage. Return1/-1; validation precedes writes. */
int ssa_sprite_image_indices(const SsaSpriteImage *, uint8_t *dst,
                              size_t stride, size_t capacity);
/* RGB555 to RGBA8888 bytes (bit replication). Palette index0 has alpha0;
 * other indices alpha255. This host conversion is not original GBA code. */
int ssa_sprite_palette_rgba(const uint16_t palette[256], uint8_t rgba[256][4]);
#endif
