#ifndef SSA_MOVEMENT_H
#define SSA_MOVEMENT_H
#include <stddef.h>
#include <stdint.h>

/* Tables are data from the user's AENP ROM. No ROM instructions are executed. */
typedef struct SsaMathTables {
    uint16_t sine_quarter[16384];
    uint16_t direction[32769];
} SsaMathTables;
int ssa_math_tables_load(SsaMathTables *tables, const uint8_t *rom, size_t size);
void ssa_sincos(const SsaMathTables *tables, uint16_t heading,
                int32_t *sine, int32_t *cosine);
uint16_t ssa_direction(const SsaMathTables *tables, int16_t x, int16_t y);

/* Recovered fields, deliberately independent of GBA structure offsets. */
typedef struct SsaMovement {
    uint32_t keys, weapon_flags;
    int32_t turn_velocity, forward_velocity, strafe_velocity, impulse;
    int32_t velocity_x, velocity_y;
    uint16_t heading, move_heading, flags, modifier_ticks, impulse_heading;
} SsaMovement;

/* Control/vector phase of 0x080064B8. The caller supplies the already computed
 * inter-player impulse, if any. Complete body synchronization, multiplayer
 * pushing and physics integration now live in player_motion.h; camera bob
 * lives in physics.h. These two functions remain the original phase API.
 * ticks is the original game-state byte at +7, not seconds or milliseconds. */
void ssa_movement_controls(SsaMovement *state, uint8_t ticks);
void ssa_movement_vector(SsaMovement *state, const SsaMathTables *tables);
#endif
