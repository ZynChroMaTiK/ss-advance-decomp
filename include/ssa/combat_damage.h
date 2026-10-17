#ifndef SSA_COMBAT_DAMAGE_H
#define SSA_COMBAT_DAMAGE_H
#include "ssa/world.h"
#include "ssa/inventory.h"

typedef struct SsaCombatDamageTarget {
    SsaEntity *entity;
    int16_t *health;             /* Canonical original entity +32 projection. */
    uint16_t *timer;             /* +4E; required for flag-10 player bodies. */
    SsaInventory *inventory;     /* Player record C6/C4 armor, when flag 10. */
} SsaCombatDamageTarget;
typedef struct SsaCombatDamage {
    const SsaEntity *source;     /* 0300443C, may be NULL in multiplayer. */
    const SsaEntity *source_owner; /* Explicit source entity +20 projection. */
    uint32_t *shot_hit_count;    /* Selected GAME+9C+(03003F4C-1)*16, or NULL. */
    uint32_t enemy_hits[12], enemy_kills[12]; /* GAME+134/+138, stride 12. */
    uint32_t frame_counter, hurt_flash, credit_player;
    uint8_t *credited_kills;     /* GAME+83+credit_player, when multiplayer. */
    uint16_t local_player, local_flags; /* 0300464E and that player's +BC. */
    uint8_t player_count, winner, difficulty, god_mode, kill_limit;
    unsigned updating;
} SsaCombatDamage;
typedef enum SsaCombatDamageEvent {
    SSA_COMBAT_DAMAGE_NETWORK,   /* 5850: a=target player index,b=damage u16. */
    SSA_COMBAT_DAMAGE_SOUND,     /* 8D5C: a=15 or16, after hurt_flash=9. */
    SSA_COMBAT_DAMAGE_REWARD     /* 125E0: target health already zero. */
} SsaCombatDamageEvent;
typedef struct SsaCombatDamageOps {
    /* >=0 success, <0 failure; earlier mutations remain on failure. Do not
     * mutate/reenter this context or target from a callback. Source, target,
     * pointers and storage must remain stable until the call returns. */
    int (*event)(void *, const SsaCombatDamageTarget *, SsaCombatDamageEvent,
                 uint32_t a, uint32_t b);
    void *context;
} SsaCombatDamageOps;
/* Complete 037F4, with explicit network/sound/reward boundaries. Damage is
 * unsigned 32-bit, with original wrapped halfwords and unsigned scaling.
 * A NULL target is the original no-op. 0 completed/-1 invalid or callback error.
 * Missing single-player player-hit source is rejected instead of dereferenced.
 * Does not synchronize native inventory/body mirrors, unlink, release or notify
 * groups. The caller owns those runtime transitions. No spatial queries occur. */
int ssa_combat_damage(SsaCombatDamage *, SsaCombatDamageTarget *, uint32_t damage,
                       const SsaCombatDamageOps *);
#endif
