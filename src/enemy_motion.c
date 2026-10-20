#include "ssa/enemy_motion.h"
#include <limits.h>
static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
static int16_t half(uint16_t n) { return n<=INT16_MAX?(int16_t)n:(int16_t)(-1-(int32_t)(UINT16_MAX-n)); }
static int16_t difference(int32_t a,int32_t b) { return half((uint16_t)(((uint32_t)a-(uint32_t)b)>>16)); }
static int32_t fixed(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b, q=p/65536;
    if (p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static int valid(const SsaEnemyMotion *m) {
    return m && m->entity && !m->updating &&
        (!m->entity->owner || !m->entity->owner->query_active);
}
int ssa_enemy_face(SsaEnemyMotion *m,const SsaEntity *target,const SsaMathTables *t) {
    if (!valid(m) || !target || !t) return -1;
    m->desired_facing=(uint8_t)(ssa_direction(t,difference(m->entity->body.x,target->body.x),
                                              difference(m->entity->body.y,target->body.y))>>8);
    return 0;
}
int ssa_enemy_turn(SsaEnemyMotion *m,unsigned ticks,int32_t *delta) {
    int32_t d;
    if (!valid(m) || ticks>255 || !delta) return -1;
    d=((int)m->desired_facing-(int)m->facing)*256;
    if (d>32768) { m->facing=(uint8_t)(m->facing-ticks); d-=65536; }
    else if (d>0) m->facing=(uint8_t)(m->facing+ticks);
    else if (d < -32768) { m->facing=(uint8_t)(m->facing+ticks); d+=65536; }
    else m->facing=(uint8_t)(m->facing-ticks);
    *delta=d;
    return 0;
}
int ssa_enemy_distance(const SsaEntity *a,const SsaEntity *b,const SsaMathTables *t,int32_t *out) {
    int32_t x,y,ax,ay,s,c,den,num;
    if (!a || !b || !t || !out) return -1;
    x=difference(a->body.x,b->body.x); y=difference(a->body.y,b->body.y);
    ax=x<0?-x:x; ay=y<0?-y:y;
    if (!ax && !ay) { *out=0; return 0; }
    ssa_sincos(t,ssa_direction(t,(int16_t)x,(int16_t)y),&s,&c);
    den=ax>ay?s:c; if (den<0) den=-den;
    num=bits((uint32_t)(ax>ay?ax:ay)<<16);
    if (!den) return -1;
    *out=num/den;
    return 0;
}
int ssa_enemy_walk(SsaEnemyMotion *m,const SsaMathTables *t,unsigned ticks,
                     uint32_t *count,const SsaEnemyMotionOps *ops) {
    SsaEntity *e;
    int32_t s,c,x,y;
    int moved;
    if (!valid(m) || !t || ticks>255 || !count || !ops || !ops->move ||
        (m->entity->body.flags&SSA_BODY_PLAYER)) return -1;
    m->updating=1; e=m->entity; ++*count;
    ssa_sincos(t,(uint16_t)(((unsigned)m->facing+128u)<<8),&s,&c);
    x=bits((uint32_t)e->body.x+(uint32_t)fixed(s,m->speed)*ticks);
    y=bits((uint32_t)e->body.y+(uint32_t)fixed(c,m->speed)*ticks);
    moved=ops->move(ops->context,m,x,y);
    if (moved<0 || moved>1 || m->entity!=e) { m->updating=0; return -1; }
    ssa_vertical_step(&e->body,NULL,(uint8_t)ticks);
    m->updating=0;
    return moved;
}
static int world_move(void *p,SsaEnemyMotion *m,int32_t x,int32_t y) {
    return ssa_world_try_move(p,m->entity,x,y);
}
int ssa_enemy_walk_world(SsaEnemyMotion *m,const SsaMathTables *t,unsigned ticks,
                           uint32_t *count,SsaWorld *w) {
    SsaEnemyMotionOps ops={world_move,w};
    if (!m || !m->entity || !w || m->entity->owner!=w) return -1;
    return ssa_enemy_walk(m,t,ticks,count,&ops);
}
static int64_t absolute(int64_t n) {return n<0?-n:n;}
static int crowd_step(SsaEnemyCrowd *c,SsaWorld *w,SsaEntity *e,int32_t dx,int32_t dy) {
    int64_t x=(int64_t)e->body.x+dx,y=(int64_t)e->body.y+dy,distance;
    const SsaEntity *blocker;int result,side;unsigned attempt;
    if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX)return -1;
    result=ssa_world_try_move_crowd(w,e,(int32_t)x,(int32_t)y);
    if(result!=0)return result;
    blocker=w->blocking_entity;
    if(!blocker || blocker->type<0xcd || blocker->type>0xd8 || (blocker->body.flags&0x10)) {
        c->blocker=NULL;return 0;
    }
    if(!c->contact)c->contact=w->blocking_entity;
    distance=absolute(dx)>absolute(dy)?absolute(dx):absolute(dy);
    if(!distance)return 0;
    if(c->blocker!=blocker || !c->side) {
        int32_t tangent;
        /* The original bodies collide as squares. Move along the nearer
         * expanded-box face; a centered head-on pair picks opposite sides. */
        c->axis=absolute((int64_t)e->body.x-blocker->body.x)>=
            absolute((int64_t)e->body.y-blocker->body.y)?1u:0u;
        tangent=c->axis?dy:dx;
        if(!tangent)tangent=c->axis?dx:-dy;
        c->side=tangent<0?-1:1;c->blocker=blocker;
    }
    side=c->side;
    for(attempt=0;attempt<2;++attempt,side=-side) {
        x=(int64_t)e->body.x+(c->axis?0:side*distance);
        y=(int64_t)e->body.y+(c->axis?side*distance:0);
        if(x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX)continue;
        result=ssa_world_try_move_crowd(w,e,(int32_t)x,(int32_t)y);
        if(result<0)return -1;
        if(result){c->side=side;return 1;}
    }
    /* Failed lateral probes must not turn an enemy contact into a stale wall
     * or player contact observed by the attack controller. */
    w->blocking_entity=(SsaEntity *)blocker;return 0;
}
int ssa_enemy_crowd_move(SsaEnemyCrowd *c,SsaWorld *w,SsaEntity *e,int32_t x,int32_t y) {
    int64_t dx,dy,distance,px=0,py=0;unsigned i,steps;int result=0,moved=0;
    if(!c || c->updating || c->axis>1 || c->side< -1 || c->side>1 || !w || w->query_active || !e || e->owner!=w ||
       e->type<0xcd || e->type>0xd8 || (e->body.flags&0x10))return -1;
    dx=(int64_t)x-e->body.x;dy=(int64_t)y-e->body.y;
    distance=absolute(dx)>absolute(dy)?absolute(dx):absolute(dy);
    steps=(unsigned)((distance+4*65536-1)/(4*65536));if(!steps)steps=1;
    c->updating=1;c->contact=NULL;
    for(i=1;i<=steps;++i){int64_t nx=dx*i/steps,ny=dy*i/steps;
        result=crowd_step(c,w,e,(int32_t)(nx-px),(int32_t)(ny-py));
        if(result!=1)break;
        moved=1;px=nx;py=ny;
    }
    c->updating=0;return result<0?-1:moved;
}
typedef struct CrowdMove {SsaWorld *world;SsaEnemyCrowd *crowd;} CrowdMove;
static int crowd_move(void *context,SsaEnemyMotion *m,int32_t x,int32_t y) {
    CrowdMove *c=context;return ssa_enemy_crowd_move(c->crowd,c->world,m->entity,x,y);
}
int ssa_enemy_walk_world_sliding(SsaEnemyMotion *m,const SsaMathTables *t,unsigned ticks,
    uint32_t *count,SsaWorld *w,SsaEnemyCrowd *crowd) {
    CrowdMove c={w,crowd};SsaEnemyMotionOps ops={crowd_move,&c};
    if(!m || !m->entity || !w || m->entity->owner!=w || !crowd || crowd->updating)return -1;
    return ssa_enemy_walk(m,t,ticks,count,&ops);
}
int ssa_enemy_hazards(const SsaEntity *e,int16_t *health,const SsaMapAssets *map,
                        const SsaPlayerHazardGame *g) {
    const uint8_t *sector;
    size_t off;
    int sample;
    if (!e || !health || !map || !map->data || !g || g->hazard_count>255 ||
        (g->hazard_count && !g->hazard_sectors) || e->sector<0 ||
        e->sector>=map->sector_count || map->sectors>map->size ||
        (size_t)e->sector>(map->size-map->sectors)/20) return -1;
    off=map->sectors+(size_t)e->sector*20;
    if (map->size-off<20) return -1;
    sector=map->data+off;
    if ((sector[10]==255 || (sector[10]==35 && g->level_counter>5)) &&
        half((uint16_t)((uint32_t)e->body.z>>16))==half((uint16_t)(sector[0]|(uint16_t)sector[1]<<8)))
        *health=0;
    sample=ssa_player_hazard_sample(map,e,g);
    if (sample<0) return -1;
    if (sample) *health=0;
    return 0;
}
