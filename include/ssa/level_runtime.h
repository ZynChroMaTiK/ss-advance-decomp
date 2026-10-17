#ifndef SSA_LEVEL_RUNTIME_H
#define SSA_LEVEL_RUNTIME_H
#include "ssa/player_gameplay.h"
#include "ssa/sector_movers.h"
#include "ssa/switches.h"
#include "ssa/level_script.h"

typedef struct SsaLevelRuntime {
    SsaMapAssets *map;
    SsaSectorMovers movers;
    SsaSwitches switches;
    SsaLevelScriptData data;
    SsaLevelScript script;
    int32_t hazard_sectors[8];
    unsigned level, updating;
} SsaLevelRuntime;
typedef struct SsaLevelPlayerOps {
    int (*event)(void *, SsaPlayerGameplay *, enum SsaPlayerLifecycleEvent,
                 int32_t, int32_t, int32_t, int32_t);
    int (*animation)(void *, SsaPlayerRuntime *, unsigned, unsigned);
    int (*sound)(void *, SsaPlayerRuntime *, unsigned);
    void *context;
} SsaLevelPlayerOps;
/* Zero-init out. Initializes movers (including map heights), switches and VM.
 * Call before player/entity construction. Owns switch records, borrows map and
 * immutable ROM; both must outlive it. Returns0/-1. Free before map/ROM release.
 * No main loop, rendering or entity AI is provided here. script_spawn.h supplies
 * the optional native script-spawn allocator/insertion adapter. */
int ssa_level_runtime_init(SsaLevelRuntime *, SsaMapAssets *, const uint8_t *, size_t,
                            unsigned level);
int ssa_level_runtime_free(SsaLevelRuntime *);
/* Native line/sector adapters. First sector entry queues the original tag.
 * Sound callback may be NULL. No structural/state mutation in sound callbacks.
 * Return0/-1; earlier changes persist on error. All APIs reject reentry. */
int ssa_level_runtime_line(SsaLevelRuntime *, unsigned kind, unsigned line,
                            const SsaSectorMoverOps *);
int ssa_level_runtime_sector(SsaLevelRuntime *, unsigned sector);
/* Replaces player_gameplay's line/sector boundaries with actual movers,
 * switches and script enqueueing. Temporarily supplies the live mover hazard
 * list, restoring caller's hazard list afterwards; caller supplies other game
 * counters. Presentation callbacks remain required. Returns0/-1.
 * Run for each player/entity BEFORE the once-per-world update below. */
int ssa_level_runtime_player_step(SsaLevelRuntime *, SsaPlayerGameplay *,
                                   const SsaPlayerRuntimeInput *, const SsaLevelPlayerOps *);
/* Original0244C order after entity updates: mover heights first, then one VM
 * instruction per active slot. VM mover opcodes route to native mover commands;
 * script_ops.mover is ignored. Allocation/animation/insertion/particles remain
 * required external callbacks. Call once per simulation frame, not per player.
 * ticks is0..255; fallback_z specifies original undefined allocation-failure Z.
 * Original pickup-respawn/audio/render tail remains outside this function.
 * Return0/-1; failures preserve earlier effects and clear busy state. */
int ssa_level_runtime_step(SsaLevelRuntime *, unsigned ticks, int32_t fallback_z,
                            const SsaLevelScriptOps *, const SsaSectorMoverOps *);
/* Separate original frame stages. Use these OR the combined step above, once
 * per frame: movers first, scripts second. Both enforce the same level guard.
 * Script mover commands still execute synchronously during the script stage. */
int ssa_level_runtime_movers_step(SsaLevelRuntime *, unsigned ticks,
                                  const SsaSectorMoverOps *);
int ssa_level_runtime_scripts_step(SsaLevelRuntime *, unsigned ticks, int32_t fallback_z,
                                   const SsaLevelScriptOps *, const SsaSectorMoverOps *);
#endif
