#include "ssa/world.h"
#include "ssa/horizontal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t asr23(int32_t v) { return bits(((uint32_t)v>>23)|(v<0 ? UINT32_MAX<<9 : 0)); }
static uint32_t magnitude(int32_t v) { return v<0 ? 0u-(uint32_t)v : (uint32_t)v; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) { unsigned n=u16(p); return (int16_t)(n<32768 ? (int)n : (int)n-65536); }

int ssa_entity_contact(const SsaEntity *a,const SsaEntity *b,int32_t x,int32_t y) {
    uint32_t radius;
    if (!a || !b) return -1;
    if (a==b || ((b->body.flags&0x10) && (a->body.flags&0x10)) || (b->body.flags&0x20)) return 1;
    if ((b->body.flags&1) && a->type==0x67) return 1;
    if ((b->body.flags&0x10) && (a->body.flags&0x100) && (int)a->player_index-1==b->player_index) return 1;
    if (b->type==0xe1) return 2;
    radius=(uint32_t)a->radius+(uint32_t)b->radius;
    if (magnitude(sub(b->body.x,x))>=radius || magnitude(sub(b->body.y,y))>=radius) return 1;
    if (a->body.flags&0x200) {
        if (a->body.x>add(b->body.z,b->body.height) || add(a->body.x,a->body.height)<b->body.z) return 1;
    }
    return 0;
}

int ssa_world_init(SsaWorld *w,const SsaMapAssets *m,size_t capacity) {
    const uint8_t *b;
    size_t cells;
    SsaWorld n={0};
    if (!w || w->query_active || !m || !m->data || !capacity || capacity>SIZE_MAX/sizeof(SsaEntity *) ||
        m->size<0x48 || m->blockmap>m->size || m->size-m->blockmap<8 || !m->sector_count) return 0;
    b=m->data+m->blockmap;
    if (s16(b+4)<=0 || s16(b+6)<=0) return 0;
    n.width=u16(b+4); n.height=u16(b+6); cells=(size_t)n.width*n.height;
    if (cells>(m->size-m->blockmap-8)/2 || cells>SIZE_MAX/sizeof(SsaEntity *)) return 0;
    n.origin_x=(int32_t)s16(b)*65536; n.origin_y=(int32_t)s16(b+2)*65536;
    n.capacity=capacity;
    n.cells=calloc(cells,sizeof(*n.cells)); n.sectors=calloc(m->sector_count,sizeof(*n.sectors));
    n.special_contacts=calloc(capacity,sizeof(*n.special_contacts));
    if (!n.cells || !n.sectors || !n.special_contacts || !ssa_map_collision_init(&n.collision,m)) {
        ssa_world_free(&n); return 0;
    }
    *w=n; return 1;
}

