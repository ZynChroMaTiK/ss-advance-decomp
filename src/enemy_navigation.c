#include "ssa/enemy_navigation.h"
#include <limits.h>

static int32_t bits(uint32_t v) { return v <= INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t add(int32_t a, int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t neg(int32_t a) { return bits(0u-(uint32_t)a); }
static int32_t abs32(int32_t a) { return a < 0 ? neg(a) : a; }
static int32_t high(int32_t a) { return bits(((uint32_t)a>>16)|(a<0 ? 0xffff0000u:0)); }
static int16_t s16(uint16_t v) { return v <= INT16_MAX ? (int16_t)v : (int16_t)(-1-(int32_t)(uint16_t)~v); }
static int32_t fixed(int32_t a,int32_t b) {
    int64_t v=(int64_t)a*b,q=v/65536;
    if(v<0 && v%65536) --q;
    return bits((uint32_t)q);
}
static void reset(SsaEnemyNavigation *n) {
    n->phase=0; n->heading=(uint16_t)(n->heading+0x1000);
    if(!n->heading) {
        n->distance=(uint16_t)(n->distance+0x40);
        if(s16(n->distance)>0x2ff) n->distance=0x40;
    }
}
static int valid(const SsaEnemyNavigation *n) {
    return n && n->entity && !n->updating &&
           (!n->entity->owner || !n->entity->owner->query_active);
}
int ssa_enemy_navigation_reset(SsaEnemyNavigation *n) {
    if(!valid(n)) return -1;
    reset(n); return 1;
}
int ssa_enemy_navigation_speeds(int32_t out[12], const uint8_t *rom,size_t size) {
    unsigned i;
    if(!out || !rom || size<0x44bfd4+12*8) return -1;
    for(i=0;i<12;++i) {
        const uint8_t *p=rom+0x44bfd4+i*8;
        out[i]=bits((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
    }
    return 1;
}
/* 093E8 distance uses signed division after the numerator's wrapped LSL16.
 * Consequently -32768 inputs can produce negative distances; retain it. */
static uint16_t direction(const SsaMathTables *m,int32_t x,int32_t y,int32_t *distance) {
    int32_t sx,cx,ax=abs32(x),ay=abs32(y),den,num;
    uint16_t h=ssa_direction(m,(int16_t)x,(int16_t)y);
    if(!(ax+ay)) { *distance=0; return h; }
    ssa_sincos(m,h,&sx,&cx);
    den=abs32(ax>ay?sx:cx); num=bits((uint32_t)(ax>ay?ax:ay)<<16);
    *distance=den ? num/den : 0;
    return h;
}
int ssa_enemy_navigation_step(SsaEnemyNavigation *n,SsaEntity *player,
                             const SsaMathTables *m,unsigned ticks,
                             int32_t factor,const SsaEnemyNavigationOps *o) {
    SsaEntity *e,*target,*near;
    int blocked,mask,status=-1;
    unsigned cf=0;
    int32_t x,y,s,c,distance=0,delta=0,moved,ox=0,oy=0;
    if(!valid(n) || !player || !m || ticks>255 || !o || !o->visibility ||
       !o->nearest || !o->path || !o->turn || !o->move || !o->finish ||
       n->entity->type<0xcd || n->entity->type>0xd8 ||
       (player->owner && player->owner->query_active)) return -1;
    n->updating=1; e=n->entity; target=player;
    if(o->visibility(o->context,player->sector,e->sector,&blocked)<0) goto done;
    if(blocked || (e->body.flags&0x800)) goto finish;
    if(e->body.flags&0x400) {
        n->target_timer=(uint16_t)(n->target_timer-ticks);
        if(n->target && (n->target->body.flags&0x20)) n->target_timer=0;
        if(s16(n->target_timer)<=0 || !n->target) {
            n->target_timer=600;
            if(o->nearest(o->context,e,&near)<0) goto done;
            n->target=near;
        }
        if(n->target) target=n->target;
    }
    if(o->nearest(o->context,player,&near)<0) goto done;
    cf=e->type==0xcf && near==e;
    if(n->phase==0) {
        n->navigation_flags|=1;
        n->desired_facing=(uint8_t)(direction(m,high(sub(e->body.x,player->body.x)),
                                   high(sub(e->body.y,player->body.y)),&distance)>>8);
        if(o->turn(o->context,n,0x10000,&delta)<0) goto done;
        ssa_sincos(m,n->heading,&s,&c);
        x=add(e->body.x,bits((uint32_t)s*(uint32_t)(int32_t)s16(n->distance)));
        y=add(e->body.y,bits((uint32_t)c*(uint32_t)(int32_t)s16(n->distance)));
        if(o->path(o->context,n,e->body.x,e->body.y,x,y,e->radius,cf,0,&mask,&ox,&oy)<0) goto done;
        if(mask==3) {
            if(o->path(o->context,n,x,y,target->body.x,target->body.y,e->radius,cf,1,&mask,&ox,&oy)<0) goto done;
            if(mask==3) { n->waypoint_x=(uint16_t)high(x); n->waypoint_y=(uint16_t)high(y); n->phase=1; goto finish; }
        }
        reset(n);
        if(o->move(o->context,n,&moved)<0) goto done;
        goto finish;
    }
    if(n->phase!=1 && n->phase!=2) goto finish;
    n->navigation_flags&=(uint8_t)~1u;
    n->desired_facing=(uint8_t)(direction(m,high(sub(e->body.x,bits((uint32_t)n->waypoint_x<<16))),
                                   high(sub(e->body.y,bits((uint32_t)n->waypoint_y<<16))),&distance)>>8);
    if(n->phase==2) n->distance=(uint16_t)distance;
    if(o->turn(o->context,n,0x10000,&delta)<0) goto done;
    delta=abs32(delta);
    n->speed=fixed(bits((uint32_t)sub(0x8000,delta)<<3),factor);
    if(o->move(o->context,n,&moved)<0) goto done;
    n->move_result=(uint8_t)moved;
    if(o->path(o->context,n,e->body.x,e->body.y,target->body.x,target->body.y,e->radius,cf,1,&mask,&ox,&oy)<0) goto done;
    if(n->phase==1) {
        if(mask==3) {
            n->waypoint_x=(uint16_t)high(target->body.x); n->waypoint_y=(uint16_t)high(target->body.y); n->phase=2;
        } else if(distance<=7 || (!n->move_result && abs32(delta)<=0x7ff)) reset(n);
    } else {
        if(mask==3) {
            n->waypoint_x=(uint16_t)high(target->body.x); n->waypoint_y=(uint16_t)high(target->body.y);
            if(delta<=0x3ff) n->navigation_flags|=1;
        } else if(mask) {
            n->phase=1; ox=bits((uint32_t)ox<<1); oy=bits((uint32_t)oy<<1);
            if(mask==2) { ox=neg(ox); oy=neg(oy); }
            n->waypoint_x=(uint16_t)high(add(e->body.x,ox));
            n->waypoint_y=(uint16_t)high(add(e->body.y,oy));
            goto finish;
        }
        if(!n->move_result && abs32(delta)<=0x7ff) reset(n);
    }
finish:
    if(o->finish(o->context,n)<0) goto done;
    status=1;
done:
    n->updating=0; return status;
}

int ssa_enemy_path_query(SsaEnemyPath *p,const SsaMathTables *m,int32_t x0,
                        int32_t y0,int32_t x1,int32_t y1,int32_t radius,
                        unsigned cf,unsigned mode,SsaEnemyPathRay ray,
                        void *context,int *mask) {
    int32_t s,c,ox,oy,ex,ey,dist,lines[2][4],half;
    uint16_t h;
    int clear,result=0;
    unsigned i,j;
    if(!p || p->updating || p->segment_count>10 || !m || !ray || !mask) return -1;
    p->updating=1;
    half=bits(((uint32_t)radius>>1)|(radius<0?0x80000000u:0));
    radius=add(radius,half);
    h=(uint16_t)(direction(m,high(sub(x1,x0)),high(sub(y1,y0)),&dist)+0x4000);
    ssa_sincos(m,h,&s,&c);
    ox=fixed(s,radius); oy=fixed(c,radius);
    ex=mode?bits((uint32_t)s<<2):ox; ey=mode?bits((uint32_t)c<<2):oy;
    lines[0][0]=add(x0,ox); lines[0][1]=add(y0,oy);
    lines[0][2]=add(x1,ex); lines[0][3]=add(y1,ey);
    lines[1][0]=sub(x0,ox); lines[1][1]=sub(y0,oy);
    lines[1][2]=sub(x1,ex); lines[1][3]=sub(y1,ey);
    if(cf) {
        for(i=0;i<2;++i) if(p->segment_count!=10) {
            for(j=0;j<4;++j) p->segments[p->segment_count][j]=lines[i][j];
            ++p->segment_count;
        }
        p->trace_mode=1;
    }
    for(i=0;i<2;++i) {
        if(ray(context,p,lines[i][0],lines[i][1],lines[i][2],lines[i][3],&clear)<0) {
            p->updating=0; return -1;
        }
        if(clear) result|=1<<(int)i;
    }
    p->offset_x=ox; p->offset_y=oy; p->trace_mode=0;
    *mask=result; p->updating=0;
    return 1;
}
