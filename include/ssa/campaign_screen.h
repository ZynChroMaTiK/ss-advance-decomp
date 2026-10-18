#ifndef SSA_CAMPAIGN_SCREEN_H
#define SSA_CAMPAIGN_SCREEN_H
#include "ssa/campaign_story.h"

/* Original19268/197B0 campaign presentation, using native pixels and ROM data.
 * Allocate on the heap, zero before init. No world or player ownership. */
typedef struct SsaCampaignScreen {
    SsaCampaignStory story;
    const uint8_t *rom;size_t rom_size;
    uint8_t tiles[32768],panels[8][16384],fonts[4032],rows[21][768];
    uint16_t palette[256];
    int16_t draw_positions[4][2];
    unsigned loaded,language,next_level,frame,scroll,draw_scroll,draw_text_scroll;
    unsigned text_fade,text_enabled,alpha,blend_a,blend_b;
    int fade_volume,fade_step;
    unsigned fading,done,image_ready,updating;
    unsigned commands[4],command_count,fade_intervals[2],fade_count;
} SsaCampaignScreen;
int ssa_campaign_screen_init(SsaCampaignScreen *,const uint8_t *,size_t,
                             unsigned next_level,unsigned language,unsigned music);
/* One60Hz presentation update. Processed story input bits are documented in
 * campaign_story.h. Output queues reset each step; consume initial command11
 * after init too. Commands go through the menu/music service; fade_intervals
 * go to the sound-fade service (7 is NOT music command7). */
int ssa_campaign_screen_step(SsaCampaignScreen *,uint32_t input);
int ssa_campaign_screen_draw(const SsaCampaignScreen *,uint16_t *rgb555,size_t pixels);
/* Original localized18DFC and18AB0/18C84/18D70 row service. Output is12 tiled
 * 8x8 BG tiles (96x8), including original2px inset, palette directives and
 * cropped glyph metrics. Rows past the last line are blank. */
int ssa_campaign_screen_lines(const SsaCampaignScreen *,unsigned text_id);
int ssa_campaign_screen_row(const SsaCampaignScreen *,unsigned text_id,unsigned row,
                             uint8_t out[768]);
#endif
