#ifndef SSA_PALETTE_EFFECT_H
#define SSA_PALETTE_EFFECT_H
#include <stddef.h>
#include <stdint.h>
/* Background-palette phase of original0750C. Object/HUD palette is separate.
 * Borrow unchanged ROM data; step once per simulation presentation phase,
 * never from extra renderer draws. The request is shared with combat hurt
 * flash and weapon8/9; preserve its high bits while counting down. */
typedef struct SsaPaletteEffect {
    const uint8_t *rom;size_t size;
    uint16_t colors[256];
    unsigned level;
    uint8_t counter;
} SsaPaletteEffect;
int ssa_palette_effect_init(SsaPaletteEffect *,const uint8_t *,size_t,unsigned level);
/* 1 success,-1 invalid data/request; no partial writes on failure. Protecting
 * local flag8 selects the original special palette and forces request1.
 * Valid counts:0/1,2..9 for hurt/weapon flashes,2..31 for bit20000000. */
int ssa_palette_effect_step(SsaPaletteEffect *,uint32_t *request,uint16_t local_flags);
#endif
