#include "ssa/projectile_motion.h"
#include <limits.h>
static int32_t bits(uint32_t v) { return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v; }
static int16_t half(uint16_t v) { return v<=INT16_MAX?(int16_t)v:(int16_t)(-1-(int32_t)(UINT16_MAX-v)); }
static int32_t fixed(int32_t a,int32_t b) {
    int64_t n=(int64_t)a*b,q=n/65536;
    if(n<0 && n%65536) --q;
    return bits((uint32_t)q);
}
static int motion(SsaProjectile *p,SsaProjectileMotionGame *g,
                          const SsaMathTables *tables,const SsaProjectileMotionOps *ops,int unlimited) {
    SsaEntity *e,*other=NULL;
    int32_t saved[6],x,y,s,c,forward,side,zoff;
    int16_t target_health,difference;
    int moved,result=-1;
    if(!p || !p->entity || p->updating || !g || !tables || !ops || !ops->move ||
       !ops->vertical || !ops->blocker || !ops->health || !ops->damage ||
       !ops->freeze || !ops->unlink || !ops->insert || !ops->bounce ||
       g->player_count>4 || p->owner>g->player_count ||
       (p->owner && !g->players[p->owner-1]) ||
       (p->entity->owner && p->entity->owner->query_active)) return -1;
    p->updating=1; e=p->entity;
    saved[0]=e->body.x; saved[1]=e->body.y; saved[2]=e->body.z;
    saved[3]=e->body.velocity_x; saved[4]=e->body.velocity_y; saved[5]=e->body.velocity_z;
    if(unlimited && p->owner){unsigned age=(unsigned)p->timer+g->ticks;p->timer=(uint16_t)(age>480?480:age);}
    else p->timer=(uint16_t)(p->timer+g->ticks);
    if(p->owner && (e->body.flags&0x4000) &&
       bits((uint32_t)(int32_t)half(p->timer)*(uint32_t)p->speed)>0x1900000)
        e->body.flags&=(uint16_t)~0x4000;
    if(p->owner && (e->body.flags&0x4000)) {
        const SsaEntity *owner=g->players[p->owner-1];
        forward=0x500000; side=0; zoff=0x500000;
        if(e->type==0x67) { side=0x80000; zoff=0x400000; }
        else if(e->type==0x69) { forward=0xc00000; side=0x280000; zoff=0x400000; }
        ssa_sincos(tables,g->headings[p->owner-1],&s,&c);
        x=bits((uint32_t)owner->body.x+(uint32_t)fixed(s,forward)+(uint32_t)fixed(c,side));
        y=bits((uint32_t)owner->body.y+(uint32_t)fixed(c,forward)-(uint32_t)fixed(s,side));
        x=bits((uint32_t)x+(uint32_t)fixed(s,p->speed)*(uint32_t)(int32_t)half(p->timer));
        y=bits((uint32_t)y+(uint32_t)fixed(c,p->speed)*(uint32_t)(int32_t)half(p->timer));
        e->body.z=bits((uint32_t)owner->body.z+(uint32_t)zoff+
                       (uint32_t)e->body.velocity_z*(uint32_t)((int32_t)half(p->timer)-1));
        e->body.velocity_x=fixed(s,p->speed); e->body.velocity_y=fixed(c,p->speed);
    } else {
        x=bits((uint32_t)e->body.x+(uint32_t)e->body.velocity_x*g->ticks);
        y=bits((uint32_t)e->body.y+(uint32_t)e->body.velocity_y*g->ticks);
    }
    if(!(unlimited && p->owner) && half(p->timer)>480) { result=0; goto done; }
    moved=ops->move(ops->context,p,x,y);
    if(moved<0 || moved>1 || p->entity!=e) goto done;
    if(moved) {
        moved=ops->vertical(ops->context,p);
        if(moved<0 || moved>1 || p->entity!=e) goto done;
        if(moved) { result=1; goto done; }
    }
    if(ops->blocker(ops->context,p,&other)<0 || p->entity!=e) goto done;
    if(other) {
        if(e->type==0x68) {
            g->credit=p->owner; g->hit_weapon=p->weapon;
            moved=ops->damage(ops->context,p,other,100);
            g->credit=0; g->hit_weapon=0;
            if(moved<0) goto done;
            result=1;
        } else if(e->type==0x69) {
            result=0;
            if((uint16_t)(other->type-0xcd)>9 || (other->body.flags&0x400)) goto done;
            other->body.flags|=0x400;
            if(ops->freeze(ops->context,p,other)<0) result=-1;
        } else if(e->type==0x67) {
            result=0;
            if((uint16_t)(other->type-0xd7)<=1 || !p->owner) {
                p->health=0; p->speed=0; goto done;
            }
            if(ops->health(ops->context,other,&target_health)<0) { result=-1; goto done; }
            difference=half((uint16_t)((uint16_t)target_health-(uint16_t)p->health));
            if(ops->damage(ops->context,p,other,(uint32_t)(int32_t)p->health)<0) { result=-1; goto done; }
            p->health=half((uint16_t)(-(int32_t)difference));
            if(p->health>=0) result=1;
            else { p->health=0; p->speed=0; }
        } else result=0;
    } else if(e->type==0x67 && p->owner) {
        if(ops->unlink(ops->context,p)<0) goto done;
        e->body.x=saved[0]; e->body.y=saved[1]; e->body.z=saved[2];
        e->body.velocity_x=saved[3]; e->body.velocity_y=saved[4]; e->body.velocity_z=saved[5];
        if(ops->insert(ops->context,p)<0) goto done;
        result=ops->bounce(ops->context,p);
        if(result!=0 && result!=1) result=-1;
    } else result=0;
done:
    if(p->entity!=e) result=-1;
    p->updating=0;
    return result;
}
int ssa_projectile_motion(SsaProjectile *p,SsaProjectileMotionGame *g,
    const SsaMathTables *tables,const SsaProjectileMotionOps *ops) {
    return motion(p,g,tables,ops,0);
}
int ssa_projectile_motion_unlimited(SsaProjectile *p,SsaProjectileMotionGame *g,
    const SsaMathTables *tables,const SsaProjectileMotionOps *ops) {
    return motion(p,g,tables,ops,1);
}
