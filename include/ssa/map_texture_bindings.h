#ifndef SSA_MAP_TEXTURE_BINDINGS_H
#define SSA_MAP_TEXTURE_BINDINGS_H
#include "ssa/map_assets.h"
#include "ssa/texture_assets.h"

typedef struct SsaMapTextureRecord {
    char name[8];
    uint16_t width,height;
    int32_t texture_index; /* Original descriptor+C after B0C4, sign-extended. */
} SsaMapTextureRecord;
typedef struct SsaMapTextureBindings {
    const SsaMapAssets *map;
    const SsaTextureAssets *assets;
    SsaMapTextureRecord *records;
    uint16_t count;
    uint8_t family;
} SsaMapTextureBindings;
enum SsaMapSideTexture {SSA_MAP_SIDE_UPPER,SSA_MAP_SIDE_MIDDLE,SSA_MAP_SIDE_LOWER};

/* Whole original B0C4/B0FC binding phase. First case-insensitive eight-byte
 * match supplies dimensions and signed16 archive index. Missing names become
 * index0 while retaining the incoming dimensions. The original temporarily
 * copies a pixel pointer before replacing it with the index; no pointer is
 * retained here. Return1/-1; invalid arguments leave records unchanged. */
int ssa_map_texture_bind_records(SsaMapTextureRecord *,size_t count,const SsaTextureArchive *);
/* Zero-init output. Owns only a descriptor copy; never changes decoded map or
 * asset pixels. Borrows stable map/assets until free, including all arrays.
 * Caller selects the original family with ssa_texture_assets_family(level).
 * Missing-name fallback is preserved, including zero dimensions in real maps.
 * Return1/-1, atomic on failure; free before reuse. */
int ssa_map_texture_bindings_init(SsaMapTextureBindings *,const SsaMapAssets *,const SsaTextureAssets *,unsigned family);
void ssa_map_texture_bindings_free(SsaMapTextureBindings *);
const SsaMapTextureRecord *ssa_map_texture_record(const SsaMapTextureBindings *,unsigned map_index);
const SsaTextureImage *ssa_map_texture_image(const SsaMapTextureBindings *,unsigned map_index,unsigned mip);
/* Read CURRENT side+4/+6/+8 every call so native switch changes remain visible.
 * Return1 with a borrowed record,0 with NULL for original -1 absent,-1 for
 * malformed/invalid (output unchanged). No side/sector index cache. Sector
 * bytes+A/+B are flat-color/sky selectors, not map texture indices. */
int ssa_map_side_texture(const SsaMapTextureBindings *,unsigned side,unsigned layer,const SsaMapTextureRecord **);
#endif
