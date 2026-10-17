#ifndef SSA_PROJECTILE_EFFECT_H
#define SSA_PROJECTILE_EFFECT_H
#include "ssa/projectile_motion.h"
/* Complete E2 attached conversion-effect controller113E4. Source is the stable
 * target, not a firing player. Uses only unlink/insert; storage and target must
 * survive callbacks. Return1 followed,0 request deferred destruction,-1 error.
 * Earlier effects persist on failure. Does not apply health drain/retargeting. */
int ssa_projectile_effect_follow(SsaProjectile *,const SsaProjectileMotionOps *);
#endif
