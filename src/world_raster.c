#include "ssa/world_raster.h"

static int column(uint8_t *framebuffer, size_t framebuffer_size,
    size_t destination, const uint8_t *texture, size_t texture_size,
    uint32_t fraction, uint32_t step, uint32_t count, unsigned sample_shift,
    int clip_bottom)
{
    uint32_t final_fraction, scan, i;
    size_t last;
    int repeat_previous;
    if (!framebuffer || !texture || !count || count > SSA_WORLD_RASTER_HEIGHT ||
        !((sample_shift >= 25u && sample_shift <= 30u) || sample_shift == 32u) ||
        destination >= framebuffer_size ||
        destination > SIZE_MAX - (size_t)(count - 1u) * SSA_WORLD_RASTER_STRIDE)
        return -1;
    last = destination + (size_t)(count - 1u) * SSA_WORLD_RASTER_STRIDE;
    if (last >= framebuffer_size &&
        (!clip_bottom || last - framebuffer_size >= SSA_WORLD_RASTER_STRIDE))
        return -1;
    final_fraction = fraction + step * count;
    /* ARM CMP/BGT compares signed words without saturating the accumulator. */
    repeat_previous = (step ^ UINT32_C(0x80000000)) <=
                      (final_fraction ^ UINT32_C(0x80000000));
    if (repeat_previous && last < SSA_WORLD_RASTER_STRIDE)
        return -1;
    scan = fraction;
    for (i = 0; i < count; ++i) {
        uint32_t index = sample_shift == 32u ? 0u : scan >> sample_shift;
        if ((size_t)index >= texture_size)
            return -1;
        scan += step;
    }
    for (i = 0; i < count; ++i) {
        uint32_t index = sample_shift == 32u ? 0u : fraction >> sample_shift;
        size_t at = destination + (size_t)i * SSA_WORLD_RASTER_STRIDE;
        if (at < framebuffer_size)
            framebuffer[at] = texture[index];
        fraction += step;
    }
    if (repeat_previous && last < framebuffer_size)
        framebuffer[last] = framebuffer[last - SSA_WORLD_RASTER_STRIDE];
    return 0;
}

int ssa_world_raster_column(uint8_t *framebuffer, size_t framebuffer_size,
    size_t destination, const uint8_t *texture, size_t texture_size,
    uint32_t fraction, uint32_t step, uint32_t count, unsigned sample_shift)
{
    return column(framebuffer, framebuffer_size, destination, texture, texture_size,
        fraction, step, count, sample_shift, 0);
}

int ssa_world_raster_column_clip_bottom(uint8_t *framebuffer, size_t framebuffer_size,
    size_t destination, const uint8_t *texture, size_t texture_size,
    uint32_t fraction, uint32_t step, uint32_t count, unsigned sample_shift)
{
    return column(framebuffer, framebuffer_size, destination, texture, texture_size,
        fraction, step, count, sample_shift, 1);
}
