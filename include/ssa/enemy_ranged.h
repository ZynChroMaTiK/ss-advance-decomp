#ifndef SSA_ENEMY_RANGED_H
#define SSA_ENEMY_RANGED_H
#include "ssa/enemy_control.h"
/* Whole CE135C4, D3 13FA4, D4 141E8, D5 13DB0 and D6 13780.
 * The name groups this recovery, not the enemy attack category: CE is melee
 * and D5 burrows. Shared control/game/event contracts are in enemy_control.h.
 * D5 uses navigation.move for direct04498 walks; D4 uses navigation.finish
 * for0354C hazards when high above the player. These callbacks must preserve
 * unrelated navigation fields just as in the shared navigator.
 * CE loop_source corresponds to03004674, distinct from CD's03004670.
 * 0 updated,1 destruction requested,-1 error. Earlier effects persist; after
 * a DESTROY request storage must stay stable until return, including errors.
 */
int ssa_enemy_ranged_step(SsaEnemyControl *,SsaEnemyControlGame *,
    const SsaMathTables *,unsigned ticks,int32_t speed,const SsaEnemyControlOps *);
#endif
