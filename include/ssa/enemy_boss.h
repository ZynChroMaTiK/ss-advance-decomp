#ifndef SSA_ENEMY_BOSS_H
#define SSA_ENEMY_BOSS_H
#include "ssa/enemy_control.h"
#include "ssa/projectile_create.h"
typedef struct SsaEnemyBossRules {
    uint8_t groups[2][2], scripts[2][2]; /* D7, D8; original phase 0/1 */
    unsigned loaded;
} SsaEnemyBossRules;
typedef struct SsaEnemyBoss {
    SsaEnemyControl *control;
    uint8_t animation_index, summon_cycles; /* original +5C, +52 */
} SsaEnemyBoss;
typedef struct SsaEnemyBossOps {
    const SsaEnemyControlOps *control;
    /* Native creation service, not an observer. Stable projection remains
     * borrowed through commit. Pool exhaustion returns NULL: unlike the ROM's
     * subsequent NULL dereference, this is an explicit error. */
    int (*create)(void *,const SsaEnemyBoss *,const SsaProjectileCreateRequest *,SsaProjectile **);
    int (*commit)(void *,const SsaEnemyBoss *,SsaProjectile *);
    void *context;
} SsaEnemyBossOps;
int ssa_enemy_boss_rules_load(SsaEnemyBossRules *,const uint8_t *,size_t);
/* Whole D7 14AB8 and D8 14648. Common game.activation owns boss/phase; alive
 * borrows the VM's 256 group bytes. SCRIPT events queue original script IDs.
 * Callbacks cannot reenter or change actors, players, world or ownership.
 * 0 updated, 1 destruction requested, -1 invalid/effect failure. Prefix effects
 * persist on failure; callers must consume destroy_requested on all exits. */
int ssa_enemy_boss_step(SsaEnemyBoss *,SsaEnemyControlGame *,const SsaEnemyBossRules *,
    const uint8_t *alive,size_t alive_count,const SsaMathTables *,unsigned ticks,const SsaEnemyBossOps *);
#endif
