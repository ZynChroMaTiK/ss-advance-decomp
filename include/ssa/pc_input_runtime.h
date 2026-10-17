#ifndef SSA_PC_INPUT_RUNTIME_H
#define SSA_PC_INPUT_RUNTIME_H
#include "ssa/pc_input.h"
#include "ssa/input_dispatch.h"
#include "ssa/game_runtime.h"

typedef struct SsaPcInputRuntime {
    SsaPcInput *input;
    SsaGameRuntime *game;
    SsaPcInputSample sample;
    unsigned player_index;
    uint16_t unused_flags[4];
    int last_error;
    uint8_t controls_enabled,polled;
    uint8_t sampled;
    uint64_t poll_serial;
} SsaPcInputRuntime;
/* Borrow initialized input/game; index is original GAME+68 input recipient,
 * distinct from display-local. No remote input or network transport invented.
 * Keep storage stable and free this adapter before game/input teardown. */
int ssa_pc_input_runtime_init(SsaPcInputRuntime *,SsaPcInput *,SsaGameRuntime *,unsigned player_index);
int ssa_pc_input_runtime_free(SsaPcInputRuntime *);
void ssa_pc_input_runtime_enable(SsaPcInputRuntime *,int enabled);
int ssa_pc_input_runtime_key(SsaPcInputRuntime *,int key,int down,int repeat);
int ssa_pc_input_runtime_button(SsaPcInputRuntime *,unsigned button,int down);
int ssa_pc_input_runtime_relative(SsaPcInputRuntime *,int32_t dx,int32_t dy);
void ssa_pc_input_runtime_focus(SsaPcInputRuntime *,int focused);
/* Call from the matching game external stage. POLL samples PC events and fills
 * processed_input. DISTRIBUTE runs the exact original local/bypass gate, then
 * applies the PC extension to ONLY selected player's keys/yaw/jump/fire.
 * Genuine multiplayer without original bypass requires network code: returns
 * NETWORK_REQUIRED(-2), retains remote data, never silently succeeds.
 * Inactive BCbit4 players retain all prior inputs. Paused/input-mode3/explicitly
 * disabled controls consume actions and pending
 * yaw. Held action keys must release before resume can synthesize an edge.
 * Focus loss/disable after sampling suppresses that cached sample immediately.
 * Both stage functions enforce the same current step_serial; duplicate sampling
 * or stale distribution fails. CONTROL/sprite presentation remain external.
 * Returns0/-1/-2; last_error
 * retains a precise distributor error when outer frame converts failure to-1. */
int ssa_pc_input_runtime_poll(SsaPcInputRuntime *);
int ssa_pc_input_runtime_distribute(SsaPcInputRuntime *);
#endif
