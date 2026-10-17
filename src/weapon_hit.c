#include "ssa/weapon_hit.h"
#include <limits.h>
#include <string.h>

static int32_t bits(uint32_t n) { return n<=INT32_MAX ? (int32_t)n : -1-(int32_t)~n; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,int32_t b) { return bits((uint32_t)a*(uint32_t)b); }
static int32_t whole(int32_t a) { return bits(((uint32_t)a>>16)|(a<0 ? 0xffff0000u : 0)); }
static int32_t fx(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;
    if(p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static unsigned u16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
typedef struct HitCall { SsaWeaponAim *trace; SsaWeaponHit *hit; int manual,sector; int32_t entered; } HitCall;
static void impact(HitCall *c,int32_t fraction,SsaEntity *target) {
    SsaWeaponAim *a=c->trace;
    c->hit->impact=1;c->hit->target=target;
    c->hit->x=add(a->traversal.ray.x,fx(a->traversal.ray.dx,fraction));
    c->hit->y=add(a->traversal.ray.y,fx(a->traversal.ray.dy,fraction));
    c->hit->z=add(a->shoot_z,fx(a->marker,fx(fraction,a->range)));
}
static int plane_hit(HitCall *c,int32_t end) {
    SsaWeaponAim *a=c->trace;const uint8_t *s;
    int64_t dz=(int64_t)a->range*a->marker/65536,delta,fraction;
    if(!c->manual || !dz || c->sector<0 || (unsigned)c->sector>=a->walls.map->sector_count)return 0;
    s=a->walls.map->data+a->walls.map->sectors+(size_t)c->sector*20;
    delta=(int64_t)(int16_t)u16(s+(dz>0?2:0))*65536-a->shoot_z;
    fraction=delta*65536/dz;
    if(fraction<c->entered || fraction>end)return 0;
    impact(c,(int32_t)fraction,NULL);return 1;
}
static int predicate(void *context,const SsaRayIntercept *h) {
    HitCall *c=context;SsaWeaponAim *a=c->trace;
    const SsaMapAssets *m=a->walls.map;
    int32_t distance=fx(a->range,h->fraction);
    if(plane_hit(c,h->fraction))return 0;
    if(h->line<m->line_count) {
        const uint8_t *l=m->data+m->lines+h->line*40;
        if(u16(l+26)&4) {
            const uint8_t *front,*back;
            if(ssa_ray_opening(m,h->line,&a->opening)<0) return -1;
            front=m->data+m->sectors+u16(l+12)*20;
            back=m->data+m->sectors+u16(l+14)*20;
            if(u16(front)!=u16(back) &&
               ssa_trace_divide(sub(a->opening.bottom,a->shoot_z),distance)>a->marker) goto wall;
            if(u16(front+2)!=u16(back+2) &&
               ssa_trace_divide(sub(a->opening.top,a->shoot_z),distance)<a->marker) goto wall;
            if(c->manual){unsigned front_sector=u16(l+12),back_sector=u16(l+14);
                c->sector=(int)((unsigned)c->sector==front_sector?back_sector:front_sector);c->entered=h->fraction;}
            return a->top_slope>a->bottom_slope;
        }
wall:
        impact(c,c->manual==2?h->fraction:sub(h->fraction,ssa_trace_divide(0x40000,a->range)),NULL);
        return 0;
    } else {
        size_t index=h->line-m->line_count;SsaEntity *e;
        if(index>=a->entity_count) return -1;
        e=a->entities[index];
        if(e==a->source || !(e->body.flags&2)) return 1;
        if(ssa_trace_divide(sub(add(e->body.z,e->body.height),a->shoot_z),distance)<a->marker ||
           ssa_trace_divide(sub(e->body.z,a->shoot_z),distance)>a->marker) return 1;
        impact(c,c->manual==2?h->fraction:sub(h->fraction,fx(h->fraction,0x1999)),e);
        return 0;
    }
}
int ssa_weapon_hit_query(SsaWeaponAim *a,SsaEntity *source,uint8_t facing,
    int32_t sx,int32_t sy,int32_t z,int32_t range,int32_t slope,SsaWeaponHit *hit) {
    int32_t sine,cosine,x,y,ex,ey;int result;HitCall call={a,hit,0,0,0};
    if(!a || !a->world || !a->math || !source || !hit || a->updating ||
       a->world->query_active || a->traversal.updating || a->walls.updating) return -1;
    ssa_sincos(a->math,(uint16_t)((unsigned)facing<<8),&sine,&cosine);
    x=add(source->body.x,mul(sine,whole(sx)));y=add(source->body.y,mul(cosine,whole(sy)));
    ex=add(source->body.x,mul(sine,whole(range)));ey=add(source->body.y,mul(cosine,whole(range)));
    a->source=source;a->shoot_z=add(source->body.z,z);a->range=range;a->marker=slope;
    memset(hit,0,sizeof(*hit));
    result=ssa_weapon_aim_trace(a,x,y,ex,ey,predicate,&call);
    return result<0 ? -1 : 1;
}
static int query_from(SsaWeaponAim *a,SsaEntity *source,uint16_t heading,
    int32_t x,int32_t y,int32_t z,int32_t range,int32_t slope,SsaWeaponHit *hit,int exact) {
    int32_t sine,cosine,ex,ey;int result;HitCall call={a,hit,exact?2:1,0,0};
    if(!a || !a->world || !a->math || !source || !hit || range<=0 || a->updating ||
       a->world->query_active || a->traversal.updating || a->walls.updating)return -1;
    ssa_sincos(a->math,heading,&sine,&cosine);
    ex=add(x,fx(sine,range));ey=add(y,fx(cosine,range));
    a->end_x=ex;a->end_y=ey;
    a->source=source;a->shoot_z=z;a->range=range;a->marker=slope;
    a->top_slope=INT32_MAX;a->bottom_slope=INT32_MIN;
    call.sector=ssa_map_sector(a->walls.map,x,y);
    memset(hit,0,sizeof(*hit));
    result=ssa_weapon_aim_trace(a,x,y,ex,ey,predicate,&call);
    if(result>0 && !hit->impact)plane_hit(&call,65536);
    return result<0?-1:1;
}
int ssa_weapon_hit_query_free(SsaWeaponAim *a,SsaEntity *source,uint16_t heading,
    int32_t z,int32_t range,int32_t slope,SsaWeaponHit *hit) {
    if(!source)return -1;
    return query_from(a,source,heading,source->body.x,source->body.y,add(source->body.z,z),range,slope,hit,0);
}
int ssa_weapon_hit_query_camera(SsaWeaponAim *a,SsaEntity *source,uint16_t heading,
    int32_t z,int32_t range,int32_t slope,SsaWeaponHit *hit) {
    if(!source)return -1;
    return query_from(a,source,heading,source->body.x,source->body.y,add(source->body.z,z),range,slope,hit,1);
}
int ssa_weapon_hit_query_unlimited(SsaWeaponAim *a,SsaEntity *source,uint16_t heading,
    int32_t z,int32_t slope,SsaWeaponHit *hit,int exact) {
    int32_t sine,cosine,x,y;int64_t height;
    if(!a || !a->world || !a->math || !source || !hit || a->updating ||
       a->world->query_active || a->traversal.updating || a->walls.updating)return -1;
    x=source->body.x;y=source->body.y;height=(int64_t)source->body.z+z;
    ssa_sincos(a->math,heading,&sine,&cosine);
    memset(hit,0,sizeof(*hit));
    for(;;){
        int64_t range=512*65536,values[3]={x,y,height},directions[3]={sine,cosine,slope};unsigned i;
        if(height<INT32_MIN || height>INT32_MAX)return -1;
        /* Stop at representable/world boundaries, never wrap coordinates.
         * Segment length is a numerical step, not an attack reach limit; each
         * segment crosses fewer than64 block cells even diagonally. */
        for(i=0;i<3;++i)if(directions[i]){
            int64_t remaining=directions[i]>0?INT32_MAX-values[i]:values[i]-INT32_MIN;
            int64_t length=remaining*65536/(directions[i]>0?directions[i]:-directions[i]);
            if(length<range)range=length;
        }
        if(range<=0)return 1;
        if(query_from(a,source,heading,x,y,(int32_t)height,(int32_t)range,slope,hit,exact)<0)return -1;
        if(hit->impact)return 1;
        if(range<512*65536)return 1;
        x=a->end_x;y=a->end_y;height+=(int64_t)range*slope/65536;
        if((int64_t)x<a->world->origin_x || (int64_t)y<a->world->origin_y ||
           (int64_t)x>=(int64_t)a->world->origin_x+(int64_t)a->world->width*128*65536 ||
           (int64_t)y>=(int64_t)a->world->origin_y+(int64_t)a->world->height*128*65536)return 1;
    }
}
