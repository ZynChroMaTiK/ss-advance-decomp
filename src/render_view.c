#include "ssa/render_view.h"

int ssa_render_view_step(SsaRenderView *v,const SsaRenderViewOps *ops)
{
    unsigned i;
    int result=-1;
    if (!v || v->updating || !ops || !ops->event) return -1;
    v->updating=1;
#define EVENT(kind,arg) do { if(ops->event(ops->context,v,kind,arg)<0) goto done; } while(0)
    EVENT(SSA_RENDER_VIEW_CLEAR,0);
    for (i=0;i<4;++i) {
        SsaRenderViewPlayer *p=&v->players[i];
        if ((p->flags&12u)==12u) {
            if (p->keys&1u) {
                p->zoom+=(p->zoom>>4)*v->ticks;
                if (p->zoom>65536u) p->zoom=65536u;
            }
            if (p->keys&2u) {
                p->zoom-=(p->zoom>>4)*v->ticks;
                if (p->zoom<4096u) p->zoom=4096u;
            }
        }
    }
    if (v->local_index>=4) goto done;
    if (v->players[v->local_index].flags&8u) {
        EVENT(SSA_RENDER_VIEW_AUTOMAP,v->players[v->local_index].zoom);
    } else {
        if (!v->math) goto done;
        ssa_sincos(v->math,(uint16_t)(0u-v->camera.heading),
                   &v->camera.sine,&v->camera.cosine);
        EVENT(SSA_RENDER_VIEW_WORLD,0);
        if (v->overlay) EVENT(SSA_RENDER_VIEW_OVERLAY,0);
    }
    result=0;
done:
    v->updating=0;
    return result;
#undef EVENT
}
int ssa_render_view_pose(SsaRenderView *v,const SsaViewPose *pose,
                           uint16_t heading,const SsaMapAssets *map)
{
    int sector;
    if (!v || v->updating || !pose || !map) return -1;
    sector=ssa_map_sector(map,pose->x,pose->y);
    if (sector<0 || sector>32767) return -1;
    v->camera.x=pose->x;v->camera.y=pose->y;v->camera.z=pose->z;
    v->camera.heading=heading;v->camera.sector=(int16_t)sector;
    return 0;
}
