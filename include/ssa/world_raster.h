#ifndef SSA_WORLD_RASTER_H
#define SSA_WORLD_RASTER_H

#include <stddef.h>
#include <stdint.h>

#define SSA_WORLD_RASTER_STRIDE 120u
#define SSA_WORLD_RASTER_HEIGHT 120u

/* Native ARM 080002F4 / IWRAM03000270 textured column.
 * Pixels are palette indices, stored every 120 bytes. Fraction and step wrap
 * at 32 bits. sample_shift is 25..30 or 32, the LSR choices used by BE60.
 * Shift 32 samples texture[0]. count must be 1..120.
 *
 * The original signed final-fraction comparison can replace the last output
 * pixel with its predecessor. With count == 1 that predecessor is the existing
 * framebuffer byte at destination - 120; it must be in the supplied buffer.
 * Source and destination may overlap: reads/writes retain their original order.
 * Input storage must remain stable throughout this call.
 * Returns 0 on success, -1 on invalid bounds/parameters without changing pixels.
 * Projection, clipping, mip choice, palette conversion and presentation are
 * caller responsibilities. This is a raster primitive, not a scene renderer.
 */
int ssa_world_raster_column(uint8_t *framebuffer, size_t framebuffer_size,
    size_t destination, const uint8_t *texture, size_t texture_size,
    uint32_t fraction, uint32_t step, uint32_t count, unsigned sample_shift);

/* Same original sampling/count arithmetic, but discard a trailing pixel in the
 * row immediately beyond framebuffer_size. No other out-of-bounds span is
 * allowed. Shortening count would move the final-pixel correction on screen. */
int ssa_world_raster_column_clip_bottom(uint8_t *framebuffer, size_t framebuffer_size,
    size_t destination, const uint8_t *texture, size_t texture_size,
    uint32_t fraction, uint32_t step, uint32_t count, unsigned sample_shift);

#endif
