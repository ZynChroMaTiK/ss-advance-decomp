#include "ssa/weapon_aim.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,int32_t b) { return bits((uint32_t)a*(uint32_t)b); }
static int32_t asr16(int32_t a) { return bits(((uint32_t)a>>16)|(a<0 ? 0xffff0000u : 0)); }
static int32_t fx(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;
    if (p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }

int ssa_weapon_aim_init(SsaWeaponAim *a,SsaWorld *w,const SsaMathTables *math) {
    size_t cap;
    if (!a || a->updating || a->world || a->entities || a->traversal.intercepts ||
        !w || !w->collision.map || !w->cells || !math || w->query_active ||
        w->capacity>(UINT_MAX-w->collision.map->line_count)/64) return -1;
    cap=w->capacity*64;
    if (ssa_ray_walls_init(&a->walls,w->collision.map)<0) return -1;
    a->entities=calloc(cap ? cap : 1,sizeof(*a->entities));
    a->traversal.capacity=cap+w->collision.map->line_count;
    a->traversal.intercepts=calloc(a->traversal.capacity ? a->traversal.capacity : 1,sizeof(SsaRayIntercept));
    if (!a->entities || !a->traversal.intercepts) {
        free(a->entities); free(a->traversal.intercepts); ssa_ray_walls_free(&a->walls);
        memset(a,0,sizeof(*a)); return -1;
    }
    a->entity_capacity=cap; a->world=w; a->math=math; return 1;
}
int ssa_weapon_aim_free(SsaWeaponAim *a) {
    if (!a || a->updating || a->traversal.updating || a->walls.updating ||
        (a->world && a->world->query_active)) return -1;
    ssa_ray_walls_free(&a->walls); free(a->entities); free(a->traversal.intercepts);
    memset(a,0,sizeof(*a)); return 1;
}
static int collect_entity(SsaWeaponAim *a,SsaRayTraversal *t,SsaEntity *e) {
    int32_t x1=sub(e->body.x,e->radius),x2=add(e->body.x,e->radius),y1,y2,f;
    SsaTraceVector diagonal;
    if (bits((uint32_t)t->ray.dx^(uint32_t)t->ray.dy)>0) {
        y1=add(e->body.y,e->radius); y2=sub(e->body.y,e->radius);
    } else { y1=sub(e->body.y,e->radius); y2=add(e->body.y,e->radius); }
    if (ssa_trace_side(x1,y1,&t->ray)==ssa_trace_side(x2,y2,&t->ray)) return 1;
    diagonal.x=x1; diagonal.y=y1; diagonal.dx=sub(x2,x1); diagonal.dy=sub(y2,y1);
    f=ssa_trace_intercept(&t->ray,&diagonal);
    if (f<0) return 1;
    if (t->count>=t->capacity || a->entity_count>=a->entity_capacity) return -1;
    t->intercepts[t->count].fraction=f;
    t->intercepts[t->count++].line=(unsigned)(a->walls.map->line_count+a->entity_count);
    a->entities[a->entity_count++]=e;
    return 1;
}
static int cell(void *context,SsaRayTraversal *t,int32_t x,int32_t y) {
    SsaWeaponAim *a=context;
    SsaEntity *e;
    size_t seen=0;
    int result=ssa_ray_walls_cell(&a->walls,t,x,y);
    if (result!=1 || x<0 || y<0 || (unsigned)x>=a->world->width || (unsigned)y>=a->world->height) return result;
    for (e=a->world->cells[(size_t)y*a->world->width+(unsigned)x];e;e=e->block_next) {
        if (++seen>a->world->entity_count || e->owner!=a->world) return -1;
        if (collect_entity(a,t,e)<0) return -1;
    }
    return 1;
}
static int predicate(void *context,const SsaRayIntercept *hit) {
    SsaWeaponAim *a=context;
    const SsaMapAssets *m=a->walls.map;
    int32_t distance=fx(a->range,hit->fraction),top,bottom;
    if (hit->line<m->line_count) {
        const uint8_t *l=m->data+m->lines+hit->line*40,*front,*back;
        if (!(u16(l+26)&4)) return 0;
        if (ssa_ray_opening(m,hit->line,&a->opening)<0) return -1;
        if (a->opening.bottom>=a->opening.top) return 0;
        front=m->data+m->sectors+u16(l+12)*20;
        back=m->data+m->sectors+u16(l+14)*20;
        if (u16(front)!=u16(back)) {
            bottom=ssa_trace_divide(sub(a->opening.bottom,a->shoot_z),distance);
            if (bottom>a->bottom_slope) a->bottom_slope=bottom;
        }
        if (u16(front+2)!=u16(back+2)) {
            top=ssa_trace_divide(sub(a->opening.top,a->shoot_z),distance);
            if (top<a->top_slope) a->top_slope=top;
        }
        return a->top_slope>a->bottom_slope;
    } else {
        size_t index=hit->line-m->line_count;
        SsaEntity *e;
        int32_t sum;
        if (index>=a->entity_count) return -1;
        e=a->entities[index];
        if (e==a->source || !(e->body.flags&2)) return 1;
        top=ssa_trace_divide(sub(add(e->body.z,e->body.height),a->shoot_z),distance);
        if (top<a->bottom_slope) return 1;
        bottom=ssa_trace_divide(sub(e->body.z,a->shoot_z),distance);
        if (bottom>a->top_slope) return 1;
        if (top>a->top_slope) top=a->top_slope;
        if (bottom<a->bottom_slope) bottom=a->bottom_slope;
        sum=add(top,bottom);
        a->marker=((uint16_t)(e->type-0xd7)>1 && top>0 && bottom<0) ? 0 : sum/2;
        a->target=e; return 0;
    }
}
typedef struct TraceContext {
    SsaWeaponAim *aim;
    SsaRayPredicate predicate;
    void *context;
} TraceContext;
static int trace_cell(void *context,SsaRayTraversal *t,int32_t x,int32_t y) {
    TraceContext *c=context;
    return cell(c->aim,t,x,y);
}
static int trace_predicate(void *context,const SsaRayIntercept *hit) {
    TraceContext *c=context;
    return c->predicate(c->context,hit);
}
int ssa_weapon_aim_trace(SsaWeaponAim *a,int32_t x,int32_t y,int32_t ex,int32_t ey,
    SsaRayPredicate callback,void *context) {
    TraceContext c={a,callback,context};
    int result;
    if (!a || !callback || !a->world || a->updating || a->world->query_active ||
        a->traversal.updating || a->walls.updating || !a->traversal.intercepts ||
        ssa_ray_walls_begin(&a->walls)<0) return -1;
    a->updating=1; a->world->query_active=1; a->entity_count=0;
    result=ssa_ray_trace(&a->traversal,a->world->origin_x,a->world->origin_y,
        x,y,ex,ey,trace_cell,trace_predicate,&c);
    a->world->query_active=0; a->updating=0;
    return result;
}
int ssa_weapon_aim_query(SsaWeaponAim *a,SsaEntity *source,int32_t sx,int32_t sy,
    int32_t z,uint16_t heading,int32_t range,int32_t *slope) {
    int32_t sine,cosine,x,y;
    int result;
    if (!a || !source || !slope || !a->world || !a->math || a->updating ||
        a->world->query_active || a->traversal.updating || a->walls.updating ||
        !a->traversal.intercepts) return -1;
    a->source=source; a->target=NULL;
    a->entity_count=0; a->shoot_z=add(source->body.z,z); a->range=range;
    a->top_slope=0xa000; a->bottom_slope=-0xa000;
    ssa_sincos(a->math,heading,&sine,&cosine);
    x=add(source->body.x,mul(sine,asr16(sx))); y=add(source->body.y,mul(cosine,asr16(sy)));
    a->end_x=add(source->body.x,mul(sine,asr16(range)));
    a->end_y=add(source->body.y,mul(cosine,asr16(range)));
    a->start_x=x; a->start_y=a->end_y;
    result=ssa_weapon_aim_trace(a,x,y,a->end_x,a->end_y,predicate,a);
    if (result<0) return -1;
    *slope=a->target ? a->marker : 0; return 1;
}
