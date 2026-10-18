#ifndef SSA_CAMPAIGN_STORY_H
#define SSA_CAMPAIGN_STORY_H
#include <stddef.h>
#include <stdint.h>

/* Original19458/193D8/1965C campaign panels. ROM is used only to copy the
 * eight presentation records. This controller does not own a screen, audio
 * device, text formatter or the outer197B0 fade/dispatch loop. */
typedef struct SsaCampaignStory {
    uint32_t counter,wait,index,stage,x,y,vx,vy;
    uint32_t display_image,image,scroll,rate,display_text,text,speed;
    int16_t positions[4][2];
    uint16_t fade_target;
    uint8_t mode,music;
    uint32_t records[8][5];
    unsigned loaded,updating;
} SsaCampaignStory;
typedef enum SsaCampaignStoryEvent {
    SSA_STORY_COMMAND,   /* a=13 on skip. */
    SSA_STORY_MUSIC,     /* a=7 on skip, only when music is enabled. */
    SSA_STORY_IMAGE,     /* a=original image index, after moving all4 sprites. */
    SSA_STORY_TEXT,      /* a=localized text ID; reset scroll before service. */
    SSA_STORY_LINES,     /* a=text ID; return original18DFC line count (font0). */
    SSA_STORY_ROW        /* a=text ID,b=row cast to16,c=signed row%21,d=12. */
} SsaCampaignStoryEvent;
typedef struct SsaCampaignStoryOps {
    /* Service requests and observe committed prefix state. Return>=0 on
     * success (LINES returns its count),<0 on failure. Do not mutate/reenter
     * this state or invalidate ops. Earlier effects persist on failure. */
    int (*event)(void *,const SsaCampaignStory *,SsaCampaignStoryEvent,
                 uint32_t a,uint32_t b,uint32_t c,uint32_t d);
    void *context;
} SsaCampaignStoryOps;
/* Original197B0 selects record0 for level0,4 for level6,6 for level12.
 * Other levels have an undefined stack index in the ROM and are rejected.
 * Initial previous-image69 forces the original first image transfer. */
int ssa_campaign_story_init(SsaCampaignStory *,const void *rom,size_t,
                           unsigned next_level,unsigned music_enabled);
/* One original1965C call. Input is processed original030042F0: skip10000,
 * pause40, accelerate80, skip wait800000 (ignored while pause40 is held).
 * mode1/2 represent the outer screen's leaving/fading states. Its owner may
 * set mode2 after starting the fade; this function does not finish that fade.
 * Returns1 while mode0,0 otherwise,-1 on invalid/reentry/service failure.
 * It still updates panels/text on calls in mode1/2, as the original does. */
int ssa_campaign_story_step(SsaCampaignStory *,uint32_t input,
                           const SsaCampaignStoryOps *);
#endif
