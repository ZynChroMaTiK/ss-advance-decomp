#ifndef SSA_MAP_VIEW_H
#define SSA_MAP_VIEW_H
#include <stddef.h>
#include <stdint.h>

/* Original pre-level campaign map (01A9A4/01AAA0/01AC80). ROM data is borrowed.
 * This screen owns no gameplay state; simulation starts only after done. */
typedef struct SsaMapGlyph {
    int16_t x,y;
    uint8_t glyph,font,palette;
} SsaMapGlyph;
typedef struct SsaMapView {
    const uint8_t *rom;
    size_t rom_size;
    uint8_t background[240*160],fonts[20160];
    uint16_t palette[256];
    SsaMapGlyph glyphs[128];
    unsigned loaded,level,language,difficulty,glyph_count;
    uint32_t frame;
    int fade_volume,fade_step;
    unsigned fading,leaving,done;
} SsaMapView;
/* Zero-initialize before init; valid campaign maps0..11, languages/difficulties0..2. */
int ssa_map_view_init(SsaMapView *,const uint8_t *,size_t,unsigned level,
    unsigned language,unsigned difficulty);
/* One original60Hz presentation tick. Returns1 on first fire press,0 otherwise,
 * -1 on invalid input. The caller plays original click song2 and starts music
 * fade5 on that first press. Start with music command3; command2 only after done. */
int ssa_map_view_step(SsaMapView *,unsigned fire_pressed);
int ssa_map_view_draw(SsaMapView *,uint16_t *rgb555,size_t pixels);
/* Shared original menu font/palette canvas. Caller resets glyph_count, submits
 * text, and composes. These do not advance map/fade/gameplay state. */
const char *ssa_map_view_text(const SsaMapView *,unsigned index);
unsigned ssa_map_view_text_width(const SsaMapView *,const char *,unsigned font);
int ssa_map_view_text_line(SsaMapView *,const char *,int x,int y,unsigned font,unsigned palette);
int ssa_map_view_text_centered(SsaMapView *,const char *,int y,unsigned font,unsigned palette,int offset);
int ssa_map_view_compose(const SsaMapView *,uint16_t *,size_t pixels);
/* Draw only the queued original glyphs over an existing RGB555 canvas, without
 * clearing the background or applying a fade. Supports separate PC UI batches
 * while retaining the original128-glyph limit per batch. */
int ssa_map_view_overlay(const SsaMapView *,uint16_t *,size_t pixels);
#endif
