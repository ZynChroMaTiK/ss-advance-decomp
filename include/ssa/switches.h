#ifndef SSA_SWITCHES_H
#define SSA_SWITCHES_H
#include "ssa/map_assets.h"

typedef struct SsaSwitchEntry {
    uint16_t line;
    uint8_t off_texture, on_texture;
    uint16_t flags, type, trigger, reserved;
} SsaSwitchEntry;
typedef struct SsaSwitches {
    SsaMapAssets *map;
    SsaSwitchEntry *entries;
    size_t count;
    unsigned updating;
} SsaSwitches;
typedef struct SsaSwitchOps {
    int (*enqueue)(void *, unsigned trigger);
    int (*sound)(void *, unsigned sound);
    void *context;
} SsaSwitchOps;
/* Original 1C3B4/1C374. Zero-init out. Owns records, borrows stable mutable map.
 * Four switch texture names come from the user's ROM, not hardcoded art.
 * Native allocation starts at zero; original allocator leaves unspecified bytes.
 * Texture indices are intentionally truncated to bytes. Unknown texture names
 * leave both texture indices and flags zero. A known name with a missing pair,
 * invalid side/texture index or allocation failure returns -1 without binding.
 * Returns 0 success. Map and ROM layout must be valid for AENP. */
int ssa_switches_init(SsaSwitches *, SsaMapAssets *, const uint8_t *rom, size_t size);
int ssa_switches_free(SsaSwitches *);
/* Original 1C518: first matching record, latch flag1, set front middle texture,
 * enqueue trigger then emit sound41. Already on is a no-op. Both callbacks
 * required; >=0 success/<0 failure. Previous effects remain after failure.
 * No reentry/free/pointer replacement or map layout mutation in callbacks.
 * Returns 0/-1. Missing record is rejected instead of original undefined or
 * unintended last-record access. Original valid callers pass registered lines. */
int ssa_switch_activate(SsaSwitches *, unsigned line, const SsaSwitchOps *);
#endif
