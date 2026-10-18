#ifndef SSA_COMPLETION_H
#define SSA_COMPLETION_H
#include <stddef.h>
#include <stdint.h>

/* The single-player tally loop inside original02564, not the outer screen,
 * world snapshot/fade, commit or chapter/ending router. Ammo is original
 * weapon-record+12..20: weapons3..10, signed16. Borrowed storage is unnecessary. */
typedef struct SsaCompletion {
    uint32_t counter,score,pending,elapsed,par_seconds;
    int16_t ammo[8];
    uint16_t flags,notice_timer,level;
    uint8_t difficulty,bonus_y;
    uint32_t stage,hold,delay,flash;
    uint16_t blend;
    uint16_t blend_table[30];
    unsigned loaded,running,updating;
    uint32_t source_score,source_counter;
    unsigned recorded;
} SsaCompletion;
typedef enum SsaCompletionEvent {
    SSA_COMPLETION_NOTICE, /* Original E914: a=text ID,b=0,c=40,d=0. */
    SSA_COMPLETION_COMMAND,/* Original8EB0: a=16 tally click, other args0. */
    SSA_COMPLETION_SOUND   /* Original8D5C: a=7 short PSG cue, other args0. */
} SsaCompletionEvent;
typedef struct SsaCompletionOps {
    /* Observe committed prefix state and service the original presentation
     * request; return>=0 success/<0 error. Do not mutate/reenter this state or
     * invalidate ops. Prefix effects remain on error; guard always clears.
     * Original notice formatting/animation remains the caller's responsibility. */
    int (*event)(void *,const SsaCompletion *,SsaCompletionEvent,uint32_t a,
                 uint32_t b,uint32_t c,uint32_t d);
    void *context;
} SsaCompletionOps;
/* Start the recovered tally phase with original stage0/hold60/flash29, flags10,
 * bonus_y88. Prior score/counter and input values are explicit. Only original
 * campaign levels0..11 are accepted. Output unchanged on invalid input. ROM is
 * read only for the original par-time table and blend coefficients. Music9 and
 * the pre-tally world capture/fade must be handled before this phase. */
int ssa_completion_init(SsaCompletion *,const void *rom,size_t,unsigned level,
    unsigned difficulty,uint32_t counter,uint32_t score,uint32_t elapsed,
    uint16_t flags,const int16_t ammo[8]);
/* One original presentation tick; do not use render delta time. held bit1
 * accelerates count100->1000 and skips short waits; pressed bit1 dismisses the
 * final message after the original delay. voice_finished (legacy name) is
 * original signed music-player [03006990]+4<0, not sound7 or a wall clock.
 * Returns1 while running,
 * 0 done (also subsequent calls),-1 invalid/reentry/callback failure.
 * All arithmetic wraps32; signed comparisons preserve original edge behavior. */
int ssa_completion_step(SsaCompletion *,uint32_t held,uint32_t pressed,
    unsigned voice_finished,const SsaCompletionOps *);
#endif
