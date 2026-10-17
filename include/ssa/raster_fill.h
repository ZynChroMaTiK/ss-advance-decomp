#ifndef SSA_RASTER_FILL_H
#define SSA_RASTER_FILL_H
#include <stddef.h>
#include <stdint.h>

/* Original ARM fills03348 (flat plane rows) and03338 (columns), copied into
 * IWRAM005AC/002B4. Indexed bytes, including opaque index0. Column stride is
 * exactly120, not a configurable viewport. Return1/-1; reject zero count and
 * invalid bounds before any write. Original zero count would wrap and overrun.
 * No clipping, BSP traversal, span generation or palette effects are supplied. */
int ssa_raster_fill_row(uint8_t *dst,size_t capacity,size_t offset,
                         uint32_t count,uint8_t index);
int ssa_raster_fill_column(uint8_t *dst,size_t capacity,size_t offset,
                            uint32_t count,uint8_t index);
/* Original03358 forward byte copy used for sky spans (IWRAM005BC). Fully
 * validate both spans first. Overlap is allowed and retains original forward
 * propagation; this is not memmove. Caller owns stable readable/writable spans. */
int ssa_raster_copy_sky(uint8_t *dst,size_t capacity,size_t offset,
                        const uint8_t *src,size_t source_size,size_t source_offset,
                        uint32_t count);
#endif
