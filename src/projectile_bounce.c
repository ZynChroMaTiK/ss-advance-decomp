#include "ssa/projectile_bounce.h"
#include <limits.h>
#include <string.h>
static int32_t bits(uint32_t n) {return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static int32_t add(int32_t a,int32_t b) {return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b) {return bits((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b) {return bits((uint32_t)a*(uint32_t)b);}
static int16_t i16(uint16_t n) {return (int16_t)(n<=INT16_MAX?(int)n:(int)n-65536);}
static int32_t whole(int32_t n) {return bits(((uint32_t)n>>16)|(n<0?0xffff0000u:0));}
static int32_t fx(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;if(p<0 && p%65536)--q;return bits((uint32_t)q);
}
static unsigned u16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static int side(const SsaMapAssets *m,unsigned line,int32_t x,int32_t y) {
    const uint8_t *l=m->data+m->lines+line*40,*v;
    if(u16(l+2)>=m->vertex_count) return -1;
    v=m->data+m->vertices+u16(l+2)*4;
    return ssa_line_side(x,y,i16((uint16_t)u16(v)),i16((uint16_t)u16(v+2)),
        i16((uint16_t)u16(l+4)),i16((uint16_t)u16(l+6)));
}
int ssa_projectile_bounce_init(SsaProjectileBounce *b,const SsaMapAssets *m) {
    if(!b || b->updating) return -1;
    return ssa_enemy_ray_init(&b->ray,m);
}
int ssa_projectile_bounce_free(SsaProjectileBounce *b) {
    if(!b || b->updating || ssa_enemy_ray_free(&b->ray)<0) return -1;
    memset(b,0,sizeof(*b));return 1;
}
static int cell(void *ctx,SsaRayTraversal *t,int32_t x,int32_t y) {
    SsaProjectileBounce *b=ctx;return ssa_ray_walls_cell(&b->ray.walls,t,x,y);
}
static int predicate(void *ctx,const SsaRayIntercept *h) {
    SsaProjectileBounce *b=ctx;const SsaMapAssets *m=b->ray.walls.map;
    const uint8_t *l=m->data+m->lines+h->line*40;int s;
    if(!(u16(l+26)&4)) {
        s=side(m,h->line,b->body->x,b->body->y);
        if(s<0) return -1;
        if(s) return 1;
    } else {
        if(ssa_ray_opening(m,h->line,&b->ray.opening)<0) return -1;
        if(b->ray.opening.top>=add(b->body->z,b->body->height) &&
           b->ray.opening.bottom<b->body->z) return 1;
    }
    if(h->fraction<b->fraction) {
        b->previous_fraction=b->fraction;b->previous_line=b->line;
        b->fraction=h->fraction;b->line=h->line;
    }
    return 0;
}
int ssa_projectile_bounce(SsaProjectileBounce *b,SsaBody *body,int32_t radius,
    const SsaMathTables *math,uint8_t ticks) {
    SsaEnemyRay *r;int32_t dx,dy,leadx,trailx,leady,traily,x[3],y[3];
    int32_t sine,cosine,ax,ay,den,length;uint16_t tangent,angle;int s,result=-1;unsigned i;
    const uint8_t *l;
    if(!b || !body || !math || b->updating || b->ray.updating ||
       b->ray.traversal.updating || b->ray.walls.updating || !b->ray.walls.map || !b->ray.traversal.intercepts) return -1;
    b->updating=1;b->body=body;r=&b->ray;
    dx=mul(body->velocity_x,ticks);dy=mul(body->velocity_y,ticks);
    leadx=body->velocity_x>0?add(body->x,radius):sub(body->x,radius);
    trailx=body->velocity_x>0?sub(body->x,radius):add(body->x,radius);
    leady=body->velocity_y>0?add(body->y,radius):sub(body->y,radius);
    traily=body->velocity_y>0?sub(body->y,radius):add(body->y,radius);
    x[0]=leadx;y[0]=leady;x[1]=trailx;y[1]=leady;x[2]=leadx;y[2]=traily;
    b->fraction=0x10001;
    for(i=0;i<3;++i) {
        if(ssa_ray_walls_begin(&r->walls)<0 ||
           ssa_ray_trace(&r->traversal,r->origin_x,r->origin_y,x[i],y[i],
               add(x[i],dx),add(y[i],dy),cell,predicate,b)<0) goto done;
    }
    if(b->fraction>0x10000) {result=0;goto done;}
    s=side(r->walls.map,b->line,body->x,body->y);if(s<0) goto done;
    l=r->walls.map->data+r->walls.map->lines+b->line*40;
    tangent=ssa_direction(math,i16((uint16_t)u16(l+4)),i16((uint16_t)u16(l+6)));
    if(s==1) tangent=(uint16_t)(tangent+0x8000);
    angle=ssa_direction(math,i16((uint16_t)whole(dx)),i16((uint16_t)whole(dy)));
    ax=i16((uint16_t)whole(dx));ay=i16((uint16_t)whole(dy));
    if(ax<0)ax=-ax;
    if(ay<0)ay=-ay;
    length=0;
    if(ax || ay) {
        ssa_sincos(math,angle,&sine,&cosine);den=ax>ay?sine:cosine;if(den<0)den=-den;
        length=den?bits((uint32_t)(ax>ay?ax:ay)<<16)/den:0;
    }
    ssa_sincos(math,(uint16_t)(2u*tangent-angle),&sine,&cosine);
    length=bits((uint32_t)length<<16);
    body->velocity_x=ssa_trace_divide(fx(length,sine),(int32_t)ticks*65536);
    body->velocity_y=ssa_trace_divide(fx(length,cosine),(int32_t)ticks*65536);
    result=1;
done:
    b->body=NULL;b->updating=0;return result;
}
