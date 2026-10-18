#ifndef SSA_ENTITY_RUNTIME_H
#define SSA_ENTITY_RUNTIME_H
#include "ssa/entity_dispatch.h"
#include "ssa/enemy_queries.h"
#include "ssa/projectile_runtime.h"
#include "ssa/enemy_control_runtime.h"

typedef struct SsaEntityRuntime {
    SsaCombatRuntime *combat;
    SsaProjectileRuntime *projectiles;
    SsaEnemyQueries *queries;
    SsaPlayerGameplay *players[4];
    SsaEnemyCdRuntime *enemies[100];
    uint32_t *previous_walk_count[100];
    SsaEntity *entities[100];
    SsaEntityDispatch dispatch;
    unsigned updating;
    /* Optional canonical original global state for a composed game frame.
     * Private player/CD projections mirror these at their use sites, including
     * callback visibility and error exits. Borrowed storage remains stable. */
    SsaPlayerLifecycleGame *shared_life_game;
    SsaEnemyCdGame *shared_cd_game;
    SsaEnemyControlRuntime *generic;
    const SsaEnemyControlRuntimeOps *generic_ops;
    const int32_t *enemy_speeds; /* Optional12 original1288C type coefficients. */
    unsigned fast_converted; /* PC: two behavior updates for living Prion victims. */
    unsigned nearest_converted; /* PC: nearest living combatant, refreshed each behavior pass. */
} SsaEntityRuntime;
typedef struct SsaEntityRuntimeFrame {
    SsaPlayerRuntimeInput players[4];
    SsaEnemyCdFrame enemies;
    unsigned target_player;
} SsaEntityRuntimeFrame;
typedef struct SsaEntityRuntimeOps {
    const SsaLevelPlayerOps *players[4];
    const SsaCombatCdOps *enemies;
    const SsaProjectileRuntimeOps *projectiles;
    /* Optional observer, before each selected handler. Return>=0/-1. Do not
     * mutate/reenter the world, pool, controllers, bindings or dispatcher. */
    int (*before)(void *,const SsaEntityRuntime *,uint32_t entry,unsigned slot);
    void *context;
} SsaEntityRuntimeOps;
/* Native entity phase, not a complete game loop. Zero-init, stable addresses.
 * Existing players, projectiles, combat and queries must share spawn/world.
 * active_tail explicitly projects the original adjacent halfword used when
 * the pool has100 entries. No controller is invented for unsupported types.
 * Free before all referenced runtimes; owns no entities or allocated memory. */
int ssa_entity_runtime_init(SsaEntityRuntime *,SsaCombatRuntime *,SsaProjectileRuntime *,
    SsaEnemyQueries *,SsaPlayerGameplay *const *players,uint16_t active_tail);
/* Bind an initialized CD controller. Its walk counter is temporarily rebound
 * until release/unbind/free. Keep controller storage alive throughout. */
int ssa_entity_runtime_bind_cd(SsaEntityRuntime *,SsaEnemyCdRuntime *);
int ssa_entity_runtime_unbind_cd(SsaEntityRuntime *,unsigned slot);
int ssa_entity_runtime_bind_generic(SsaEntityRuntime *,SsaEnemyControlRuntime *,
    const SsaEnemyControlRuntimeOps *);
int ssa_entity_runtime_free(SsaEntityRuntime *);
/* Run once BEFORE mover/VM updates and weapons. Players use native level
 * interactions; CD uses native sight/navigation/combat; projectile/E2 uses
 * native flight/effects; owned E1 uses its original timer wrapper. Other known
 * handlers fail explicitly. Unhandled table types retain the ROM's no-op.
 * Dynamic active-list order, swap-release skipping and auxiliary output follow
 *04E04. Earlier effects remain on error; current-source restores to slot0.
 * Every player's ticks must equal enemies.ticks. Other game counters remain
 * explicit caller inputs. Observers cannot run other frame stages.
 * The generic adapter also dispatches both bosses D7/D8 when bound. */
int ssa_entity_runtime_step(SsaEntityRuntime *,const SsaEntityRuntimeFrame *,
    const SsaEntityRuntimeOps *);
#endif
