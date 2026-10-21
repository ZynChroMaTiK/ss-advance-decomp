#ifndef SSA_ENEMY_CONTROL_H
#define SSA_ENEMY_CONTROL_H
#include "ssa/enemy_activation.h"
#include "ssa/enemy_navigation.h"
#include "ssa/enemy_lifecycle.h"
#include "ssa/enemy_motion.h"

typedef struct SsaEnemyControl {
    SsaEnemyActivation *activation;
    SsaEnemyNavigation *navigation;
    uint16_t drain_period, drain_countdown;
    uint8_t animation_done, destroy_requested, bob_direction; /* original +42 */
    unsigned updating;
} SsaEnemyControl;
typedef struct SsaEnemyControlGame {
    SsaEnemyActivationGame *activation;
    SsaEntity *player, *blocking_entity, *loop_source;
    int32_t player_x, player_y;
    uint16_t attack_period[12]; /* 0844BFD8, stride eight; original low halfword */
    unsigned prion_targets; /* PC: allow firing at the selected converted enemy. Default off. */
} SsaEnemyControlGame;
typedef enum SsaEnemyControlEventKind {
    SSA_ENEMY_CONTROL_ANIMATION,
    SSA_ENEMY_CONTROL_SOUND,
    SSA_ENEMY_CONTROL_STOP_SOUND,
    SSA_ENEMY_CONTROL_DAMAGE,
    SSA_ENEMY_CONTROL_GROUP_RELEASE,
    SSA_ENEMY_CONTROL_DESTROY,
    SSA_ENEMY_CONTROL_EXPLOSION,
    SSA_ENEMY_CONTROL_PROJECTILE,
    SSA_ENEMY_CONTROL_MUSIC,
    SSA_ENEMY_CONTROL_NOTICE,
    SSA_ENEMY_CONTROL_KNOCKBACK, /* args impulse,heading; other is player body */
    SSA_ENEMY_CONTROL_SCRIPT /* original1C670 queue request, args[0]=script ID */
} SsaEnemyControlEventKind;
typedef struct SsaEnemyControlEvent {
    SsaEnemyControlEventKind kind;
    SsaEntity *other;
    /* Animation=id,period; damage=amount; projectile=type,forward,side,z_offset,
     * heading,mode (0=10830,1=1097C). Source is control.activation.entity.
     * Effects are synchronous; earlier effects survive a later failure. */
    int32_t args[6];
} SsaEnemyControlEvent;
typedef struct SsaEnemyControlOps {
    const SsaEnemyNavigationOps *navigation;
    /* Full A4F4 auto-aim boundary, NOT a simple line-of-sight query. Return
     * the actual selected entity; NULL is valid no-hit. Native no-hit treats
     * target flags as zero instead of original dereference of BIOS address30.
     * Parameters are source, z offset, heading, range. */
    int (*aim)(void *, const SsaEnemyControl *, int32_t, uint16_t, int32_t,
               SsaEntity **);
    int (*event)(void *, const SsaEnemyControl *, const SsaEnemyControlEvent *);
    void *context;
} SsaEnemyControlOps;

int ssa_enemy_control_periods(SsaEnemyControlGame *, const uint8_t *, size_t);
/* Shared 12638/12C60. Return original boolean (0/1), or -1 on error. */
int ssa_enemy_control_contact(SsaEnemyControl *, SsaEnemyControlGame *,
                              unsigned mode, const SsaEnemyControlOps *);
int ssa_enemy_control_attack(SsaEnemyControl *, SsaEnemyControlGame *,
                             unsigned ticks, const SsaEnemyControlOps *);
/* Complete CF/D0/D1/D2 dispatchers; 0 updated, 1 destruction requested,
 * -1 invalid/callback error. Animation/destruction remain explicit events. */
int ssa_enemy_basic_step(SsaEnemyControl *, SsaEnemyControlGame *,
                          const SsaMathTables *, unsigned ticks, int32_t speed,
                          const SsaEnemyControlOps *);
#endif
