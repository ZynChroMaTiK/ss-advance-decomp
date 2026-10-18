#ifndef SSA_INTERMISSION_H
#define SSA_INTERMISSION_H
#include "ssa/native_completion.h"
#include "ssa/pause.h"
/* Stable presentation owner; borrows the frozen game's assets/math. Free or
 * reset before destroying that game. No gameplay fields are changed here. */
typedef struct SsaIntermission {
    const SsaNativeGame *game;
    SsaCompletion tally;
    SsaPause pause;
    SsaWeaponObject objects[84];
    SsaSpriteDispatch sprites;
    SsaSpritePool pool;
    SsaSpriteCache cache;
    SsaSpriteOam oam;
    SsaHudNotice notice;
    SsaHudSpriteState hud,bonus;
    unsigned loaded,completion,frame,commands[8],command_count,error_line;
    unsigned final_visible_ticks;
} SsaIntermission;
int ssa_intermission_init(SsaIntermission *,const SsaNativeGame *,unsigned language,unsigned completion);
/* Pause input uses packed edges; completion uses held bit1 and pressed bit1.
 * Fresh completion presses settle the current bonus in full. The final text
 * stays readable for180 visible ticks (fresh confirm after60 can dismiss).
 * voice_finished is the original music-player completion status (or1 without
 * audio). The legacy argument name does not imply recorded speech.
 * Returns1 active,0 finished,-1 error. Commands serviced by host each tick. */
int ssa_intermission_step(SsaIntermission *,uint32_t held,uint32_t pressed,unsigned voice_finished);
/* Original glyph/number sprites over a frozen PC/reference frame, preserving
 * that frame's resolution. Draw never advances state. */
int ssa_intermission_overlay(SsaIntermission *,const uint32_t *,unsigned width,unsigned height,uint32_t *);
/* Native240x160 pointer cells; selection changes only on real enabled rows.
 * Returns packed confirm edge on click,0 otherwise. */
uint32_t ssa_intermission_pointer(SsaIntermission *,int x,int y,unsigned click);
#endif
