#ifndef SSA_PC_INPUT_H
#define SSA_PC_INPUT_H
#include <stdint.h>

enum {SSA_PC_MOUSE_LEFT=1};
enum SsaPcKeyBits {SSA_PC_W=1,SSA_PC_S=2,SSA_PC_A=4,SSA_PC_D=8,
    SSA_PC_SPACE=16,SSA_PC_H=32,SSA_PC_E=64,SSA_PC_R=128};
typedef struct SsaPcInput {
    uint32_t down,blocked,pressed,pending_yaw;
    int32_t yaw_per_count; /* 65536 heading units/turn; default64 recommended. */
    int32_t pending_pitch;
    uint8_t left_down,left_blocked,fire_pressed,jump_pressed,use_pressed;
    uint8_t view_toggle,focused,enabled,reload_pressed;
} SsaPcInput;
typedef struct SsaPcInputSample {
    uint32_t keys,packed; /* keys: movement/use only; packed: original poll word. */
    int32_t yaw_delta;
    int32_t pitch_delta; /* Positive looks up; clamped accumulation, no wrap. */
    uint16_t held,pressed; /* Weapon fire only, separate from movement/use. */
    uint8_t jump_down,view_toggle,enabled,reload_pressed;
} SsaPcInputSample;
/* New PC controls, not a matched GBA routine. Initialize with sensitivity>0.
 * WASD moves/strafe; opposing keys cancel. Space jumps; H toggles; E uses.
 * R requests a reload once per press; its PC-only edge is separate from packed.
 * Mouse-left fires; horizontal relative counts turn, positive is clockwise.
 * Vertical relative counts aim, with normal (non-inverted) mouse look.
 * All functions are single-threaded.
 * No OS registration/window is provided by this portable event accumulator. */
int ssa_pc_input_init(SsaPcInput *,int32_t yaw_per_count);
int ssa_pc_input_sensitivity(SsaPcInput *,int32_t yaw_per_count);
/* Disable consumes pending actions/yaw and blocks held action keys/buttons
 * until released. Held movement may resume. Focus loss clears all held/pending
 * input; focus gain never synthesizes presses. Selected camera mode is external. */
void ssa_pc_input_enable(SsaPcInput *,int enabled);
void ssa_pc_input_focus(SsaPcInput *,int focused);
/* ASCII key names (case-insensitive), Space=' '. Repeats never synthesize
 * action presses. Returns1 handled,0 unsupported,-1 null/uninitialized. */
int ssa_pc_input_key(SsaPcInput *,int key,int down,int repeat);
int ssa_pc_input_button(SsaPcInput *,unsigned button,int down);
int ssa_pc_input_relative(SsaPcInput *,int32_t dx,int32_t dy);
/* Consume pending edges and yaw once. A press/release between samples still
 * yields a one-frame jump/fire pulse. H pairs reduce to their toggle parity.
 * Heading arithmetic wraps modulo32 without signed overflow, then native
 * movement wraps modulo65536. Invalid input leaves sample/state unchanged. */
int ssa_pc_input_sample(SsaPcInput *,SsaPcInputSample *);
#endif
