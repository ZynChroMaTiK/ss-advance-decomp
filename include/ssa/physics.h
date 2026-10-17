#ifndef SSA_PHYSICS_H
#define SSA_PHYSICS_H
#include <stdint.h>
#include "ssa/movement.h"

/* Host structures, not GBA binary overlays. Positions, velocities and height
 * use signed 16.16. Floor/ceiling are signed integer map heights. */
typedef struct SsaBody {
    int32_t x, y, z;
    int32_t velocity_x, velocity_y, velocity_z;
    int32_t height;
    uint16_t flags;
    int16_t floor, ceiling;
    int8_t hover_offset;
    int8_t step_adjust;
} SsaBody;

typedef struct SsaPlayerVertical {
    int32_t eye_offset, eye_velocity;
    uint16_t flags;
    uint16_t lowered;
} SsaPlayerVertical;

typedef struct SsaCamera {
    int32_t eye_z, bob_amplitude;
} SsaCamera;
typedef struct SsaCameraFrame {
    int32_t previous_bob;
    uint16_t counter;
    uint8_t is_local, footsteps_muted;
} SsaCameraFrame;

/* Port of 0x080062D4. Returns the sound event (0, 13 or 14); audio playback is
 * the caller's responsibility. Uses entity velocities after horizontal physics.
 * Preserves the original eye-offset and clamp arithmetic, including its quirks. */
int ssa_camera_step(const SsaBody *body, SsaPlayerVertical *player,
                     SsaCamera *camera, SsaCameraFrame *frame,
                     const SsaMathTables *tables);

enum {
    SSA_BODY_PLAYER = 0x10,
    SSA_BODY_NO_GRAVITY = 0x100,
    SSA_PLAYER_CONTROL_LOCK = 0x10,
    SSA_PLAYER_FLOOR_CONTACT = 0x20
};

/* Behavioral port of 0x08004CE0. Pass the corresponding player state for a
 * player body, NULL for other bodies. Return matches the original: zero when
 * the floor clamp happened, one otherwise (not a general grounded flag). */
int ssa_vertical_step(SsaBody *body, SsaPlayerVertical *player, uint8_t ticks);

typedef struct SsaJump {
    int32_t launch_velocity;
    uint8_t was_down;
} SsaJump;
#define SSA_JUMP_DEFAULT_VELOCITY (5 * INT32_C(65536))
#define SSA_JUMP_MAX_VELOCITY (30 * INT32_C(65536))
void ssa_jump_init(SsaJump *jump);

/* Native extension: jump edge, then vertical physics in one-tick substeps.
 * The caller supplies the held jump action; OS key binding is external.
 * ticks==0 is a no-op, including the input latch. No buffering, air jumps or
 * auto-repeat. Disabled actions still consume edges on a simulated tick.
 * Invalid/nonpositive launch speeds disable jumping; speeds above 30 clamp.
 * Returns one only if a jump began. Horizontal velocity is preserved. */
int ssa_player_vertical_step(SsaBody *body, SsaPlayerVertical *player,
                             SsaJump *jump, uint8_t ticks, int jump_down,
                             int controls_enabled);
#endif