static size_t cell_at(const SsaWorld *w,int32_t x,int32_t y) {
    uint32_t bx=(uint32_t)sub(x,w->origin_x)>>23, by=(uint32_t)sub(y,w->origin_y)>>23;
    return bx<w->width && by<w->height ? (size_t)by*w->width+bx : SIZE_MAX;
}
static void link_entity(SsaWorld *w,SsaEntity *e,int leaf,int sector) {
    e->leaf=leaf; e->sector=sector; e->cell=cell_at(w,e->body.x,e->body.y); e->owner=w;
    e->sector_previous=NULL; e->sector_next=w->sectors[sector];
    if (e->sector_next) e->sector_next->sector_previous=e;
    w->sectors[sector]=e;
    e->block_previous=NULL; e->block_next=NULL;
    if (e->cell!=SIZE_MAX) {
        e->block_next=w->cells[e->cell];
        if (e->block_next) e->block_next->block_previous=e;
        w->cells[e->cell]=e;
    }
    e->body.step_adjust=0;
    ++w->entity_count;
}
int ssa_world_insert(SsaWorld *w,SsaEntity *e) {
    const SsaMapAssets *m;
    const uint8_t *s;
    int leaf,sector;
    if (!w || w->query_active || !e || e->owner || !(m=w->collision.map) || w->entity_count>=w->capacity || e->radius<0) return 0;
    leaf=ssa_map_leaf(m,e->body.x,e->body.y); sector=ssa_map_sector(m,e->body.x,e->body.y);
    if (leaf<0 || sector<0 || m->sectors>m->size || (size_t)(sector+1)>(m->size-m->sectors)/20) return 0;
    s=m->data+m->sectors+(size_t)sector*20;
    e->body.floor=s16(s); e->body.ceiling=s16(s+2);
    link_entity(w,e,leaf,sector); return 1;
}
static int sector_head(const SsaWorld *w,const SsaEntity *e) {
    unsigned i;
    if(e->sector>=0 && (unsigned)e->sector<w->collision.map->sector_count && w->sectors[e->sector]==e)return e->sector;
    for(i=0;i<w->collision.map->sector_count;++i)if(w->sectors[i]==e)return (int)i;
    return -1;
}
int ssa_world_remove(SsaWorld *w,SsaEntity *e) {
    int head=-1;
    if (!w || !e || e->owner!=w || (w->query_active && e!=w->executing_special)) return 0;
    if(!e->sector_previous && (head=sector_head(w,e))<0)return 0;
    if (e->block_next) e->block_next->block_previous=e->block_previous;
    if (e->block_previous) e->block_previous->block_next=e->block_next;
    else if (e->cell!=SIZE_MAX) w->cells[e->cell]=e->block_next;
    if (e->sector_next) e->sector_next->sector_previous=e->sector_previous;
    if (e->sector_previous) e->sector_previous->sector_next=e->sector_next;
    else w->sectors[head]=e->sector_next;
    e->owner=NULL; e->block_next=e->block_previous=e->sector_next=e->sector_previous=NULL;
    e->cell=SIZE_MAX; e->leaf=e->sector=-1; --w->entity_count;
    return 1;
}
int ssa_world_sector_move(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y,int32_t z,
                          int leaf,int sector,int relink) {
    const SsaMapAssets *m;int head=-1;
    if(!w || w->query_active || !e || e->owner!=w || !(m=w->collision.map) ||
       leaf<0 || (unsigned)leaf>=m->leaf_count || sector<0 || (unsigned)sector>=m->sector_count ||
       (relink!=0 && relink!=1) || m->leaves>m->size || (size_t)(leaf+1)>(m->size-m->leaves)/8 ||
       u16(m->data+m->leaves+(size_t)leaf*8+4)!=(unsigned)sector)return -1;
    if(!e->sector_previous && (head=sector_head(w,e))<0)return -1;
    if(relink){
        if(e->sector_next)e->sector_next->sector_previous=e->sector_previous;
        if(e->sector_previous)e->sector_previous->sector_next=e->sector_next;
        else w->sectors[head]=e->sector_next;
        e->sector_previous=NULL;e->sector_next=w->sectors[sector];
        if(e->sector_next)e->sector_next->sector_previous=e;
        w->sectors[sector]=e;
    }
    e->body.x=x;e->body.y=y;e->body.z=z;e->leaf=leaf;e->sector=sector;
    return 1;
}
int ssa_world_replace(SsaWorld *w,SsaEntity *old,SsaEntity *next) {
    int head=-1;
    if(!w || w->query_active || !old || !next || old==next || old->owner!=w ||
       next->owner || old->sector<0 || !w->collision.map ||
       (unsigned)old->sector>=w->collision.map->sector_count) return 0;
    if(!old->sector_previous && (head=sector_head(w,old))<0)return 0;
    *next=*old;
    if(next->block_previous) next->block_previous->block_next=next;
    else if(next->cell!=SIZE_MAX) w->cells[next->cell]=next;
    if(next->block_next) next->block_next->block_previous=next;
    if(next->sector_previous) next->sector_previous->sector_next=next;
    else w->sectors[head]=next;
    if(next->sector_next) next->sector_next->sector_previous=next;
    if(w->blocking_entity==old) w->blocking_entity=next;
    /* Contact snapshots cannot escape this initialization-only transfer. */
    w->special_count=0;
    old->owner=NULL;
    old->block_next=old->block_previous=old->sector_next=old->sector_previous=NULL;
    old->cell=SIZE_MAX;old->leaf=old->sector=-1;
    return 1;
}
void ssa_world_free(SsaWorld *w) {
    size_t i;
    if (!w || w->query_active) return;
    if (w->collision.map && w->sectors) for (i=0;i<w->collision.map->sector_count;++i)
        while (w->sectors[i]) ssa_world_remove(w,w->sectors[i]);
    free(w->cells); free(w->sectors); free(w->special_contacts);
    ssa_map_collision_free(&w->collision); memset(w,0,sizeof(*w));
}

