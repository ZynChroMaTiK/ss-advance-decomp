#ifndef SSA_ENEMY_CONTROL_RUNTIME_H
#define SSA_ENEMY_CONTROL_RUNTIME_H
#include "ssa/enemy_control.h"
#include "ssa/enemy_boss.h"
#include "ssa/enemy_queries.h"
#include "ssa/projectile_runtime.h"
#include "ssa/weapon_aim.h"

typedef struct SsaEnemyControlRuntime {
    SsaEnemyBossRules boss_rules;
    SsaEnemyBoss bosses[100];
    SsaCombatRuntime *combat;
    SsaProjectileRuntime *projectiles;
    SsaEnemyQueries *queries;
    const SsaMathTables *math;
    SsaEnemyControlGame game;
    SsaEnemyControl controllers[100];
    SsaEnemyNavigation navigation[100];
    uint8_t active[100];
    SsaWeaponAim aim;
    uint32_t *walk_count;
    unsigned updating;
    unsigned crowd_slide; /* PC policy shared by CE..D8 walking controllers. */
    unsigned converted_combat; /* PC: converted-attacker contact and selected shot targeting. */
    SsaEnemyCrowd crowd[100];
} SsaEnemyControlRuntime;
typedef struct SsaEnemyControlRuntimeOps {
    const SsaCombatRuntimeOps *combat;
    /* Animation metadata is updated before this observer. Audio/music/notice
     * are presentation effects. Damage/projectiles/groups/destruction are
     * performed natively and are not forwarded. No callback may mutate bound
     * runtime/world/entities or reenter. Return >=0 success, <0 failure. */
    int (*presentation)(void *,const SsaEnemyControl *,const SsaEnemyControlEvent *);
    void *context;
} SsaEnemyControlRuntimeOps;
/* Zero-init; stable address; combat/projectiles/queries must share spawn/world.
 * walk_count is shared movement-call counter. game.player/player_x/player_y
 * must be refreshed with original selected player before every step. Init
 * selects combat.players[0]. Bind lazily on first step to owned state-zero
 * CE..D6 actors (including adopted map slots). CD uses its existing adapter.
 * Runtime owns only aim scratch; free before combat/spawn/world teardown. */
int ssa_enemy_control_runtime_init(SsaEnemyControlRuntime *,SsaCombatRuntime *,
    SsaProjectileRuntime *,SsaEnemyQueries *,const SsaMathTables *,
    uint32_t *walk_count,const uint8_t *rom,size_t size);
int ssa_enemy_control_runtime_free(SsaEnemyControlRuntime *);
/* 0 updated,1 released,-1 invalid/effect failure. Frame.speed_factor must be
 * coefficient for this actor's actual type. Effects before failure persist. */
int ssa_enemy_control_runtime_step(SsaEnemyControlRuntime *,unsigned slot,
    const SsaEnemyCdFrame *,const SsaEnemyControlRuntimeOps *);
#endif
