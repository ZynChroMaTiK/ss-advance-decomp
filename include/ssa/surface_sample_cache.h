#ifndef SSA_SURFACE_SAMPLE_CACHE_H
#define SSA_SURFACE_SAMPLE_CACHE_H
#include <math.h>
#include <stdint.h>
/* Caller-owned scratch for one immutable scene/row job. Zero-init for each
 * job; never share between workers or retain across palette/map/frame changes.
 * Only unfaded texels (distance <=768) qualify. Fading remains per pixel. */
enum { SSA_SURFACE_SAMPLE_CACHE_SIZE=512 };
typedef struct SsaSurfaceSample {
    int32_t x,y;double height;unsigned tag;int value;
} SsaSurfaceSample;
typedef struct SsaSurfaceSampleCache {SsaSurfaceSample entries[SSA_SURFACE_SAMPLE_CACHE_SIZE];} SsaSurfaceSampleCache;
static inline SsaSurfaceSample *ssa_surface_sample_slot(SsaSurfaceSampleCache *cache,
    double x,double y,double height,double distance,unsigned kind,unsigned color,int *hit)
{
    int32_t tx,ty;uint32_t hash;SsaSurfaceSample *entry;unsigned tag=1+color+kind*256;
    *hit=0;
    if(!cache || !(distance>=0 && distance<=768 && x>-65536 && x<65536 && y>-65536 && y<65536))return 0;
    tx=(int32_t)floor(x/2);ty=(int32_t)floor(y/2);
    hash=(uint32_t)tx*UINT32_C(0x9e3779b9)^(uint32_t)ty*UINT32_C(0x85ebca6b)^tag;
    entry=&cache->entries[(hash^(hash>>16))&(SSA_SURFACE_SAMPLE_CACHE_SIZE-1)];
    *hit=entry->tag==tag && entry->x==tx && entry->y==ty && entry->height==height;
    if(!*hit){entry->tag=tag;entry->x=tx;entry->y=ty;entry->height=height;}
    return entry;
}
#endif
