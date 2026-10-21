/* Shared render-only doorway camera; no gameplay or progression changes. */
#include "ssa/geometry.h"
#include <math.h>
static unsigned fixture_u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int fixture_s16(const uint8_t *p){unsigned n=fixture_u16(p);return n<32768?(int)n:(int)n-65536;}
static int godray_fixture_camera(SsaNativeGame *g){unsigned i;
    /* Render-only camera at a real eave, on the covered side. No player or
     * campaign progression is moved to manufacture a gameplay test. */
    for(i=0;i<g->map.line_count;++i){const uint8_t *l=g->map.data+g->map.lines+i*40,*a,*b,*s,*roof;
        unsigned front=fixture_u16(l+12),back=fixture_u16(l+14),other;double dx,dy,len,nx,ny,x,y;
        if(!(fixture_u16(l+26)&4) || front==back || back>=g->map.sector_count)continue;
        s=g->map.data+g->map.sectors+front*20;roof=g->map.data+g->map.sectors+back*20;other=back;
        if(s[11] && !roof[11]){const uint8_t *swap=s;s=roof;roof=swap;other=front;}
        else if(s[11] || !roof[11])continue;
        if(fixture_s16(s+2)<=fixture_s16(roof+2) || fixture_s16(roof+2)<=fmax(fixture_s16(s),fixture_s16(roof))+32)continue;
        a=g->map.data+g->map.vertices+fixture_u16(l)*4;b=g->map.data+g->map.vertices+fixture_u16(l+2)*4;
        dx=fixture_s16(b)-fixture_s16(a);dy=fixture_s16(b+2)-fixture_s16(a+2);len=hypot(dx,dy);if(len<8)continue;
        nx=dy/len;ny=-dx/len;if(other==front){nx=-nx;ny=-ny;}
        x=(fixture_s16(a)+fixture_s16(b))*0.5-nx*64;y=(fixture_s16(a+2)+fixture_s16(b+2))*0.5-ny*64;
        if(ssa_map_sector(&g->map,(int32_t)(x*65536),(int32_t)(y*65536))!=(int)other)continue;
        g->pc_camera.x=(int32_t)(x*65536);g->pc_camera.y=(int32_t)(y*65536);
        g->pc_camera.z=(int32_t)((fmax(fixture_s16(s),fixture_s16(roof))+fixture_s16(roof+2))*0.5*65536);g->pc_camera.sector=(int16_t)other;
        g->pc_camera.heading=(uint16_t)(int)(atan2(nx,ny)*65536/(2*3.141592653589793));return 1;
    }
    return 0;
}
