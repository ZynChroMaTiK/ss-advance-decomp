#ifndef SSA_ENEMY_NAVIGATION_H
#define SSA_ENEMY_NAVIGATION_H
#include "ssa/world.h"
#include "ssa/movement.h"

/* Projection of the shared enemy navigator 1288C. These phase/waypoint fields
 * overlap other entity uses: only an appropriate enemy AI state may call it. */
typedef struct SsaEnemyNavigation {
    SsaEntity *entity, *target;
    int32_t speed;
    uint16_t phase, distance, heading, waypoint_x, waypoint_y, target_timer;
    uint8_t desired_facing, move_result, navigation_flags;
    unsigned updating;
} SsaEnemyNavigation;

typedef struct SsaEnemyNavigationOps {
    int (*visibility)(void *, int player_sector, int enemy_sector, int *blocked);
    int (*nearest)(void *, SsaEntity *source, SsaEntity **result);
    int (*path)(void *, const SsaEnemyNavigation *, int32_t x0, int32_t y0,
                int32_t x1, int32_t y1, int32_t radius, unsigned cf_nearest,
                unsigned mode, int *mask, int32_t *offset_x, int32_t *offset_y);
    int (*turn)(void *, SsaEnemyNavigation *, int32_t rate, int32_t *delta);
    int (*move)(void *, SsaEnemyNavigation *, int32_t *result);
    int (*finish)(void *, SsaEnemyNavigation *);
    void *context;
} SsaEnemyNavigationOps;

/* All callbacks return >=0 success / <0 failure. Path returns original 1271C
 * mask 0..3 and 03005708/03005704 offsets. Turn may update externally owned
 * facing; move may update the entity's body through its world API; finish may
 * update body flags. Callbacks must not change navigation fields, target/player,
 * or entity type/radius; other callbacks only report results. Storage,
 * pointers and ownership must remain stable, and callbacks must not reenter.
 * Boundaries represent B18C,036C4,1271C,04614,04498,0354C. enemy_queries supplies
 * the first three; enemy_cd_runtime supplies native turn/walk/hazards.
 * No default no-op substitutes are provided. */
int ssa_enemy_navigation_step(SsaEnemyNavigation *, SsaEntity *player,
                              const SsaMathTables *, unsigned ticks,
                              int32_t speed_factor, const SsaEnemyNavigationOps *);
/* Original 12850. 1 success, -1 invalid/busy. */
int ssa_enemy_navigation_reset(SsaEnemyNavigation *);
/* Read the twelve CD..D8 speed coefficients from 0844BFD4, stride eight. */
int ssa_enemy_navigation_speeds(int32_t out[12], const uint8_t *rom, size_t size);

typedef struct SsaEnemyPath {
    int32_t offset_x, offset_y;
    uint32_t trace_mode, segment_count;
    int32_t segments[10][4]; /* Original 083F0 append buffer. */
    unsigned updating;
} SsaEnemyPath;
typedef int (*SsaEnemyPathRay)(void *, const SsaEnemyPath *, int32_t, int32_t,
                              int32_t, int32_t, int *clear);
/* Whole 1271C and 083F0. Ray callback represents A240 with flags=1 and A041
 * predicate, with trace_mode corresponding to 030056FC. The callback observes
 * but may not mutate/reenter this context. 1 success/-1 invalid or callback
 * error; mask output is 0..3. Effects preceding a failure are retained.
 * cf_nearest records both segments (up to ten) and enables trace_mode before
 * ray queries. Nonzero mode uses four-unit target offsets instead of 1.5radius.
 * enemy_ray supplies the recovered A240/A041 wall-ray implementation. */
int ssa_enemy_path_query(SsaEnemyPath *, const SsaMathTables *, int32_t x0,
                         int32_t y0, int32_t x1, int32_t y1, int32_t radius,
                         unsigned cf_nearest, unsigned mode, SsaEnemyPathRay,
                         void *context, int *mask);
#endif
