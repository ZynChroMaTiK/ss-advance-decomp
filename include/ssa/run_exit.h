#ifndef SSA_RUN_EXIT_H
#define SSA_RUN_EXIT_H
#include "ssa/player_lifecycle.h"
enum { SSA_RUN_RETRY=1,SSA_RUN_SCORES=2,SSA_RUN_MENU=3 };
/* Original02564 non-completion handoff after its black fade (02A7A..02B52).
 * Input4 consumes one remaining life and retries, or opens name entry at zero.
 * Input5 returns to Main. The host owns fade, level rebuilding and menu setup.
 * Returns the destination above, or-1 without changes for an unsupported mode.
 * Both state projections must be distinct, idle and exclusively owned. */
int ssa_run_exit(SsaPlayerLifecycleGame *,uint16_t *flags);
#endif
