#include "ssa/world_scene.h"
#include "ssa/raster_fill.h"
#include <limits.h>
#include <string.h>

int ssa_scene_coverage_reset(SsaSceneCoverage *c)
{
    if (!c) return -1;
    c->ranges[0].left=-32767;c->ranges[0].right=-1;
    c->ranges[1].left=120;c->ranges[1].right=32767;
    c->count=2;
    return 1;
}
int ssa_scene_coverage_merge(SsaSceneCoverage *c,unsigned left,unsigned right)
{
    unsigned i=0,j,k,old;
    if (!c || c->count<2 || c->count>=SSA_SCENE_COVERAGE_LIMIT ||
        left>119 || right>120 || left>right) return -1;
    while (i<c->count && c->ranges[i].right<(int)left-1) ++i;
    if (i>=c->count) return -1;
    if ((int)left<c->ranges[i].left) {
        if ((int)right<c->ranges[i].left-1) {
            if (c->count+1>=SSA_SCENE_COVERAGE_LIMIT) return -1;
            for (k=c->count;k>i;--k) c->ranges[k]=c->ranges[k-1];
            c->ranges[i].left=(int16_t)left;c->ranges[i].right=(int16_t)right;
            ++c->count;return 1;
        }
        c->ranges[i].left=(int16_t)left;
    }
    if ((int)right<=c->ranges[i].right) return 1;
    j=i;
    for (;;) {
        if (j+1>=c->count) return -1;
        if ((int)right<c->ranges[j+1].left-1) {
            c->ranges[i].right=(int16_t)right;break;
        }
        ++j;
        if ((int)right<=c->ranges[j].right) {
            c->ranges[i].right=c->ranges[j].right;break;
        }
    }
    if (j!=i) {
        old=c->count;
        /* Original copies the word at its old exclusive end too. */
        for(k=j+1;k<=old;++k)c->ranges[++i]=c->ranges[k];
        c->count=i+1;
    }
    return 1;
}
#ifndef SSA_SCENE_COVERAGE_ONLY
static int ready(const SsaWorldScene *s)
{
    return s && s->loaded==1 && s->level<16 && s->sky && s->sky->loaded &&
        s->visibility.planes==&s->planes && s->wall_renderer.planes==&s->planes &&
        s->wall_renderer.columns==s->columns &&
        s->wall_renderer.framebuffer==s->framebuffer;
}
int ssa_world_scene_init(SsaWorldScene *s,const SsaMapTextureBindings *b,
    const SsaMathTables *math,const SsaVisibilityTables *vt,
    const SsaWallRenderTables *wt,const SsaSkyAssets *sky,unsigned level)
{
    SsaWorldVisibility visibility={0};SsaPlanePool pool={0};
    if (!s || s->loaded || s->updating || s->visibility.map || s->visibility.stack ||
        s->visibility.vertex_angles || s->visibility.vertex_valid || s->visibility.sector_visited ||
        !b || !b->map || !b->assets || !b->assets->loaded ||
        b->family!=(unsigned)ssa_texture_assets_family(level) ||
        !wt || !wt->loaded || !sky || !sky->loaded ||
        ssa_plane_pool_init(&pool,level)<0) return -1;
    if (ssa_world_visibility_init(&visibility,b->map,math,vt,&pool)<0) return -1;
    memset(s,0,sizeof(*s));s->planes=pool;s->visibility=visibility;
    s->visibility.planes=&s->planes;s->sky=sky;s->level=level;
    s->wall_renderer.bindings=b;s->wall_renderer.tables=wt;
    s->wall_renderer.planes=&s->planes;s->wall_renderer.columns=s->columns;
    s->wall_renderer.framebuffer=s->framebuffer;
    s->wall_renderer.capacity=sizeof(s->framebuffer);
    s->wall_renderer.framebuffer_offset=SSA_SCENE_FRAME_PREFIX;
    s->wall_renderer.openings=s->openings;
    s->wall_renderer.opening_capacity=sizeof(s->openings);
    s->loaded=1;
    return 1;
}
int ssa_world_scene_free(SsaWorldScene *s)
{
    if (!ready(s) || s->updating || s->visibility.updating ||
        s->wall_renderer.updating || s->plane_renderer.updating) return -1;
    if(ssa_world_visibility_free(&s->visibility)<0)return -1;
    memset(s,0,sizeof(*s));return 1;
}
int ssa_world_scene_clear(SsaWorldScene *s,uint8_t index)
{
    if (!ready(s) || s->updating || s->visibility.updating ||
        s->wall_renderer.updating || s->plane_renderer.updating) return -1;
    return ssa_raster_fill_row(s->framebuffer,sizeof(s->framebuffer),
                              SSA_SCENE_FRAME_PREFIX,SSA_SCENE_FRAME_BYTES,index);
}
int ssa_world_scene_draw(SsaWorldScene *s,const SsaRenderCamera *camera,
    uint8_t game_counter,uint16_t player_heading)
{
    uint8_t visited[SSA_VISIBILITY_WALL_LIMIT]={0};
    unsigned index,i;int result=-1;
    if (!ready(s) || s->updating || !camera || s->visibility.updating ||
        s->wall_renderer.updating || s->plane_renderer.updating) return -1;
    s->updating=1;
    for(i=0;i<120;++i) {s->columns[i].top=-1;s->columns[i].bottom=120;}
    s->wall_renderer.eye_z=camera->z;
    if (ssa_world_visibility_build(&s->visibility,camera,game_counter)<0 ||
        ssa_scene_coverage_reset(&s->coverage)<0) goto done;
    index=s->visibility.head;
    while (!(index&0x8000u)) {
        SsaVisibleWall *w;SsaWallRenderCommand c;
        if(index>=SSA_VISIBILITY_WALL_LIMIT || index>=s->visibility.wall_count ||
           visited[index])goto done;
        visited[index]=1;w=&s->visibility.walls[index];
        if(w->range_start>s->visibility.range_count ||
           w->range_count>s->visibility.range_count-w->range_start)goto done;
        if((w->segment_flags&0x8000u) &&
           ssa_scene_coverage_merge(&s->coverage,w->left,w->right)<0)goto done;
        for(i=0;i<w->range_count;++i) {
            s->draw_ranges[i].left=s->visibility.ranges[w->range_start+i].left;
            s->draw_ranges[i].right=s->visibility.ranges[w->range_start+i].right;
        }
        c.depth_left=w->depth_left;c.depth_right=w->depth_right;
        c.u_left=w->u_left;c.u_right=w->u_right;c.left=w->left;c.right=w->right;
        c.segment=w->segment_flags;c.ranges=s->draw_ranges;c.range_count=w->range_count;
        c.ceiling=w->ceiling;c.floor=w->floor;
        if(ssa_wall_render(&s->wall_renderer,&c,&s->results[index])<0)goto done;
        /* BE60 changes working plane globals, not queued record+20/+24.
         * Split handles are retained in results; input wall references persist. */
        index=w->next;
    }
    {
        uint16_t high=(uint16_t)((uint32_t)camera->z>>16);
        int16_t eye=(int16_t)(high<=32767?(int)high:(int)high-65536);
        if(ssa_plane_render(&s->plane_renderer,&s->planes,s->sky,s->level,
             player_heading,eye,s->framebuffer+SSA_SCENE_FRAME_PREFIX,
             SSA_SCENE_FRAME_BYTES)<0)goto done;
    }
    result=1;
done:
    s->updating=0;return result;
}
const uint8_t *ssa_world_scene_pixels(const SsaWorldScene *s)
{return ready(s)?s->framebuffer+SSA_SCENE_FRAME_PREFIX:NULL;}
#endif
