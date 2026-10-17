#ifndef SSA_LEVEL_SCRIPT_H
#define SSA_LEVEL_SCRIPT_H
#include <stddef.h>
#include <stdint.h>

/* ROM addresses below identify DATA, never executable native pointers. The
 * immutable ROM buffer must outlive this view and every VM using it. */
typedef struct SsaLevelScriptData {
    const uint8_t *rom;
    size_t size;
    uint32_t table[16];
    uint16_t count[16];
} SsaLevelScriptData;
typedef struct SsaLevelScript {
    uint32_t pc[5], state[5][5], count;
    /* Real scripts use spawn group zero, incrementing byte before alive[].
     * Keep that original stray byte explicitly, without host out-of-bounds. */
    uint8_t alive_before, alive[256], visited[256];
    uint16_t game_state, exit_toggle;
    unsigned updating;
} SsaLevelScript;
/* Known fields written by the VM. The allocator owns this stable object;
 * animation and insertion bridge it to the host entity/world representation.
 * They may update these fields, as the original callees can. */
typedef struct SsaLevelScriptEntity {
    int32_t x, y, z;
    uint16_t type, flags, trigger, timer, floor;
    uint8_t variant, facing;
} SsaLevelScriptEntity;
typedef struct SsaLevelScriptOps {
    SsaLevelScriptEntity *(*allocate)(void *);
    int (*animation)(void *, SsaLevelScriptEntity *, unsigned id, unsigned ticks);
    int (*insert)(void *, SsaLevelScriptEntity *);
    int (*particle)(void *, unsigned id, int32_t x, int32_t y, int32_t z);
    /* Opcodes 4=open, 5=close, 6=unlock, 7=lock; argument is mover index. */
    int (*mover)(void *, unsigned opcode, uint32_t argument);
    void *context;
} SsaLevelScriptOps;
/* Return 1 success, 0 invalid. Empty script tables on maps 12..15 are valid. */
int ssa_level_script_data_load(SsaLevelScriptData *, const uint8_t *, size_t);
/* 1C5A8, except allocation is owned inline. Zero-initialize before first use.
 * Like the ROM, reset preserves state words 3/4 and game/exit fields. */
int ssa_level_script_reset(SsaLevelScript *);
int ssa_level_script_queue(SsaLevelScript *, const SsaLevelScriptData *, unsigned level, unsigned id);
/* 1C624. sector_flags is original sector+10; visited set even without trigger.
 * Invalid queue requests fail before visited is changed. */
int ssa_level_script_enter(SsaLevelScript *, const SsaLevelScriptData *, unsigned level,
                           unsigned sector, uint16_t sector_flags);
/* 1C6B0: tag low byte 0 or bit 100 prevents decrement; otherwise uint8 wraps.
 * Original uses the complete tag as index; native rejects out-of-range tags. */
int ssa_level_script_release(SsaLevelScript *, uint16_t tag);
/* Whole 1C6DC dispatch, one instruction per active slot per call. ticks<=255.
 * Return 0 completed, -1 invalid/callback error. Effects already applied remain.
 * Callbacks return >=0 success/<0 error; allocator NULL is normal pool exhaustion.
 * No VM mutation/reentry from callbacks. Out-of-ROM data is rejected on access.
 * On allocation failure original particle Z is incoming/stale r10: fallback_z
 * explicitly supplies it until a successful spawn sets the frame's last Z. */
int ssa_level_script_step(SsaLevelScript *, const SsaLevelScriptData *, unsigned ticks,
                          int32_t fallback_z, const SsaLevelScriptOps *);
#endif
