#ifndef SSA_MAP_ASSETS_H
#define SSA_MAP_ASSETS_H
#include <stddef.h>
#include <stdint.h>

/* Host-owned decoded map. All offsets remain relative to data; no GBA pointers.
 * Initialize to zero before loading and free before reusing a populated object.
 * Texture bindings, BSP parent links and entity allocation are not initialized. */
typedef struct SsaMapAssets {
    uint8_t *data;
    size_t size;
    uint16_t thing_count, texture_count, sector_count, node_count, leaf_count;
    uint16_t segment_count, side_count, line_count, vertex_count;
    uint32_t things, textures, sectors, nodes, leaves, segments, sides, lines;
    uint32_t vertices, reject, blockmap, rom_offset;
} SsaMapAssets;

/* GBA type-0x10 LZ77, with explicit input/output bounds. Output may be partially
 * written on failure. consumed is optional and set only on success. */
int ssa_lz77_decode(const uint8_t *src, size_t src_size, uint8_t *dst,
                    size_t dst_capacity, size_t *consumed);
/* AENP ROM resource table, levels 0..15. ROM identity is the caller's concern.
 * Returns zero on invalid/truncated data or allocation failure. Does not alter
 * out on failure. Successful loading computes original line collision fields. */
int ssa_map_assets_load(SsaMapAssets *out, const uint8_t *rom,
                        size_t rom_size, unsigned level);
void ssa_map_assets_free(SsaMapAssets *map);
#endif
