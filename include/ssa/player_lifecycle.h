#ifndef SSA_PLAYER_LIFECYCLE_H
#define SSA_PLAYER_LIFECYCLE_H
#include "ssa/player_spawn.h"
#include "ssa/player_frame.h"
#include "ssa/weapon_select.h"

/* Fields of the weapon display object touched by 08005FEC. Rendering and
 * weapon animation execution are separate systems. */
typedef struct SsaRespawnWeaponView {
    int32_t x, y;
    uint16_t flags;
    uint8_t kind, player_number;
} SsaRespawnWeaponView;

typedef struct SsaPlayerLifecycle {
    SsaPlayerSpawn *player;
    SsaPlayerFrame *frame;
    SsaCamera *camera;
    SsaWeaponControl *weapon;
    SsaRespawnWeaponView *weapon_view;
    uint16_t death_ticks; /* player +DA, separate from entity +4E */
    unsigned updating;
} SsaPlayerLifecycle;

typedef struct SsaPlayerLifecycleGame {
    uint16_t message_ticks, input_state; /* game +76 / input +14 */
    uint8_t ticks, player_count, local_player;
    uint8_t continue_flag, exit_flag; /* game +7C / +7D */
} SsaPlayerLifecycleGame;

enum SsaPlayerLifecycleEvent {
    SSA_LIFECYCLE_ANIMATION,       /* 05394: animation, period */
    SSA_LIFECYCLE_NOTICE,          /* 0E914: all four original arguments */
    SSA_LIFECYCLE_SOUND,           /* 08D5C: sound */
    SSA_LIFECYCLE_MODE,            /* 08EB0: mode */
    SSA_LIFECYCLE_UNLINK,          /* 0A65C: remove current body from world */
    SSA_LIFECYCLE_EFFECT,          /* 14B60: effect, x, y, z */
    SSA_LIFECYCLE_INSERT,          /* 0A5D0: insert current body into world */
    SSA_LIFECYCLE_WEAPON_ANIMATION /* 0FC04: animation, period */
};
typedef struct SsaPlayerLifecycleOps {
    int (*event)(void *, SsaPlayerLifecycle *, SsaPlayerLifecycleGame *,
                 enum SsaPlayerLifecycleEvent, int32_t, int32_t, int32_t, int32_t);
    int (*select_spawn)(void *, SsaPlayerLifecycle *, SsaPlayerLifecycleGame *,
                        SsaSpawnMarker *);
    void *context;
} SsaPlayerLifecycleOps;

/* Whole original 06154 and 05FEC control flow, projected onto known host state.
 * inventory.health / motion.movement.flags are authoritative; mirrors are
 * synchronized. weapon.flags must initially equal movement.weapon_flags.
 * frame pointers must reference this player's body/movement. All state and
 * pointer identities remain valid across callbacks. No reentry or replacement.
 * Callbacks return >=0 success, <0 failure; selector must fill every marker
 * component on success. Event callbacks implement the named external boundary;
 * they may change scalar state. UNLINK/INSERT must maintain spatial ownership.
 * Animation fields are written by the event adapter, not preemptively here.
 * Returns 0/-1. Earlier effects remain on failure (including an unlinked body).
 * Unknown original player bytes have no host representation. Respawn clears
 * known non-weapon player fields; it preserves ammo slots2..10 and pending
 * entries while resetting ownership/current/requested to1 and pending_count0.
 * Death respawns natively through the same ops, rather than a respawn stub.
 * Runtime must reset its native-only jump latch/camera pose after respawn. */
int ssa_player_death_step(SsaPlayerLifecycle *, SsaPlayerLifecycleGame *,
                            const SsaPlayerLifecycleOps *);
int ssa_player_respawn(SsaPlayerLifecycle *, SsaPlayerLifecycleGame *,
                         const SsaPlayerLifecycleOps *);
#endif
