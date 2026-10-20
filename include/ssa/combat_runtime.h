#ifndef SSA_COMBAT_RUNTIME_H
#define SSA_COMBAT_RUNTIME_H
#include "ssa/combat_damage.h"
#include "ssa/combat_explosion.h"
#include "ssa/combat_rewards.h"
#include "ssa/enemy_cd_runtime.h"

typedef struct SsaCombatRuntime {
    SsaScriptSpawn *spawn;
    SsaEntity *reserved[100];
    SsaPlayerRuntime *players[4];
    SsaCombatDamage *damage;
    uint32_t *score;
    SsaCombatExplosion explosion;
    unsigned reserved_count, player_count, updating;
    unsigned boss_projectile_immunity; /* PC: D7/D8 ignore only their own projectile hits/blasts. */
    unsigned prion_balance; /* PC: current-health boost and enemy-only combat/attack policy. */
} SsaCombatRuntime;
typedef struct SsaCombatRuntimeOps {
    /* Only SOUND and NETWORK are forwarded; rewards update score natively.
     * Observe only: do not change/reenter world, actors, players or combat.
     * Storage remains stable, and earlier effects survive callback failure. */
    int (*event)(void *, const SsaCombatRuntime *, const SsaCombatDamageTarget *,
                 SsaCombatDamageEvent, uint32_t, uint32_t);
    void *context;
} SsaCombatRuntimeOps;
typedef struct SsaCombatCdOps {
    const SsaCombatRuntimeOps *combat;
    /* Animation and sound only. Contact/explosion damage is native. */
    int (*presentation)(void *, const SsaEnemyCd *, SsaEnemyCdEvent, SsaEntity *,
                         int32_t, int32_t, int32_t);
    void *context;
} SsaCombatCdOps;
/* Zero-init; caller retains all storage. reserved supplies original slot order,
 * copied here, as for enemy_queries. players is indexed by original player ID;
 * all must be registered reserved entities, count1..4. damage.player_count must
 * agree; damage.local_player selects local movement flags at every hit.
 * Targets are these players or owned actors, including adopted map slots;
 * other external reserved bodies may
 * participate in ordering but have no health projection in this adapter.
 * Init/free outside updates, free before script_spawn/players/world teardown.
 * No allocation or outer dispatcher is provided. All functions return0/-1,
 * except cd_step also returns1 for released actor. */
int ssa_combat_runtime_init(SsaCombatRuntime *, SsaScriptSpawn *,
    SsaEntity *const *reserved, size_t reserved_count,
    SsaPlayerRuntime *const *players, unsigned player_count,
    SsaCombatDamage *, uint32_t *score);
int ssa_combat_runtime_free(SsaCombatRuntime *);
/* source and source_owner explicitly supply original damage globals. They are
 * restored afterward, including errors. NULL target is the original no-op.
 * Enemy health/flags change now; AI handles death/group release on its update.
 * Player inventory.health is canonical, entity_health is synchronized even on
 * error. Player movement flags are distinct and never copied from body flags. */
int ssa_combat_runtime_damage(SsaCombatRuntime *, SsaEntity *target,
    const SsaEntity *source, const SsaEntity *source_owner, uint32_t amount,
    const SsaCombatRuntimeOps *);
int ssa_combat_runtime_explode(SsaCombatRuntime *, SsaEntity *origin,
    const SsaEntity *source, const SsaEntity *source_owner,
    uint32_t radius_squared, uint32_t amount, int include_players,
    const SsaCombatRuntimeOps *);
/* Native CD controller with actual contact/radial damage, ordered target pool
 * and score reward. Pass enemy_queries.ops for native navigation. Damage's
 * frame counter, credit and difficulty remain explicit caller-owned inputs.
 * Audio, animation playback and multiplayer packet delivery remain callbacks. */
int ssa_combat_cd_step(SsaCombatRuntime *, SsaEnemyCdRuntime *,
    const SsaEnemyCdFrame *, const SsaEnemyNavigationOps *, const SsaCombatCdOps *);
#endif
