#ifndef SSA_NETRICSA_H
#define SSA_NETRICSA_H
#include "ssa/statistics.h"
#include <stddef.h>

/* Value snapshot; NETRICSA never owns or advances the live world. Weapon and
 * enemy rows are cumulative campaign records; totals include the current map. */
typedef struct SsaNetricsaData {
    SsaStatisticsRows rows;
    SsaStatisticsTotals totals;
    unsigned level,lives,show_all;
    uint32_t random;
    unsigned weapon_entries,enemy_entries; /* Persistent campaign discoveries. */
    unsigned opening_entry; /*0 briefing,1..10 weapon,11..22 enemy; must be available. */
} SsaNetricsaData;
enum { SSA_NET_CLOSE=0x10000,SSA_NET_FOCUS=0x20000,
       SSA_NET_NEXT=0x100000,SSA_NET_PREVIOUS=0x200000,
       SSA_NET_UP=0x400000,SSA_NET_DOWN=0x800000,
       SSA_NET_SCROLL_UP=0x40,SSA_NET_SCROLL_DOWN=0x80 };
enum { SSA_NET_POINTER_CLOSE=2 };
typedef struct SsaNetricsa {
    const uint8_t *rom;size_t rom_size;
    SsaNetricsaData data;
    uint8_t tiles[32768],frame_tiles[3456],images[23][4096],fonts[4032];
    uint16_t frame_map[600],palette[256];
    char formatted[2][512];
    const char *text[2];
    unsigned loaded,language,category,focus,weapon,enemy,has_enemies,image;
    unsigned title[2],text_id[2],counter,background_scroll;
    int scroll[2],delta[2],fade_volume,fade_step;
    unsigned closing,done,commands[8],command_count; /*>=0x100: direct sound ID+0x100. */
    unsigned hover; /*0outside;1..5categories,6/7text,8/9record halves,10..21numbered records. */
} SsaNetricsa;
/* Zero-init on the heap. Keep its address stable: text can refer to embedded
 * formatted buffers. Do not copy a loaded screen. ROM remains borrowed. */
int ssa_netricsa_init(SsaNetricsa *,const uint8_t *,size_t,unsigned language,const SsaNetricsaData *);
int ssa_netricsa_step(SsaNetricsa *,uint32_t processed_input);
/* PC presentation: 15 ticks (250ms at60Hz) per fade, reversible on early close.
 * Original controller timing remains available through ssa_netricsa_step. */
int ssa_netricsa_pc_step(SsaNetricsa *,uint32_t processed_input);
int ssa_netricsa_draw(const SsaNetricsa *,uint16_t *rgb555,size_t pixels);
/* Full-resolution gameplay overlay, no animated menu background. Background
 * contains world+weapon; with_hud is the same frame with all HUD sprites.
 * Fade removes HUD and darkens/tints world+weapon green while UI appears.
 * with_hud may be NULL (same as background). Output may alias either frame.
 * Screen/ROM storage must be distinct. */
int ssa_netricsa_overlay(const SsaNetricsa *,const uint32_t *background,const uint32_t *with_hud,unsigned width,
                         unsigned height,uint32_t *output);
/* Original text row service: width is BG tiles, output is tiled8bpp. */
int ssa_netricsa_row(const SsaNetricsa *,const char *,unsigned row,unsigned width,uint8_t *,size_t);
const char *ssa_netricsa_text(const SsaNetricsa *,unsigned id);
/* Letterboxed native240x160 coordinates. Hover highlights categories. Click
 * chooses a category or focuses a text panel. Wheel scrolls the hovered panel.
 * Click either half of the illustration or wheel over it to cycle records.
 * Numbered selectors above the upper panel jump to available records.
 * Returns -1 on error, 0 unchanged, 1 changed, or SSA_NET_POINTER_CLOSE when
 * the localized skip prompt is clicked (including its blink-off phase).
 * Feed SSA_NET_CLOSE to the next screen step for the normal fade/audio. */
int ssa_netricsa_pointer(SsaNetricsa *,int x,int y,unsigned click);
int ssa_netricsa_wheel(SsaNetricsa *,int steps);
/* Fully visible selected dossier only; closing/fading/other pages return zero masks. */
int ssa_netricsa_viewed(const SsaNetricsa *,unsigned *weapons,unsigned *enemies);
#endif