typedef struct Query { SsaWorld *w; const SsaEntity *e,*ignored; int32_t x,y;unsigned crowd; } Query;
static int separating(const Query *q,const SsaEntity *other) {
    const SsaEntity *e=q->e;int64_t ax,ay,bx,by;
    if(other->type<0xcd || other->type>0xd8 || (other->body.flags&0x10) ||
       ssa_entity_contact(e,other,e->body.x,e->body.y)!=0)return 0;
    if(q->crowd==1 && (e->type<0xcd || e->type>0xd8 || (e->body.flags&0x10)))return 0;
    if(q->crowd==2 && !(e->body.flags&0x10))return 0;
    ax=(int64_t)e->body.x-other->body.x;ay=(int64_t)e->body.y-other->body.y;
    bx=(int64_t)q->x-other->body.x;by=(int64_t)q->y-other->body.y;
    if((ax<0 && bx>0) || (ax>0 && bx<0) || (ay<0 && by>0) || (ay>0 && by<0))return 0;
    if(ax<0)ax=-ax;
    if(ay<0)ay=-ay;
    if(bx<0)bx=-bx;
    if(by<0)by=-by;
    return bx>=ax && by>=ay && (bx>ax || by>ay);
}
static int visit_entities(void *context,const SsaBounds *b) {
    Query *q=context; SsaWorld *w=q->w;
    int margin=(q->crowd?128:32)*65536;
    int left=asr23(sub(sub(b->left,w->origin_x),margin));
    int right=asr23(add(sub(b->right,w->origin_x),margin));
    int bottom=asr23(sub(sub(b->bottom,w->origin_y),margin));
    int top=asr23(add(sub(b->top,w->origin_y),margin));
    int original_left=asr23(sub(sub(b->left,w->origin_x),32*65536));
    int original_right=asr23(add(sub(b->right,w->origin_x),32*65536));
    int original_bottom=asr23(sub(sub(b->bottom,w->origin_y),32*65536));
    int original_top=asr23(add(sub(b->top,w->origin_y),32*65536));
    int x,y;
    if (left<0) left=0;
    if (bottom<0) bottom=0;
    if (right>=(int)w->width) right=(int)w->width-1;
    if (top>=(int)w->height) top=(int)w->height-1;
    for (x=left;x<=right;++x) for (y=bottom;y<=top;++y) {
        SsaEntity *other=w->cells[(size_t)y*w->width+(unsigned)x];
        size_t visited=0;
        for (;other;other=other->block_next) {
            int result;
            if (++visited>w->capacity || other->owner!=w) return -1;
            if(other==q->ignored)continue;
            /* Only large enemy bodies need the wider search. In particular,
             * E1 has cell-based touch semantics: expanding its scan would
             * collect items and trigger scripts before the original range. */
            if(q->crowd && (x<original_left || x>original_right || y<original_bottom || y>original_top) &&
               (other->type<0xcd || other->type>0xd8))continue;
            result=ssa_entity_contact(q->e,other,q->x,q->y);
            if(!result && q->crowd && separating(q,other))continue;
            if (!result) { w->blocking_entity=other; return 0; }
            if (result==2) {
                if (w->special_count>=w->capacity) return -1;
                w->special_contacts[w->special_count++]=other;
                if (w->observe_special) w->observe_special(w->contact_context,q->e,other);
                if (w->execute_special) {
                    w->executing_special=other;
                    result=w->execute_special(w->execute_context,q->e,other);
                    w->executing_special=NULL;
                    if (result) return -1;
                    /* 09870 reads next AFTER the handler. The original pickup
                     * release zeroes the removed entity, ending this cell. */
                    if (!other->owner) break;
                    if (other->owner!=w) return -1;
                }
            }
        }
    }
    return 1;
}
static int query(SsaWorld *w,const SsaEntity *e,int32_t x,int32_t y,const SsaEntity *ignored,unsigned crowd) {
    Query q;
    int result;
    if (!w || w->query_active || !e || !w->collision.map || (e->owner && e->owner!=w) ||
        (ignored && ignored->owner!=w)) return -1;
    w->blocking_entity=NULL; w->special_count=0;
    q.w=w; q.e=e; q.ignored=ignored; q.x=x; q.y=y;q.crowd=crowd;
    w->query_active=1;
    result=ssa_map_query_visit(&w->collision,e->body.x,e->body.y,x,y,e->radius,visit_entities,&q);
    w->query_active=0;
    return result;
}
int ssa_world_query(SsaWorld *w,const SsaEntity *e,int32_t x,int32_t y) {
    return query(w,e,x,y,NULL,0);
}
static int try_move(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y,const SsaEntity *ignored,unsigned crowd) {
    int result,leaf,sector;
    if (!w || w->query_active || !e || e->owner!=w) return -1;
    result=query(w,e,x,y,ignored,crowd);
    if (result!=1) return result;
    leaf=ssa_map_leaf(w->collision.map,x,y); sector=ssa_map_sector(w->collision.map,x,y);
    if (leaf<0 || sector<0) return -1;
    result=ssa_map_commit_move(&w->collision,&e->body,x,y);
    if (result!=1) return result;
    /* Removal uses saved cell/sector metadata, not the newly committed X/Y. */
    ssa_world_remove(w,e); link_entity(w,e,leaf,sector);
    return 1;
}
int ssa_world_try_move_ignoring(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y,const SsaEntity *ignored) {
    return try_move(w,e,x,y,ignored,0);
}
int ssa_world_try_move(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y) {
    return ssa_world_try_move_ignoring(w,e,x,y,NULL);
}
int ssa_world_try_move_crowd(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y) {
    if(!e || e->type<0xcd || e->type>0xd8 || (e->body.flags&0x10))return -1;
    return try_move(w,e,x,y,NULL,1);
}
int ssa_world_try_move_player(SsaWorld *w,SsaEntity *e,int32_t x,int32_t y) {
    if(!e || !(e->body.flags&0x10))return -1;
    return try_move(w,e,x,y,NULL,2);
}

