#include "ssa/enemy_ray.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int32_t s16(const uint8_t *p) { uint16_t v=u16(p); return v<=INT16_MAX ? v : -1-(int32_t)(UINT16_MAX-v); }
static const uint8_t *record(const SsaMapAssets *m,uint32_t off,size_t n,size_t stride) {
    if (!m || !m->data || off>m->size || stride>m->size-off || n>(m->size-off-stride)/stride) return NULL;
    return m->data+off+n*stride;
}
int ssa_ray_opening(const SsaMapAssets *m,unsigned line,SsaRayOpening *o) {
    const uint8_t *l,*a,*b;
    int32_t front,back,af,bf,ac,bc;
    if (!m || !o || line>=m->line_count || !(l=record(m,m->lines,line,40))) return -1;
    if (!(u16(l+26)&4)) { o->range=0; return 1; }
    front=s16(l+12); back=s16(l+14);
    if (front<0 || back<0 || front>=m->sector_count || back>=m->sector_count ||
        !(a=record(m,m->sectors,(unsigned)front,20)) || !(b=record(m,m->sectors,(unsigned)back,20))) return -1;
    af=s16(a); bf=s16(b); ac=s16(a+2); bc=s16(b+2);
    o->top=(ac<bc ? ac : bc)*65536;
    o->bottom=(af>bf ? af : bf)*65536;
    o->low_floor=(af>bf ? bf : af)*65536;
    o->range=bits((uint32_t)o->top-(uint32_t)o->bottom);
    return 1;
}
int ssa_enemy_ray_wall(const SsaMapAssets *m,unsigned line,SsaRayOpening *o) {
    const uint8_t *l;
    int32_t delta;
    if (!m || !o || line>=m->line_count || !(l=record(m,m->lines,line,40))) return -1;
    if (!(u16(l+26)&4)) return 0;
    if (ssa_ray_opening(m,line,o)<0) return -1;
    delta=bits((uint32_t)o->bottom-(uint32_t)o->low_floor);
    if (delta<0) delta=bits(0u-(uint32_t)delta);
    return delta<=0x100000;
}
int ssa_enemy_ray_init(SsaEnemyRay *r,const SsaMapAssets *m) {
    const uint8_t *b;
    if (!r || r->updating || r->walls.map || r->traversal.intercepts || !m ||
        !(b=record(m,m->blockmap,0,8)) || s16(b+4)<=0 || s16(b+6)<=0) return -1;
    if (ssa_ray_walls_init(&r->walls,m)<0) return -1;
    r->traversal.intercepts=calloc(m->line_count ? m->line_count : 1,sizeof(SsaRayIntercept));
    if (!r->traversal.intercepts) { ssa_ray_walls_free(&r->walls); return -1; }
    r->traversal.capacity=m->line_count;
    r->origin_x=s16(b)*65536; r->origin_y=s16(b+2)*65536;
    return 1;
}
int ssa_enemy_ray_free(SsaEnemyRay *r) {
    if (!r || r->updating || r->traversal.updating || r->walls.updating) return -1;
    if (ssa_ray_walls_free(&r->walls)<0) return -1;
    free(r->traversal.intercepts); memset(r,0,sizeof(*r)); return 1;
}
static int cell(void *context,SsaRayTraversal *t,int32_t x,int32_t y) {
    SsaEnemyRay *r=context; return ssa_ray_walls_cell(&r->walls,t,x,y);
}
static int wall(void *context,const SsaRayIntercept *hit) {
    SsaEnemyRay *r=context; return ssa_enemy_ray_wall(r->walls.map,hit->line,&r->opening);
}
int ssa_enemy_ray_query(SsaEnemyRay *r,int32_t x,int32_t y,int32_t ex,int32_t ey) {
    int result;
    if (!r || r->updating || r->traversal.updating || !r->traversal.intercepts ||
        ssa_ray_walls_begin(&r->walls)<0) return -1;
    r->updating=1;
    result=ssa_ray_trace(&r->traversal,r->origin_x,r->origin_y,x,y,ex,ey,cell,wall,r);
    r->updating=0; return result;
}
