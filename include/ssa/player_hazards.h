#ifndef SSA_PLAYER_HAZARDS_H
#define SSA_PLAYER_HAZARDS_H
#include "ssa/world.h"
#include "ssa/inventory.h"
#include "ssa/movement.h"

typedef struct SsaPlayerHazards {
    SsaEntity *entity;
    SsaMovement *movement;
    SsaInventory *inventory;
    uint32_t weapon_action; /* selected weapon record +0x10, not weapon ID */
    uint8_t facing;
} SsaPlayerHazards;
typedef struct SsaPlayerHazardGame {
    uint32_t frame_counter;
    const int32_t *hazard_sectors;
    size_t hazard_count; /* original list count is an unsigned byte */
    uint16_t level_counter;
    uint16_t local_flags; /* DISPLAY-local player's flags, not this player's */
} SsaPlayerHazardGame;
typedef struct SsaPlayerHazardOps {
    /* kind 1 = original 1C0D4; kind 2 = original 1C518.
     * May change gameplay fields and map contents; must not replace pointers,
     * change map metadata/layout, free storage, reenter, or touch scratch.
     * Nonnegative success, negative failure. Earlier effects remain on error. */
    int (*line)(void *, SsaPlayerHazards *, unsigned kind, uint16_t line);
    void *context;
} SsaPlayerHazardOps;

/* Original 0429C sweep: unlike movement queries, continues AFTER solid walls.
 * Uses dedicated initialized scratch. Does not move the entity. Returns 0/-1.
 * The ordered special_lines list is its only promised query output. */
int ssa_player_interaction_query(SsaMapCollision *, const SsaEntity *,
                                  uint8_t facing, const SsaMathTables *);
/* Original 03464: one radius sample per frame, then hazard-sector membership.
 * Returns 0/1, or -1 on invalid native inputs/geometry. */
int ssa_player_hazard_sample(const SsaMapAssets *, const SsaEntity *,
                               const SsaPlayerHazardGame *);
/* Original 05548; clears authoritative inventory health and both stats. */
int ssa_player_hazards_apply(SsaPlayerHazards *, const SsaMapAssets *,
                               const SsaPlayerHazardGame *);
/* Whole 05680 including 055BC/05548: interact first, then floor/sample hazards.
 * Requires stable map/entity/player pointers and dedicated scratch; does not
 * execute script animation/audio effects, which remain the line callback.
 * Returns 0/-1. No rollback on callback or later geometry failure. */
int ssa_player_hazards_step(SsaPlayerHazards *, SsaMapCollision *,
                              const SsaMathTables *, const SsaPlayerHazardGame *,
                              const SsaPlayerHazardOps *);
/* PC use policy: type2 buttons require the use key and local-view gate, but
 * do not wait for weapon_action==DC00. The real action value is preserved. */
int ssa_player_hazards_step_immediate_use(SsaPlayerHazards *, SsaMapCollision *,
    const SsaMathTables *, const SsaPlayerHazardGame *, const SsaPlayerHazardOps *);
#endif
