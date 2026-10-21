#include "ssa/actor_queue.h"
#include "ssa/actor_raster.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(SsaActorProjection)==36,"original actor projection");
_Static_assert(sizeof(SsaActorRecord)==12,"original actor record");
static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static int s16(uint16_t n){return n<32768?n:(int)n-65536;}
static int s8(uint8_t n){return n<128?n:(int)n-256;}
static int32_t bits(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static uint32_t rd32(const uint8_t *p){return (uint32_t)rd16(p)|(uint32_t)rd16(p+2)<<16;}
static void wr32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static int section(const SsaMapAssets *m,uint32_t o,size_t n,size_t stride)
{return o<=m->size && n<=(m->size-o)/stride;}
static int valid(const SsaActorQueue *q)
{
    return q && q->visibility && q->visibility->map && q->visibility->map->data &&
        !q->visibility->updating && q->visibility->wall_count<=SSA_VISIBILITY_WALL_LIMIT &&
        q->actor_count<=SSA_ACTOR_QUEUE_LIMIT && q->projection_count<=SSA_ACTOR_QUEUE_LIMIT &&
        q->clip_cursor<=SSA_ACTOR_CLIP_WORDS;
}
static int links(SsaActorQueue *q,unsigned index,uint16_t **previous,uint16_t **next)
{
    if(index==65535)return -1;
    if(index&0x8000) {
        index&=0x7fff;if(index>=q->actor_count)return -1;
        *previous=&q->actors[index].previous;*next=&q->actors[index].next;
    } else {
        if(index>=q->visibility->wall_count)return -1;
        *previous=&q->visibility->walls[index].previous;*next=&q->visibility->walls[index].next;
    }
    return 1;
}
int ssa_actor_queue_begin(SsaActorQueue *q,SsaWorldVisibility *v)
{
    unsigned index,count=0,previous=65535;
    if(!q || q->updating || !v || v->updating || !v->map || !v->map->data ||
        v->wall_count>SSA_VISIBILITY_WALL_LIMIT)return -1;
    index=v->head;
    while(index!=65535) {
        if(index>=v->wall_count || ++count>v->wall_count || v->walls[index].previous!=previous)return -1;
        previous=index;index=v->walls[index].next;
    }
    if(v->tail!=previous)return -1;
    q->visibility=v;q->actor_count=q->projection_count=q->draw_count=0;q->clip_cursor=0;q->fixed_screen_mask=0;return 1;
}
int ssa_actor_queue_allocate(SsaActorQueue *q)
{
    if(!valid(q) || q->updating || q->actor_count>=SSA_ACTOR_QUEUE_LIMIT)return -1;
    return q->actor_count++;
}
static int clip_view(const uint8_t *source,const SsaWallRenderer *r,const SsaActorAssets *a,
    const uint8_t **out,size_t *size)
{
    uintptr_t address=(uintptr_t)source,base=(uintptr_t)r->openings;
    if(!source){*out=NULL;*size=0;return 1;}
    if(source==r->tables->closed_top){*out=a->closed_top;*size=sizeof(a->closed_top);return 1;}
    if(source==r->tables->closed_bottom){*out=a->closed_bottom;*size=sizeof(a->closed_bottom);return 1;}
    if(!r->openings || address<base || address-base>r->opening_capacity)return -1;
    *out=source;*size=r->opening_capacity-(size_t)(address-base);return 1;
}
int ssa_actor_queue_bind_clips(SsaActorQueue *q,const SsaWallRenderResult *results,size_t count,
    const SsaWallRenderer *r,const SsaActorAssets *a)
{
    unsigned index,seen=0;
    if(!valid(q) || q->updating || !results || !r || r->updating || !r->tables ||
        !r->tables->loaded || !a || !a->loaded)return -1;
    index=q->visibility->head;
    while(index!=65535) {
        const SsaWallRenderResult *result;SsaActorWallClip *clip;
        if(index>=q->visibility->wall_count || index>=count || ++seen>q->visibility->wall_count)return -1;
        result=results+index;clip=q->wall_clips+index;
        clip->flags=result->sprite_clip_flags;clip->bottom_height=result->bottom_height;clip->top_height=result->top_height;
        clip->top_x=result->top_clip_x;clip->bottom_x=result->bottom_clip_x;
        if(clip_view(result->top_clip,r,a,&clip->top,&clip->top_size)<0 ||
            clip_view(result->bottom_clip,r,a,&clip->bottom,&clip->bottom_size)<0)return -1;
        index=q->visibility->walls[index].next;
    }
    return 1;
}
static int side(const SsaActorQueue *q,unsigned segment,int32_t x,int32_t y)
{
    const SsaMapAssets *m=q->visibility->map;const uint8_t *seg,*line,*a,*b;unsigned li,ai,bi;
    if(segment>=m->segment_count || !section(m,m->segments,m->segment_count,16) ||
        !section(m,m->lines,m->line_count,40) || !section(m,m->vertices,m->vertex_count,4))return -1;
    seg=m->data+m->segments+segment*16;li=rd16(seg+4);
    if(li>=m->line_count || rd16(seg+8)>1)return -1;
    line=m->data+m->lines+li*40;ai=rd16(line);bi=rd16(line+2);
    if(ai>=m->vertex_count || bi>=m->vertex_count)return -1;
    a=m->data+m->vertices+ai*4;b=m->data+m->vertices+bi*4;
    return ssa_bsp_side(x,y,s16(rd16(a)),s16(rd16(a+2)),
        s16(rd16(b))-s16(rd16(a)),s16(rd16(b+2))-s16(rd16(a+2)))^rd16(seg+8);
}
static int clip_byte(const uint8_t *p,size_t size,int origin,int x,int *out)
{
    if(!p || x<origin || (size_t)(x-origin)>=size)return -1;
    *out=s8(p[x-origin]);return 1;
}
static int insert(SsaActorQueue *q,unsigned actor)
{
    SsaActorRecord *record=&q->actors[actor];SsaActorProjection *p;
    SsaWorldVisibility *v=q->visibility;uint8_t scratch[248]={0};
    uint8_t seen_walls[SSA_VISIBILITY_WALL_LIMIT]={0},seen_actors[SSA_ACTOR_QUEUE_LIMIT]={0};
    unsigned index=v->head,previous,new_index=actor|0x8000;uint16_t *prev,*next;
    int left,right,x;size_t words;
    if(record->projection>=q->projection_count)return -1;
    p=q->projections+record->projection;left=p->left;right=left+p->clipped_width+1;
    if(left<0 || left>=120 || p->clipped_width<=0 || left+p->clipped_width>120)return -1;
    words=(size_t)(p->clipped_width+2)*2;
    if(words>SSA_ACTOR_CLIP_WORDS-q->clip_cursor || q->clip_cursor>65535)return -1;
    /* Original two120-byte arrays share stack storage with the world point.
     * Extra right-edge columns alias the first top bytes/world-point bytes. */
    for(x=left;x<=right;++x){scratch[x]=120;scratch[120+x]=255;}
    wr32(scratch+240,(uint32_t)(uint16_t)p->world_x<<16);
    wr32(scratch+244,(uint32_t)(uint16_t)p->world_y<<16);
    while(index!=65535) {
        if(links(q,index,&prev,&next)<0)return -1;
        if(index&0x8000) {
            unsigned i=index&0x7fff;
            if(i==actor || seen_actors[i])return -1;
            seen_actors[i]=1;
            if(record->depth>=q->actors[i].depth)break;
        } else {
            const SsaVisibleWall *w=v->walls+index;const SsaActorWallClip *clip=q->wall_clips+index;
            int low,high,behind,from,to;uint32_t flags;
            if(seen_walls[index])return -1;
            seen_walls[index]=1;
            if(w->left>right || w->right<left){index=*next;continue;}
            low=w->depth_left;high=w->depth_right;if(low>high){int t=low;low=high;high=t;}
            if(record->depth<low)behind=1;
            else if(record->depth>high)behind=0;
            else behind=side(q,w->segment_flags&0x7fff,bits(rd32(scratch+240)),bits(rd32(scratch+244)));
            if(behind<0)return -1;
            if(behind==1) {
                from=w->left>left?w->left:left;to=w->right<right?w->right:right;
                if(to>120)return -1;
                flags=clip->flags;
                if(flags==3 && clip->bottom_height==32767 && clip->top_height==INT16_MIN) {
                    for(x=from;x<=to;++x){scratch[120+x]=120;scratch[x]=255;}
                } else {
                    if(p->bottom_z>=clip->bottom_height)flags&=~1u;
                    if(p->top_z<=clip->top_height)flags&=~2u;
                    if(flags>=1 && flags<=3)for(x=from;x<=to;++x) {
                        int value;
                        if(flags&1) {
                            if(clip_byte(clip->bottom,clip->bottom_size,clip->bottom_x,x,&value)<0)return -1;
                            if(s8(scratch[x])>value)scratch[x]=(uint8_t)value;
                        }
                        if(flags&2) {
                            if(clip_byte(clip->top,clip->top_size,clip->top_x,x,&value)<0)return -1;
                            if(s8(scratch[120+x])<value)scratch[120+x]=(uint8_t)value;
                        }
                    }
                }
            }
        }
        index=*next;
    }
    for(x=left;x<=right;++x) {
        if(s8(scratch[x])==-2)scratch[x]=120;
        if(s8(scratch[120+x])==-2)scratch[120+x]=255;
    }
    p->clip_offset=record->clip_offset=(uint16_t)q->clip_cursor;
    for(x=left;x<=right;++x) {
        unsigned at=q->clip_cursor+(unsigned)(x-left)*2;
        q->clip_words[at]=(int16_t)s8(scratch[120+x]);q->clip_words[at+1]=(int16_t)s8(scratch[x]);
    }
    q->clip_cursor+=(uint32_t)p->clipped_width*2;
    if(index!=65535) {
        if(links(q,index,&prev,&next)<0)return -1;
        previous=*prev;*prev=(uint16_t)new_index;
    } else previous=v->tail;
    if(previous!=65535) {
        if(links(q,previous,&prev,&next)<0)return -1;
        *next=(uint16_t)new_index;
    }
    record->previous=(uint16_t)previous;record->next=(uint16_t)index;
    if(v->head==index)v->head=(uint16_t)new_index;
    if(v->tail==previous)v->tail=(uint16_t)new_index;
    return 1;
}
int ssa_actor_queue_insert(SsaActorQueue *q,unsigned actor)
{
    int result;
    if(!valid(q) || q->updating || actor>=q->actor_count)return -1;
    q->updating=1;result=insert(q,actor);q->updating=0;return result;
}
int ssa_actor_queue_order(SsaActorQueue *q)
{
    unsigned index,count=0;uint16_t *prev,*next;
    uint8_t seen_walls[SSA_VISIBILITY_WALL_LIMIT]={0},seen_actors[SSA_ACTOR_QUEUE_LIMIT]={0};
    if(!valid(q) || q->updating)return -1;
    index=q->visibility->head;q->draw_count=0;q->updating=1;
    while(index!=65535) {
        if(links(q,index,&prev,&next)<0)goto failure;
        if(index&0x8000) {
            unsigned actor=index&0x7fff;
            if(seen_actors[actor] || count>=SSA_ACTOR_QUEUE_LIMIT)goto failure;
            seen_actors[actor]=1;q->draw_order[count++]=(uint16_t)actor;
        } else {
            if(seen_walls[index])goto failure;
            seen_walls[index]=1;
        }
        index=*next;
    }
    q->draw_count=(uint16_t)count;q->updating=0;return 1;
failure:q->updating=0;return -1;
}
int ssa_actor_queue_draw_layers(SsaActorQueue *q,const SsaActorAssets *assets,uint8_t *framebuffer,size_t size,
    SsaActorUnderlay underlay,void *context)
{
    unsigned i;int result=-1;
    if(!assets || !assets->loaded || !framebuffer || size<14400 || ssa_actor_queue_order(q)<0)return -1;
    q->updating=1;
    for(i=q->draw_count;i>0;--i) {
        const SsaActorRecord *record=q->actors+q->draw_order[i-1];
        if(record->projection>=q->projection_count || record->clip_offset>=SSA_ACTOR_CLIP_WORDS)goto done;
        if(underlay && underlay(context,q,q->draw_order[i-1],framebuffer,size)<0)goto done;
        if(ssa_actor_raster_draw(assets,q->projections+record->projection,
            q->clip_words+record->clip_offset,SSA_ACTOR_CLIP_WORDS-record->clip_offset,framebuffer,size)<0)goto done;
    }
    result=1;
done:q->updating=0;return result;
}

int ssa_actor_queue_draw(SsaActorQueue *q,const SsaActorAssets *assets,uint8_t *fb,size_t size)
{return ssa_actor_queue_draw_layers(q,assets,fb,size,NULL,NULL);}
int ssa_actor_queue_sprite_clips(const SsaActorQueue *q,int32_t depth,int32_t wx,int32_t wy,
    int bottom,int top,int left,int right,int16_t clips[240])
{
    unsigned index,seen=0;int x;
    if(!valid(q) || !clips || left<0 || right>=120 || right<left)return -1;
    for(x=left;x<=right;++x){clips[x*2]=-1;clips[x*2+1]=120;}
    index=q->visibility->head;
    while(index!=65535){const SsaVisibleWall *w;const SsaActorWallClip *c;int low,high,behind,from,to;unsigned flags;
        if(++seen>q->actor_count+q->visibility->wall_count)return -1;
        if(index&0x8000){unsigned a=index&0x7fff;if(a>=q->actor_count)return -1;index=q->actors[a].next;continue;}
        if(index>=q->visibility->wall_count)return -1;
        w=&q->visibility->walls[index];c=&q->wall_clips[index];index=w->next;
        if(w->left>right || w->right<left)continue;
        low=w->depth_left;high=w->depth_right;if(low>high){int t=low;low=high;high=t;}
        behind=depth<low?1:depth>high?0:side(q,w->segment_flags&0x7fff,wx,wy);
        if(behind<0)return -1;
        if(!behind)continue;
        from=w->left>left?w->left:left;to=w->right<right?w->right:right;flags=c->flags;
        if(flags==3 && c->bottom_height==32767 && c->top_height==INT16_MIN){
            for(x=from;x<=to;++x){clips[x*2]=120;clips[x*2+1]=-1;}continue;}
        if(bottom>=c->bottom_height)flags&=~1u;
        if(top<=c->top_height)flags&=~2u;
        if(flags>=1 && flags<=3)for(x=from;x<=to;++x){int value;
            if(flags&1){if(clip_byte(c->bottom,c->bottom_size,c->bottom_x,x,&value)<0)return -1;
                if(value!=-2 && clips[x*2+1]>value)clips[x*2+1]=(int16_t)value;}
            if(flags&2){if(clip_byte(c->top,c->top_size,c->top_x,x,&value)<0)return -1;
                if(value!=-2 && clips[x*2]<value)clips[x*2]=(int16_t)value;}}
    }
    return 1;
}
