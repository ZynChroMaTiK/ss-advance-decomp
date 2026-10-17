#ifndef SSA_SECTOR_CLIP_H
#define SSA_SECTOR_CLIP_H
#include "ssa/world.h"

/* Original03003F10 scratch, with map-relative records and no ROM execution.
 * Dedicated call-local state; does not overwrite movement/ray workspaces. */
typedef struct SsaSectorClip {
    int32_t x,y,z,end_x,end_y,end_z,radius,distance;
    uint16_t line,leaf,sector;
} SsaSectorClip;
/* OriginalD000 / CE50. Return candidate/correction count, -1 malformed/loop.
 * Failures leave state unchanged. Current live sector heights are borrowed. */
int ssa_sector_clip_find(const SsaMapAssets *,const SsaMathTables *,SsaSectorClip *);
int ssa_sector_clip(const SsaMapAssets *,const SsaMathTables *,SsaSectorClip *);
/* Original053D0 geometry: add velocity once, radius32, clip, then clamp Z to
 * the destination sector (without subtracting height). Return0/1 or -1.
 * Caller applies final XYZ/leaf/sector and relinks SECTOR only on result1. */
int ssa_sector_relocate(const SsaMapAssets *,const SsaMathTables *,const SsaEntity *,SsaSectorClip *);
#endif
