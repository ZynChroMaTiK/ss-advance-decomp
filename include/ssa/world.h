#ifndef SSA_WORLD_H
#define SSA_WORLD_H
#include "ssa/geometry.h"
#include "ssa/slide.h"

typedef struct SsaWorld SsaWorld;
/* Host entity, NOT a complete original entity or a GBA binary overlay.
 * Zero-initialize. Keep storage stable while registered. Move X/Y through the
 * world API, not by modifying an inserted body's coordinates. */
typedef struct SsaEntity {
    SsaBody body;
    int32_t radius;
    uint16_t type;
    uint8_t player_index;
    int leaf, sector;
    size_t cell;
    SsaWorld *owner;
    struct SsaEntity *block_next, *block_previous;
    struct SsaEntity *sector_next, *sector_previous;
} SsaEntity;
/* Optional observation of every special encounter, including internal slide
 * retries. Observer must not mutate entities/world or reenter movement. It is
 * not an implementation of the original gameplay handler. */
typedef void (*SsaSpecialContact)(void *, const SsaEntity *, const SsaEntity *);
/* Optional synchronous gameplay dispatch, after observation. Return 0 on
 * success, negative on error. May change external inventory and remove ONLY
 * the encountered special using ssa_world_remove. Retain its storage until
 * the query ends; do not free, insert, move, reenter or mutate other entities.
 * Removing a special ends this cell's traversal, as in the original game. */
typedef int (*SsaSpecialExecute)(void *, const SsaEntity *, SsaEntity *);

struct SsaWorld {
    SsaMapCollision collision;
    SsaEntity **cells, **sectors, **special_contacts;
    size_t entity_count, capacity, special_count;
    int32_t origin_x, origin_y;
    unsigned width, height;
    SsaEntity *blocking_entity;
    SsaSpecialContact observe_special;
    void *contact_context;
    SsaSpecialExecute execute_special;
    void *execute_context;
    unsigned query_active;
    SsaEntity *executing_special;
};

/* Whole 08003D7C predicate, except its 0801B99C call is returned as SPECIAL.
 * 0=blocked, 1=pass, 2=special contact. Preserves original arithmetic/flags,
 * including the flag-0x200 path using current X in its height comparison. */
int ssa_entity_contact(const SsaEntity *current, const SsaEntity *other,
                        int32_t proposed_x, int32_t proposed_y);
/* Map must remain loaded and structurally unchanged. capacity limits registered
 * entities and allocates ordered contact storage. Zero-initialize before init.
 * Free detaches entities but does not free them or the map. */
int ssa_world_init(SsaWorld *world, const SsaMapAssets *map, size_t capacity);
void ssa_world_free(SsaWorld *world);
int ssa_world_insert(SsaWorld *world, SsaEntity *entity);
int ssa_world_remove(SsaWorld *world, SsaEntity *entity);
/* 053D0 sector-only commit. Preserve block-grid membership, floor/ceiling and
 * step adjustment. Relink sector nodes only when requested by CE50's result;
 * geometry sector and the existing sector-list head can intentionally differ.
 * No query/reentry. Returns1/-1 without mutation on invalid input. */
int ssa_world_sector_move(SsaWorld *,SsaEntity *,int32_t x,int32_t y,int32_t z,
                          int leaf,int sector,int relink);
/* Transfer registered storage before gameplay borrowers are bound. Preserves
 * payload, list order and count; old storage is detached. No active query. */
int ssa_world_replace(SsaWorld *, SsaEntity *old_storage, SsaEntity *new_storage);
/* Return 1 clear/success, 0 blocked, -1 invalid. Query visits entities before
 * walls. Special type-0xE1 contacts are reported in order and synchronously
 * dispatched if execute_special is set. Results are overwritten by the next
 * query; contact pointers can include removed entities. Do not reenter queries
 * or mutate the world except as permitted by SsaSpecialExecute. Callback errors
 * propagate as -1; earlier gameplay effects are not rolled back. */
int ssa_world_query(SsaWorld *world, const SsaEntity *entity, int32_t x, int32_t y);
int ssa_world_try_move(SsaWorld *world, SsaEntity *entity, int32_t x, int32_t y);
/* PC launch-clearance extension: skip only this registered entity in this
 * query. All walls, other entities, specials and normal commits remain active.
 * No flags/list membership change and no retained exclusion. NULL is the
 * original try_move behavior. Same return/ownership/reentry contract. */
int ssa_world_try_move_ignoring(SsaWorld *,SsaEntity *,int32_t x,int32_t y,const SsaEntity *ignored);
/* PC enemy crowd movement. Keeps ordinary contacts/geometry, but permits an
 * already overlapping enemy pair to separate monotonically without crossing
 * centers. New overlap and player overlap remain blocked. Broadphase covers
 * original enemy radii up to128 units. No flags or other bodies are changed. */
int ssa_world_try_move_crowd(SsaWorld *,SsaEntity *,int32_t x,int32_t y);
/* PC player collision uses the same128-unit broadphase. An existing overlap
 * with an enemy may only shrink monotonically without crossing its center;
 * new overlap, walls, pickups and spatial commits retain their normal rules. */
int ssa_world_try_move_player(SsaWorld *,SsaEntity *,int32_t x,int32_t y);
/* Full horizontal timestep with recovered wall sliding and dynamic queries.
 * Slide workspace must reference this world's map. Run vertical/jump physics
 * and eye/view updates afterward; this does not run an entire game frame.
 * Return 0 on completed update, -1 on invalid integration/geometry. */
int ssa_world_horizontal_step(SsaWorld *world, SsaEntity *entity, SsaSlide *slide,
                               const SsaMathTables *tables, uint8_t ticks,
                               uint8_t player_count, uint8_t local_player);
int ssa_world_horizontal_step_player(SsaWorld *,SsaEntity *,SsaSlide *,const SsaMathTables *,
                               uint8_t ticks,uint8_t player_count,uint8_t local_player);
#endif
