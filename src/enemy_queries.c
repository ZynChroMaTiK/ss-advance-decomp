#include "ssa/enemy_queries.h"
#include <string.h>

static int ready(const SsaEnemyQueries *q) {
    const SsaScriptSpawn *s=q ? q->spawn : NULL;
    return q && !q->updating && !q->path.updating && !q->ray.updating &&
        !q->ray.traversal.updating && !q->ray.walls.updating &&
        s && s->level && s->level->map && s->world &&
        q->math && q->reserved_count==s->reserved && s->reserved<=100 &&
        s->world->cells && s->world->sectors && !s->world->query_active &&
        !s->level->updating && !s->level->script.updating &&
        !s->level->movers.updating && !s->level->switches.updating &&
        s->world->collision.map==s->level->map && q->ray.walls.map==s->level->map;
}
static int ordered(const SsaEnemyQueries *q,SsaEntity **out) {
    const SsaScriptSpawn *s=q->spawn;
    unsigned i,j,slot,n=s->pool.active_count;
    if (n>100 || n!=s->world->entity_count) return -1;
    for (i=0;i<n;++i) {
        slot=s->pool.active_slots[i];
        if (slot>=100 || !s->pool.occupied[slot] || s->pool.positions[slot]!=i) return -1;
        out[i]=ssa_script_spawn_entity(q->spawn,q->reserved,slot);
        if (!out[i] || out[i]->owner!=s->world) return -1;
        for (j=0;j<i;++j) if (out[j]==out[i]) return -1;
    }
    return 0;
}
static int visibility(void *context,int player,int enemy,int *blocked) {
    SsaEnemyQueries *q=context;
    int result;
    if (!ready(q) || !blocked) return -1;
    q->updating=1;
    result=ssa_sector_reject(q->spawn->level->map,player,enemy);
    if (result>=0) *blocked=result;
    q->updating=0; return result<0 ? -1 : 0;
}
static int nearest(void *context,SsaEntity *source,SsaEntity **out) {
    SsaEnemyQueries *q=context;
    SsaEntity *entities[100];
    int result;
    if (!ready(q) || !source || !out || source->owner!=q->spawn->world) return -1;
    q->updating=1;
    result=ordered(q,entities);
    if (!result) result=ssa_enemy_nearest(source,entities,q->spawn->pool.active_count,out);
    q->updating=0; return result;
}
static int ray(void *context,const SsaEnemyPath *p,int32_t x,int32_t y,
                int32_t ex,int32_t ey,int *clear) {
    SsaEnemyQueries *q=context;
    int result;
    (void)p;
    result=ssa_enemy_ray_query(&q->ray,x,y,ex,ey);
    if (result<0) return -1;
    *clear=result; return 0;
}
static int path(void *context,const SsaEnemyNavigation *nav,int32_t x,int32_t y,
                 int32_t ex,int32_t ey,int32_t radius,unsigned special,unsigned mode,
                 int *mask,int32_t *dx,int32_t *dy) {
    SsaEnemyQueries *q=context;
    int result;
    if (!ready(q) || !nav || !nav->entity || nav->entity->owner!=q->spawn->world ||
        !mask || !dx || !dy) return -1;
    q->updating=1;
    result=ssa_enemy_path_query(&q->path,q->math,x,y,ex,ey,radius,special,mode,ray,q,mask);
    if (result>=0) { *dx=q->path.offset_x; *dy=q->path.offset_y; }
    q->updating=0; return result<0 ? -1 : 0;
}
int ssa_enemy_queries_init(SsaEnemyQueries *q,SsaScriptSpawn *s,const SsaMathTables *m,
                            SsaEntity *const *reserved,size_t count) {
    SsaEntity *entities[100];
    unsigned i;
    if (!q || q->spawn || q->updating || q->ray.walls.map || !s || !s->level ||
        !s->world || s->updating || !m || count!=s->reserved || count>100 ||
        (count && !reserved)) return -1;
    if (ssa_enemy_ray_init(&q->ray,s->level->map)<0) return -1;
    q->spawn=s; q->math=m; q->reserved_count=(unsigned)count;
    for (i=0;i<count;++i) q->reserved[i]=reserved[i];
    if (!ready(q) || ordered(q,entities)<0) {
        ssa_enemy_ray_free(&q->ray); memset(q,0,sizeof(*q)); return -1;
    }
    q->ops=(SsaEnemyNavigationOps){visibility,nearest,path,NULL,NULL,NULL,q};
    return 0;
}
int ssa_enemy_queries_free(SsaEnemyQueries *q) {
    if (!q || q->updating || q->path.updating || (q->spawn &&
        (q->spawn->updating || !ready(q)))) return -1;
    if (ssa_enemy_ray_free(&q->ray)<0) return -1;
    memset(q,0,sizeof(*q)); return 0;
}
