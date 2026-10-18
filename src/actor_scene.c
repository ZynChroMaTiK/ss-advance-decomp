#include "ssa/actor_scene.h"
#include "ssa/entity_pool.h"
#include <string.h>

static int ready(const SsaActorScene *s)
{
    return s && s->loaded==1 && s->geometry && s->geometry->loaded==1 &&
        s->world && s->world->cells && s->world->sectors && s->assets &&
        s->assets->loaded==1 && s->geometry->visibility.map==s->world->collision.map &&
        s->projector.assets==s->assets && s->projector.queue==&s->queue &&
        s->projector.math;
}
int ssa_actor_scene_init(SsaActorScene *s,SsaWorldScene *g,const SsaWorld *w,
    const SsaActorAssets *a,const SsaMathTables *math)
{
    if (!s || s->loaded || s->updating || s->geometry || s->world || s->assets ||
        !g || g->loaded!=1 || g->updating || g->visibility.updating ||
        g->wall_renderer.updating || g->plane_renderer.updating ||
        !w || !w->cells || !w->sectors || w->query_active ||
        g->visibility.map!=w->collision.map || !a || a->loaded!=1 || !math) return -1;
    memset(s,0,sizeof(*s));s->geometry=g;s->world=w;s->assets=a;
    s->projector.assets=a;s->projector.math=math;s->projector.queue=&s->queue;
    s->loaded=1;return 1;
}
int ssa_actor_scene_free(SsaActorScene *s)
{
    if(!ready(s) || s->updating || s->queue.updating || s->projector.updating ||
       s->geometry->updating || s->geometry->visibility.updating ||
       s->geometry->wall_renderer.updating || s->geometry->plane_renderer.updating ||
       s->world->query_active)return -1;
    memset(s,0,sizeof(*s));return 1;
}
int ssa_actor_scene_draw_offsets(SsaActorScene *s,const SsaRenderCamera *camera,uint32_t game_flags,
    const SsaActorSource *sources,size_t count,const int32_t *z_offsets)
{
    int result=-1;
    if(!ready(s) || s->updating || s->queue.updating || s->projector.updating ||
       s->geometry->updating || s->geometry->visibility.updating ||
       s->geometry->wall_renderer.updating || s->geometry->plane_renderer.updating ||
       s->world->query_active || !camera || count>SSA_ENTITY_POOL_CAPACITY ||
       (!sources && count))return -1;
    s->updating=1;
    if(ssa_actor_queue_begin(&s->queue,&s->geometry->visibility)<0)goto done;
    if(ssa_actor_queue_bind_clips(&s->queue,s->geometry->results,
       s->geometry->visibility.wall_count,&s->geometry->wall_renderer,s->assets)<0)goto done;
    s->projector.camera=camera;s->projector.game_flags=game_flags;
    if((s->screen_mirrors?ssa_actor_collect_mirrored(&s->projector,s->world,sources,count,z_offsets):
        ssa_actor_collect_offsets(&s->projector,s->world,sources,count,z_offsets))<0)goto done;
    if(ssa_actor_queue_draw(&s->queue,s->assets,
       s->geometry->framebuffer+SSA_SCENE_FRAME_PREFIX,SSA_SCENE_FRAME_BYTES)<0)goto done;
    result=1;
done:
    s->projector.camera=NULL;s->updating=0;return result;
}
int ssa_actor_scene_draw(SsaActorScene *s,const SsaRenderCamera *camera,uint32_t game_flags,
    const SsaActorSource *sources,size_t count)
{return ssa_actor_scene_draw_offsets(s,camera,game_flags,sources,count,NULL);}
