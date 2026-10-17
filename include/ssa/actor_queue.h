#ifndef SSA_ACTOR_QUEUE_H
#define SSA_ACTOR_QUEUE_H
#include "ssa/world_visibility.h"
#include "ssa/actor_assets.h"
#include "ssa/wall_renderer.h"

/* Original 36-byte 03004754 projection record. These scalar fields are an
 * exact overlay; no GBA pointer is retained. frame_flags bit31 is horizontal
 * flip; original image indexing discards bit30 as well. Bits0..29 select the
 * actor image frame (normal D490 outputs are at most3000). */
typedef struct SsaActorProjection {
    uint32_t reserved;
    int16_t x,y,width,height;
    uint32_t frame_flags;
    int32_t scale;
    int16_t world_x,world_y,bottom_z,top_z;
    uint16_t animation,clip_offset;
    int16_t left,clipped_width;
} SsaActorProjection;
/* Original 12-byte mixed-list actor record. Indices in prev/next/head/tail
 * use bit8000 for actor records and FFFF for end. */
typedef struct SsaActorRecord {
    uint16_t previous,next;
    int32_t depth;
    uint16_t projection,clip_offset;
} SsaActorRecord;
typedef struct SsaActorWallClip {
    uint32_t flags;
    int16_t bottom_height,top_height;
    const uint8_t *top,*bottom;
    size_t top_size,bottom_size;
    int16_t top_x,bottom_x;
} SsaActorWallClip;
enum { SSA_ACTOR_QUEUE_LIMIT=32, SSA_ACTOR_CLIP_WORDS=32*120*2+4 };
typedef struct SsaActorQueue {
    SsaWorldVisibility *visibility;
    SsaActorWallClip wall_clips[SSA_VISIBILITY_WALL_LIMIT];
    SsaActorProjection projections[SSA_ACTOR_QUEUE_LIMIT];
    SsaActorRecord actors[SSA_ACTOR_QUEUE_LIMIT];
    int16_t clip_words[SSA_ACTOR_CLIP_WORDS];
    uint16_t draw_order[SSA_ACTOR_QUEUE_LIMIT];
    uint16_t projection_count,actor_count,draw_count;
    uint32_t clip_cursor;
    unsigned updating;
} SsaActorQueue;

/* Begin after fresh visibility construction and wall drawing. Borrows the
 * stable wall queue; mutates its links/head/tail when inserting actors. Reset
 * counts/cursor only, retain record and clip backing bytes. Return1/-1. */
int ssa_actor_queue_begin(SsaActorQueue *,SsaWorldVisibility *);
/* Bind actual BE60 outputs. Closed clips use the actor asset loader's121-byte
 * ROM windows; opening clips expose their remaining stable backing capacity,
 * retaining original reads just beyond a copied span. Only linked walls bind. */
int ssa_actor_queue_bind_clips(SsaActorQueue *,const SsaWallRenderResult *,size_t count,
    const SsaWallRenderer *,const SsaActorAssets *);
/* Original F98: returns new actor index0..31, or-1 when full/invalid. D490
 * separately allocates its projection record and writes depth/projection. */
int ssa_actor_queue_allocate(SsaActorQueue *);
/* Complete1238, including B24C geometric side test, signed clip-byte merges,
 * shared clipword allocation, depth/tie ordering and mixed-list insertion.
 * wall_clips must expose readable backing beyond copied opening spans where
 * the original inclusive right endpoint reads a following/stale byte. Native
 * bounds never invent that byte. Return1/-1; earlier effects survive errors. */
int ssa_actor_queue_insert(SsaActorQueue *,unsigned actor);
/* Original01828 collection phase: gathers actorindices in forward mixed-list
 * order. Drawing consumes draw_order backwards. Return1/-1; guarded cycles,
 * malformed indices and duplicates fail instead of looping. */
int ssa_actor_queue_order(SsaActorQueue *);
/* Complete01828: gather then draw actors in reverse via native09238/09258.
 * Uses actual decoded actor resources. Previous actors' pixels remain after a
 * later error; storage cannot alias the framebuffer. Return1/-1. */
int ssa_actor_queue_draw(SsaActorQueue *,const SsaActorAssets *,uint8_t *framebuffer,size_t size);
#endif
