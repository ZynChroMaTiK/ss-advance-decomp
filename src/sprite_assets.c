#include "ssa/sprite_assets.h"
#include <stdlib.h>
#include <string.h>

static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)get16(p) | (uint32_t)get16(p + 2) << 16; }
static int16_t signed9(unsigned n)
{ n &= 511u; return (int16_t)(n < 256u ? (int)n : (int)n - 512); }
static int span(size_t size, size_t offset, size_t length)
{ return offset <= size && length <= size - offset; }

static int dimensions(const SsaSpritePart *p)
{
    static const uint8_t width[3][4] = {{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static const uint8_t height[3][4] = {{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    unsigned shape = p->attribute0 >> 14, size = p->attribute1 >> 14;
    /* Original resources have only shape/size and nine-bit position bits.
     * FCE4 supplies the runtime 8bpp/flip/affine flags separately. */
    return shape < 3 && !(p->attribute0 & 0x3E00u) &&
        !(p->attribute1 & 0x3E00u) &&
        p->width == width[shape][size] && p->height == height[shape][size] &&
        p->x == signed9(p->attribute1) && p->y == signed9(p->attribute0);
}

static int extent(const SsaSpriteImage *im, int *left, int *top,
                   int *right, int *bottom)
{
    unsigned i;
    if (!im || !im->parts || !im->tiles || !im->part_count ||
        im->part_count > 128 || !im->tile_count || im->tile_count > 512) return 0;
    *left = *top = 512; *right = *bottom = -512;
    for (i = 0; i < im->part_count; ++i) {
        const SsaSpritePart *p = &im->parts[i];
        unsigned count;
        int x2, y2;
        if (!dimensions(p)) return 0;
        count = (unsigned)p->width * p->height / 64u;
        if (p->tile > im->tile_count || count > (unsigned)im->tile_count - p->tile)
            return 0;
        x2 = p->x + p->width; y2 = p->y + p->height;
        if (p->x < *left) *left = p->x;
        if (p->y < *top) *top = p->y;
        if (x2 > *right) *right = x2;
        if (y2 > *bottom) *bottom = y2;
    }
    return 1;
}

static int load_image(SsaSpriteImage *im, const uint8_t *rom,
                      size_t size, unsigned index)
{
    uint32_t address = get32(rom + 0x44B710u + index * 4u);
    size_t off, payload;
    unsigned i;
    int x1, y1, x2, y2;
    if (address < UINT32_C(0x08000000)) return 0;
    off = (size_t)(address - UINT32_C(0x08000000));
    if (!span(size, off, 28) || memcmp(rom + off, "AGB!", 4)) return 0;
    im->rom_offset = (uint32_t)off;
    im->part_count = get16(rom + off + 4);
    im->tile_count = get16(rom + off + 6);
    if (!im->part_count || im->part_count > 128 ||
        !im->tile_count || im->tile_count > 512) return 0;
    payload = off + 28u + (size_t)im->part_count * 8u;
    if (!span(size, off + 28u, (size_t)im->part_count * 8u) ||
        !span(size, payload, (size_t)im->tile_count * 64u)) return 0;
    for (i = 0; i < 4; ++i) {
        unsigned b = rom[off + 8u + i];
        im->bounds[i] = (int8_t)(b < 128u ? (int)b : (int)b - 256);
    }
    im->parts = (SsaSpritePart *)calloc(im->part_count, sizeof(*im->parts));
    im->tiles = (uint8_t *)malloc((size_t)im->tile_count * 64u);
    if (!im->parts || !im->tiles) return 0;
    memcpy(im->tiles, rom + payload, (size_t)im->tile_count * 64u);
    for (i = 0; i < im->part_count; ++i) {
        const uint8_t *p = rom + off + 28u + i * 8u;
        SsaSpritePart *part = &im->parts[i];
        part->attribute0 = get16(p); part->attribute1 = get16(p + 2);
        part->tile = get16(p + 4); part->width = p[6]; part->height = p[7];
        part->x = signed9(part->attribute1); part->y = signed9(part->attribute0);
    }
    if (!extent(im, &x1, &y1, &x2, &y2)) return 0;
    im->min_x = (int16_t)x1; im->min_y = (int16_t)y1;
    im->width = (uint16_t)(x2 - x1); im->height = (uint16_t)(y2 - y1);
    return 1;
}

void ssa_sprite_assets_free(SsaSpriteAssets *out)
{
    unsigned i;
    if (!out) return;
    for (i = 0; i < SSA_SPRITE_IMAGE_COUNT; ++i) {
        free(out->images[i].parts); free(out->images[i].tiles);
    }
    memset(out, 0, sizeof(*out));
}

int ssa_sprite_assets_load(SsaSpriteAssets *out, const void *data, size_t size)
{
    const uint8_t *rom = (const uint8_t *)data;
    SsaSpriteAssets *fresh;
    unsigned i, family;
    if (!out || !rom || out->loaded ||
        !span(size, 0x44B710u, SSA_SPRITE_IMAGE_COUNT * 4u) ||
        !span(size, 0x79EFFCu, 512) || !span(size, 0x454A5Cu, 1024)) return -1;
    for (i = 0; i < SSA_SPRITE_IMAGE_COUNT; ++i)
        if (out->images[i].parts || out->images[i].tiles) return -1;
    fresh = (SsaSpriteAssets *)calloc(1, sizeof(*fresh));
    if (!fresh) return -1;
    for (i = 0; i < SSA_SPRITE_IMAGE_COUNT; ++i)
        if (!load_image(&fresh->images[i], rom, size, i)) goto fail;
    for (i = 0; i < 256; ++i) {
        fresh->palette[i] = get16(rom + 0x79EFFCu + i * 2u);
        for (family = 0; family < 2; ++family)
            fresh->background_palette[family][i] =
                get16(rom + 0x454A5Cu + family * 512u + i * 2u);
    }
    fresh->palette[0] = 0;
    fresh->background_palette[0][0] = fresh->background_palette[1][0] = 0;
    fresh->loaded = 1;
    *out = *fresh; free(fresh);
    return 1;
fail:
    ssa_sprite_assets_free(fresh); free(fresh);
    return -1;
}

int ssa_sprite_palette_family(unsigned level)
{
    if (level >= 16) return -1;
    return level <= 5 || level == 12 || level == 13 ? 0 : 1;
}

int ssa_sprite_image_indices(const SsaSpriteImage *im, uint8_t *dst,
                              size_t stride, size_t capacity)
{
    int x1, y1, x2, y2;
    unsigned i, y;
    if (!dst || !extent(im, &x1, &y1, &x2, &y2) ||
        im->min_x != x1 || im->min_y != y1 || im->width != x2 - x1 ||
        im->height != y2 - y1 || stride < im->width ||
        capacity < im->width || (im->height - 1u) > (capacity - im->width) / stride)
        return -1;
    for (y = 0; y < im->height; ++y) memset(dst + (size_t)y * stride, 0, im->width);
    /* Lower OAM entry has priority. Fill only transparent destination pixels. */
    for (i = 0; i < im->part_count; ++i) {
        const SsaSpritePart *p = &im->parts[i];
        unsigned x;
        for (y = 0; y < p->height; ++y) for (x = 0; x < p->width; ++x) {
            size_t tile = p->tile + (y / 8u) * (p->width / 8u) + x / 8u;
            uint8_t value = im->tiles[tile * 64u + (y % 8u) * 8u + x % 8u];
            uint8_t *pixel = dst + (size_t)(p->y - y1 + (int)y) * stride +
                (unsigned)(p->x - x1) + x;
            if (!*pixel) *pixel = value;
        }
    }
    return 1;
}

int ssa_sprite_palette_rgba(const uint16_t palette[256], uint8_t rgba[256][4])
{
    unsigned i, c;
    if (!palette || !rgba) return -1;
    for (i = 0; i < 256; ++i) {
        for (c = 0; c < 3; ++c) {
            unsigned n = (palette[i] >> (c * 5u)) & 31u;
            rgba[i][c] = (uint8_t)((n << 3) | (n >> 2));
        }
        rgba[i][3] = i ? 255 : 0;
    }
    return 1;
}
