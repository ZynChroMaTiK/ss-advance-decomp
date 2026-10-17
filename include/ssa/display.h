#ifndef SSA_DISPLAY_H
#define SSA_DISPLAY_H
#include "ssa/sprite_oam.h"

enum { SSA_DISPLAY_WIDTH=240, SSA_DISPLAY_HEIGHT=160,
       SSA_DISPLAY_PIXELS=38400, SSA_DISPLAY_OBJ_BYTES=32768 };
typedef struct SsaDisplayRegisters {
    uint16_t dispcnt,bg2cnt;
    int16_t pa,pb,pc,pd;
    int32_t x,y; /* Signed28-bit BG2 reference coordinates,8fractional bits. */
    uint16_t bldcnt,mosaic;
} SsaDisplayRegisters;
/* Normal074F0/0750C profile:mode1,BG2+OBJ,1D OBJ,PA128/PD192.
 * Sampling is x/2,floor(3*y/4), not a square output viewport. */
int ssa_display_registers_default(SsaDisplayRegisters *);
/* Native hardware-presentation subset for original normal gameplay. Borrow
 * world120x120 indices, both256-entry RGB555 palettes, complete raw OAM and
 * PHYSICAL06010000..06017FFF OBJ bytes (cache transfers mirror before here).
 * All128 entries are read, including matrix words. NULL OAM means no sprites;
 * otherwise loaded/idle OAM and >=32768 tile bytes required. Inputs/output
 * must not alias. No retained pointers. RGB555 output bit15 is cleared.
 * Returns1 success,-1 invalid,-2 explicitly unsupported. No output writes on
 * failure. Supports BG affine sampling,8bpp1D OBJ normal/affine/double/flip,
 * wrapped positions and tile fetches. Unsupported: other display modes/layers,
 * windows, blending, nontrivial mosaic,4bpp OBJ, decreasing OAM BG-priority
 * order (hardware priority anomaly), or conservative scanline-budget excess.
 * This is a native compositor, not CPU execution or full hardware emulation. */
int ssa_display_compose(const SsaDisplayRegisters *,const uint8_t *world,size_t world_size,
    const uint16_t background_palette[256],const uint16_t object_palette[256],
    const SsaSpriteOam *,const uint8_t *object_tiles,size_t tile_size,
    uint16_t *output,size_t output_pixels);
/* Normal gameplay0750C/0787C scanline palette: original boss/armor/health
 * ramps for OBJ139..143, including armor absence and health>100. Same
 * validation/ownership as compose; the supplied palette remains unchanged. */
int ssa_display_compose_gameplay(const SsaDisplayRegisters *,const uint8_t *world,size_t world_size,
    const uint16_t background_palette[256],const uint16_t object_palette[256],
    const SsaSpriteOam *,const uint8_t *object_tiles,size_t tile_size,
    uint16_t *output,size_t output_pixels,int16_t health,int16_t armor);
/* Host convenience, RGB555 ->0x00RRGGBB with five-bit channel replication.
 * Distinct buffers, at least38400 elements each.1/-1; no partial output. */
int ssa_display_rgb888(const uint16_t *,size_t,uint32_t *,size_t);
#endif
