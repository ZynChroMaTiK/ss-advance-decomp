#include "ssa/ray_intercepts.h"
#include "ssa/slide.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) {
    uint16_t v=u16(p);
    return v<=INT16_MAX ? (int16_t)v : (int16_t)(-1-(int32_t)(UINT16_MAX-v));
}
static const uint8_t *record(const SsaMapAssets *m,uint32_t off,size_t n,size_t stride) {
    if (!m || !m->data || off>m->size || stride>m->size-off || n>(m->size-off-stride)/stride) return NULL;
    return m->data+off+n*stride;
}
static int valid(const SsaRayWallCollector *c) {
    return c && c->map && c->map->data && c->visited && c->map->line_count<=c->capacity;
}
int ssa_ray_walls_init(SsaRayWallCollector *c,const SsaMapAssets *m) {
    if (!c || !m || !m->data || c->updating || c->visited || c->map) return -1;
    memset(c,0,sizeof(*c));
    c->visited=calloc(m->line_count ? m->line_count : 1,1);
    if (!c->visited) return -1;
    c->map=m; c->capacity=m->line_count;
    return 1;
}
int ssa_ray_walls_free(SsaRayWallCollector *c) {
    if (!c || c->updating) return -1;
    free(c->visited); memset(c,0,sizeof(*c)); return 1;
}
int ssa_ray_walls_begin(SsaRayWallCollector *c) {
    if (!valid(c) || c->updating) return -1;
    memset(c->visited,0,c->map->line_count); return 1;
}
static int collect(SsaRayWallCollector *c,SsaRayTraversal *r,unsigned index) {
    const SsaMapAssets *m=c->map;
    const uint8_t *l,*v1,*v2;
    SsaTraceVector t={r->ray.x,r->ray.y,r->ray.dx,r->ray.dy},wall;
    int a,b;
    int32_t fraction;
    if (index>=m->line_count || !(l=record(m,m->lines,index,40))) return -1;
    if (c->visited[index]) return 1;
    c->visited[index]=1;
    if (u16(l)>=m->vertex_count || u16(l+2)>=m->vertex_count ||
        !(v1=record(m,m->vertices,u16(l),4)) || !(v2=record(m,m->vertices,u16(l+2),4))) return -1;
    if (t.dx>0x100000 || t.dy>0x100000 || t.dx<-0x100000 || t.dy<-0x100000) {
        a=ssa_trace_side((int32_t)s16(v1)*65536,(int32_t)s16(v1+2)*65536,&t);
        b=ssa_trace_side((int32_t)s16(v2)*65536,(int32_t)s16(v2+2)*65536,&t);
    } else {
        a=ssa_line_side(t.x,t.y,s16(v2),s16(v2+2),s16(l+4),s16(l+6));
        b=ssa_line_side(add(t.x,t.dx),add(t.y,t.dy),s16(v2),s16(v2+2),s16(l+4),s16(l+6));
    }
    if (a==b) return 1;
    wall.x=(int32_t)s16(v1)*65536; wall.y=(int32_t)s16(v1+2)*65536;
    wall.dx=(int32_t)s16(l+4)*65536; wall.dy=(int32_t)s16(l+6)*65536;
    fraction=ssa_trace_intercept(&t,&wall);
    if ((uint32_t)fraction<=65536) {
        if (r->count>=r->capacity) return -1;
        r->intercepts[r->count].fraction=fraction;
        r->intercepts[r->count++].line=index;
    }
    return 1;
}
static int ready(SsaRayWallCollector *c,const SsaRayTraversal *r) {
    return valid(c) && !c->updating && r && r->intercepts && r->count<=r->capacity;
}
int ssa_ray_walls_line(SsaRayWallCollector *c,SsaRayTraversal *r,unsigned line) {
    int result;
    if (!ready(c,r)) return -1;
    c->updating=1; result=collect(c,r,line); c->updating=0; return result;
}
int ssa_ray_walls_cell(void *context,SsaRayTraversal *r,int32_t x,int32_t y) {
    SsaRayWallCollector *c=context;
    const uint8_t *blocks;
    size_t words,cursor;
    int width,height,result=1;
    if (!ready(c,r) || c->map->size<0x48 || !(blocks=record(c->map,c->map->blockmap,0,8))) return -1;
    words=u16(c->map->data+0x1a);
    if (words<4 || words>(c->map->size-c->map->blockmap)/2) return -1;
    width=s16(blocks+4); height=s16(blocks+6);
    if (width<=0 || height<=0 || (size_t)width*(unsigned)height>words-4) return -1;
    if (x<0 || y<0 || x>=width || y>=height) return 1;
    c->updating=1;
    cursor=(size_t)u16(blocks+2*(4+(size_t)y*(unsigned)width+(unsigned)x))+1;
    for (;;) {
        uint16_t index;
        if (cursor>=words) { result=-1; break; }
        index=u16(blocks+2*cursor++);
        if (index==UINT16_MAX) break;
        result=collect(c,r,index);
        if (result!=1) break;
    }
    c->updating=0; return result;
}
