/* Render-only regression: artwork may cross its own floor; walls still hide it.
 * Both native backends run this fixture. Gameplay/world lists are untouched. */
#include <math.h>
static void sprite_clip_fixture(SsaPcRenderer *r,SsaNativeGame *g)
{
    SsaActorResource resource=g->actor_assets.resources[1];SsaActorSource sources[100];
    int32_t offsets[100];SsaRenderCamera camera=g->pc_camera;unsigned count=g->spawn.pool.active_count;
    unsigned fov=g->fov_offset;int pitch=g->render_pitch;SsaEntity e={0};
    uint8_t texels[32*32],*floors=malloc(g->map.sector_count*2u);
    SsaActorImage im={0,-16,-24,32,32,texels};uint16_t palette=g->palette_effect.colors[255];
    size_t n=321*213,p;uint8_t *mask=malloc(n);unsigned mode,i;int32_t sine,cosine;
    CHECK(floors && mask);CHECK(ssa_pc_renderer_resize(r,321,213)==1);
    memcpy(sources,g->sources,sizeof sources);memcpy(offsets,g->source_z_offsets,sizeof offsets);
    memset(g->source_z_offsets,0,sizeof offsets);memset(texels,255,sizeof texels);
    g->palette_effect.colors[255]=0x7c1f;
    g->actor_assets.resources[1]=(SsaActorResource){0,0,1,1,2,1,&im};
    g->spawn.pool.active_count=1;g->sources[0]=(SsaActorSource){&e,1,0,0,0,0};
    for(i=0;i<g->map.sector_count;++i)memcpy(floors+i*2,g->map.data+g->map.sectors+i*20,2);
    ssa_sincos(&g->math,camera.heading,&sine,&cosine);
    e.body.x=camera.x+sine*128;e.body.y=camera.y+cosine*128;
    {const uint8_t *s=g->map.data+g->map.sectors+camera.sector*20;
        int z=(int16_t)(s[0]|s[1]<<8);e.body.z=z*65536;e.body.height=32*65536;g->pc_camera.z=(z+32)*65536;}
    for(mode=0;mode<3;++mode){unsigned visible=0,below_floor=0;float *depth=malloc(n*sizeof(*depth));
        CHECK(depth);g->render_pitch=mode==0?0:mode==1?-2048:2048;g->fov_offset=mode==2?60:10;
        g->spawn.pool.active_count=0;CHECK(ssa_pc_scene_draw(r,g)==1);memcpy(depth,r->depth,n*sizeof(*depth));
        g->spawn.pool.active_count=1;CHECK(ssa_pc_scene_draw(r,g)==1);
        for(p=0;p<n;++p){mask[p]=(uint8_t)(r->pixels[p]==0xff00ff);
            if(mask[p]){++visible;if(depth[p]+0.1f<r->depth[p])++below_floor;}}
        CHECK(visible>100 && below_floor>20);free(depth);
        /* Lowering the floor must not expose previously chopped sprite rows. */
        for(i=0;i<g->map.sector_count;++i){uint8_t *s=g->map.data+g->map.sectors+i*20;
            unsigned z=(unsigned)(s[0]|s[1]<<8)-128;s[0]=(uint8_t)z;s[1]=(uint8_t)(z>>8);}
        CHECK(ssa_pc_scene_draw(r,g)==1);
        for(p=0;p<n;++p)CHECK(mask[p]==(r->pixels[p]==0xff00ff));
        for(i=0;i<g->map.sector_count;++i)memcpy(g->map.data+g->map.sectors+i*20,floors+i*2,2);
    }
    /* Choose a real, wide one-sided wall. Place the camera on its front and
     * the actor behind it: bypassing plane depth must not reveal the actor. */
    for(i=0;i<g->map.line_count;++i){const uint8_t *l=g->map.data+g->map.lines+i*40,*a,*b,*s;
        unsigned va=l[0]|l[1]<<8,vb=l[2]|l[3]<<8,sector=l[12]|l[13]<<8;
        double ax,ay,bx,by,dx,dy,len,nx,ny,z;
        if((l[26]&4) || sector>=g->map.sector_count)continue;
        a=g->map.data+g->map.vertices+va*4;b=g->map.data+g->map.vertices+vb*4;
        ax=(int16_t)(a[0]|a[1]<<8);ay=(int16_t)(a[2]|a[3]<<8);
        bx=(int16_t)(b[0]|b[1]<<8);by=(int16_t)(b[2]|b[3]<<8);
        dx=bx-ax;dy=by-ay;len=hypot(dx,dy);if(len<128)continue;
        s=g->map.data+g->map.sectors+sector*20;z=((int16_t)(s[0]|s[1]<<8)+(int16_t)(s[2]|s[3]<<8))*0.5;
        nx=dy/len;ny=-dx/len;
        g->pc_camera.x=(int32_t)(((ax+bx)*0.5+nx*64)*65536);
        g->pc_camera.y=(int32_t)(((ay+by)*0.5+ny*64)*65536);g->pc_camera.z=(int32_t)(z*65536);
        g->pc_camera.sector=(int16_t)sector;
        g->pc_camera.heading=(uint16_t)(int)(atan2(-nx,-ny)*65536/(2*3.141592653589793));
        e.body.x=(int32_t)(((ax+bx)*0.5-nx*64)*65536);
        e.body.y=(int32_t)(((ay+by)*0.5-ny*64)*65536);e.body.z=g->pc_camera.z;
        for(mode=0;mode<3;++mode){g->render_pitch=mode==0?0:mode==1?-2048:2048;
            CHECK(ssa_pc_scene_draw(r,g)==1 && r->actor_pixels==0);}
        break;
    }
    CHECK(i<g->map.line_count);
    g->actor_assets.resources[1]=resource;g->palette_effect.colors[255]=palette;
    memcpy(g->sources,sources,sizeof sources);memcpy(g->source_z_offsets,offsets,sizeof offsets);
    g->spawn.pool.active_count=count;g->pc_camera=camera;g->render_pitch=pitch;g->fov_offset=fov;
    free(floors);free(mask);
}
