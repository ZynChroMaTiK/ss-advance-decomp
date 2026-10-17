#ifndef SSA_PLAYER_RUNTIME_H
#define SSA_PLAYER_RUNTIME_H
#include "ssa/player_spawn.h"
#include "ssa/player_frame.h"
#include "ssa/view.h"

typedef struct SsaPlayerRuntime {
    SsaPlayerSpawn *player;
    SsaWorld *world;
    SsaSlide *slide;
    const SsaMathTables *tables;
    SsaPlayerFrame frame;
    SsaPlayerFrameGame game;
    SsaCamera camera;
    SsaCameraFrame camera_frame;
    SsaJump jump;
    SsaView view;
    SsaViewPose pose;
    int32_t pitch; /* PC look, 65536 units/turn, positive up, limited to +/-80deg. */
    uint8_t free_aim; /* Set by vertical input; original autoaim until then. */
    SsaMapCollision view_collision;
    unsigned updating;
} SsaPlayerRuntime;

typedef struct SsaPlayerRuntimeInput {
    SsaMotionFrame motion;
    uint32_t keys;
    int32_t yaw_delta; /* relative yaw already converted to 65536 units/turn */
    int32_t pitch_delta;
    uint16_t camera_counter;
    /* motion.local_player is game +68 (protection/push/remote movement).
     * local_player below is display/audio global 0300464E, independently. */
    uint8_t jump_down, controls_enabled, footsteps_muted, local_player;
} SsaPlayerRuntimeInput;

typedef struct SsaPlayerRuntimeOps {
    /* Invoked only for DEATH and HAZARDS. The movement and camera stages are
     * native. Hooks mutate player fields, not frame's position/eye cache.
     * inventory.health and motion.movement.flags are authoritative: their
     * entity_health/vertical.flags mirrors are refreshed after every hook.
     * World X/Y changes must use world movement and update motion.x/y too.
     * frame.timer/animation_state and game metadata may be changed directly. */
    int (*gameplay)(void *, SsaPlayerRuntime *, enum SsaPlayerFrameStage);
    int (*sector)(void *, SsaPlayerRuntime *, int16_t);
    int (*animation)(void *, SsaPlayerRuntime *, unsigned, unsigned);
    int (*sound)(void *, SsaPlayerRuntime *, unsigned);
    void *context;
} SsaPlayerRuntimeOps;

/* Zero-init runtime; spawned player, world, slide, tables and map must outlive
 * it. Allocates its own camera collision scratch and imports entity_health into
 * inventory.health. Returns 0 success/-1 invalid/allocation failure, without
 * changing player state on failure. A successful binding starts first person.
 * This does not add weapons, initialize enemies, or create an OS/game loop. */
int ssa_player_runtime_init(SsaPlayerRuntime *, SsaPlayerSpawn *, SsaWorld *,
                             SsaSlide *, const SsaMathTables *);
/* Frees only camera scratch; never removes/frees player/world/map. */
int ssa_player_runtime_free(SsaPlayerRuntime *);
int ssa_player_runtime_key(SsaPlayerRuntime *, int key, int down, int repeat,
                            int controls_enabled);
void ssa_player_runtime_focus_lost(SsaPlayerRuntime *);
/* Composes frame scheduling, actual controls/dynamic collision/sliding/jump,
 * camera height, and final H-camera pose. New native behavior retains full yaw
 * precision and uses jump substeps. Caller supplies physical input translation
 * and timing. Disabled controls consume jump edges, suppress keys/yaw, and let
 * existing movement decay normally. All four external callbacks are required;
 * nonnegative success / negative failure. No reentry or pointer replacement.
 * Returns 0 success/-1 error. Earlier effects remain on error; pose is refreshed
 * only after successful frame completion. Check return before rendering pose.
 * No multiplayer network scheduling or OS input backend is implemented here. */
int ssa_player_runtime_step(SsaPlayerRuntime *, const SsaPlayerRuntimeInput *,
                             const SsaPlayerRuntimeOps *);
#endif
