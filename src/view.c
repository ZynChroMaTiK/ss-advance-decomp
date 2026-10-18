#include "ssa/view.h"
#include <limits.h>

#define UNIT INT32_C(65536)
#define CAMERA_RADIUS (4*UNIT)
#define MIN_BOOM (64*UNIT)

static int32_t clamp64(int64_t n) {
    return n>INT32_MAX ? INT32_MAX : n<INT32_MIN ? INT32_MIN : (int32_t)n;
}
static int32_t bounded(int32_t n, int32_t low, int32_t high) {
    return n<low ? low : n>high ? high : n;
}
static int32_t scale(int32_t n, int32_t factor) {
    return (int32_t)((int64_t)n*factor/UNIT);
}
void ssa_view_init(SsaView *v) {
    if (!v) return;
    v->distance=256*UNIT; v->lift=64*UNIT;
    v->third_person=0; v->h_down=0;
}
int ssa_view_key(SsaView *v, int key, int down, int repeat, int enabled) {
    int edge;
    if (!v || (key!='H' && key!='h')) return 0;
    edge=down && !v->h_down;
    v->h_down=(uint8_t)(down!=0);
    if (!edge || repeat || !enabled) return 0;
    v->third_person=(uint8_t)!v->third_person;
    return 1;
}
void ssa_view_focus_lost(SsaView *v) {
    if (v) v->h_down=0;
}

static int clear(SsaMapCollision *c, int32_t ax, int32_t ay, int32_t az,
                 int32_t bx, int32_t by, int32_t bz) {
    int32_t low=az<bz ? az : bz, high=az>bz ? az : bz;
    if (ssa_map_query(c,ax,ay,bx,by,CAMERA_RADIUS)!=1) return 0;
    return (int64_t)low-CAMERA_RADIUS >= (int64_t)c->floor*UNIT &&
           (int64_t)high+CAMERA_RADIUS <= (int64_t)c->ceiling*UNIT;
}

int ssa_view_pose_pitch(const SsaView *v, const SsaBody *b, const SsaCamera *eye,
                  uint16_t heading,int32_t pitch,const SsaMathTables *tables,
                  SsaMapCollision *collision, SsaViewPose *out) {
    SsaViewPose p={0};
    int32_t sx,cy,sp,cp,distance,lift,back,dx,dy,dz,px,py,pz,max_step;
    unsigned steps,i,completed=0;
    if (!v || !b || !eye || !tables || !out) return 0;
    ssa_sincos(tables,heading,&sx,&cy);
    pitch=bounded(pitch,-14564,14564);ssa_sincos(tables,(uint16_t)pitch,&sp,&cp);
    p.x=b->x; p.y=b->y; p.z=eye->eye_z;
    p.target_x=clamp64((int64_t)b->x+scale(sx,scale(cp,256*UNIT)));
    p.target_y=clamp64((int64_t)b->y+scale(cy,scale(cp,256*UNIT)));
    p.target_z=clamp64((int64_t)eye->eye_z+scale(sp,256*UNIT));
    if (!v->third_person) { *out=p; return 1; }
    distance=bounded(v->distance,0,256*UNIT);
    lift=bounded(v->lift,0,64*UNIT);
    if (!collision || distance<MIN_BOOM ||
        !clear(collision,p.x,p.y,p.z,p.x,p.y,p.z)) {
        p.obstructed=1; *out=p; return 1;
    }
    back=-scale(cp,distance);
    dx=scale(sx,back);dy=scale(cy,back);dz=lift-scale(sp,distance);
    /* At most one unit along any axis per segment. Query the swept segment,
     * not only its endpoint, to prevent tunneling through thin solid walls. */
    max_step=distance;
    if((dx<0?-dx:dx)>max_step)max_step=dx<0?-dx:dx;
    if((dy<0?-dy:dy)>max_step)max_step=dy<0?-dy:dy;
    if((dz<0?-dz:dz)>max_step)max_step=dz<0?-dz:dz;
    steps=(unsigned)((max_step+UNIT-1)/UNIT);
    px=p.x; py=p.y; pz=p.z;
    for (i=1;i<=steps;++i) {
        int64_t nx=(int64_t)b->x+(int64_t)dx*i/steps;
        int64_t ny=(int64_t)b->y+(int64_t)dy*i/steps;
        int64_t nz=(int64_t)eye->eye_z+(int64_t)dz*i/steps;
        int32_t x=clamp64(nx),y=clamp64(ny),z=clamp64(nz);
        if (x!=nx || y!=ny || z!=nz || !clear(collision,px,py,pz,x,y,z)) break;
        px=x; py=y; pz=z; completed=i;
    }
    p.obstructed=(uint8_t)(completed<steps);
    p.boom_distance=(int32_t)((int64_t)distance*completed/steps);
    if (p.boom_distance>=MIN_BOOM) {
        p.x=px; p.y=py; p.z=pz; p.third_person=1;
    } else {
        p.boom_distance=0; p.obstructed=1;
    }
    *out=p;
    return 1;
}
int ssa_view_pose(const SsaView *v,const SsaBody *b,const SsaCamera *eye,
    uint16_t heading,const SsaMathTables *tables,SsaMapCollision *collision,SsaViewPose *out) {
    return ssa_view_pose_pitch(v,b,eye,heading,0,tables,collision,out);
}
static uint16_t direction(const SsaMathTables *t,double x,double y) {
    double ax=x<0?-x:x,ay=y<0?-y:y;int ratio,angle;
    if(!(ax+ay))return 0;
    ratio=(int)(ax*16384/(ax+ay));
    angle=t->direction[x<0?16384-ratio:16384+ratio];
    if(y>0)angle=-angle;
    return (uint16_t)(angle+16384);
}
int ssa_view_angles(const SsaViewPose *p,const SsaMathTables *t,uint16_t *heading,int32_t *pitch) {
    double dx,dy,dz,n,h;unsigned i;uint16_t angle;
    if(!p || !t || !heading || !pitch)return 0;
    dx=(double)p->target_x-p->x;dy=(double)p->target_y-p->y;dz=(double)p->target_z-p->z;
    if(dx==0 && dy==0 && dz==0)return 0;
    n=dx*dx+dy*dy;h=(dx<0?-dx:dx)+(dy<0?-dy:dy);
    if(h)for(i=0;i<8;++i)h=(h+n/h)/2;
    angle=direction(t,dz,h);*pitch=angle<=32767?(int32_t)angle:(int32_t)angle-65536;
    if(h)*heading=direction(t,dx,dy);
    return 1;
}