typedef struct Motion {
    SsaWorld *world;
    SsaEntity *entity;
    SsaSlide *slide;
    const SsaMathTables *tables;
    unsigned player_clearance;
} Motion;
static int move_body(void *context,SsaBody *body,int32_t x,int32_t y) {
    Motion *motion=context;
    if (body!=&motion->entity->body) return -1;
    return motion->player_clearance?ssa_world_try_move_player(motion->world,motion->entity,x,y):
        ssa_world_try_move(motion->world,motion->entity,x,y);
}
static int hit_entity(void *context) {
    return ((Motion *)context)->world->blocking_entity!=NULL;
}
static int slide_body(void *context,SsaBody *body) {
    Motion *motion=context;
    return ssa_slide_move(motion->slide,body,motion->entity->radius,motion->tables,move_body,context);
}
static int horizontal_step(SsaWorld *w,SsaEntity *e,SsaSlide *slide,const SsaMathTables *tables,
                               uint8_t ticks,uint8_t count,uint8_t local,unsigned player) {
    Motion motion={w,e,slide,tables,player};
    SsaHorizontalOps ops={move_body,hit_entity,slide_body,&motion};
    if (!w || w->query_active || !e || e->owner!=w || !tables || !slide || slide->map!=w->collision.map) return -1;
    return ssa_horizontal_step(&e->body,e->player_index,ticks,count,local,&ops);
}
int ssa_world_horizontal_step(SsaWorld *w,SsaEntity *e,SsaSlide *slide,const SsaMathTables *tables,
                               uint8_t ticks,uint8_t count,uint8_t local) {
    return horizontal_step(w,e,slide,tables,ticks,count,local,0);
}
int ssa_world_horizontal_step_player(SsaWorld *w,SsaEntity *e,SsaSlide *slide,const SsaMathTables *tables,
                               uint8_t ticks,uint8_t count,uint8_t local) {
    if(!e || !(e->body.flags&0x10))return -1;
    return horizontal_step(w,e,slide,tables,ticks,count,local,1);
}
