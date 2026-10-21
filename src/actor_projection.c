#include "ssa/actor_projection.h"
#include <limits.h>

static int32_t bits(uint32_t u){return u<=INT32_MAX?(int32_t)u:-1-(int32_t)~u;}
static int16_t half(uint32_t u){uint16_t n=(uint16_t)u;return n<=INT16_MAX?(int16_t)n:(int16_t)(-1-(int)(uint16_t)~n);}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return bits((uint32_t)a-(uint32_t)b);}
static int32_t asr(int32_t a,unsigned n){return bits(((uint32_t)a>>n)|(a<0?UINT32_MAX<<(32-n):0));}
static int32_t mul(int32_t a,int32_t b,unsigned n){return bits((uint32_t)((uint64_t)((int64_t)a*b)>>n));}
static int32_t dot(int32_t x,int32_t y,int32_t a,int32_t b){
    uint64_t p=(uint64_t)((int64_t)x*a)+(uint64_t)((int64_t)y*b);
    return bits((uint32_t)(p>>16));
}
static int valid(const SsaActorProjector *p){
    return p && !p->updating && p->assets && p->assets->loaded && p->math &&
        p->camera && p->queue && !p->queue->updating &&
        p->queue->projection_count<=32 && p->queue->actor_count<=32 &&
        p->queue->clip_cursor<=SSA_ACTOR_CLIP_WORDS && p->queue->visibility &&
        !p->queue->visibility->updating && p->queue->visibility->map &&
        p->queue->visibility->map->data &&
        p->queue->visibility->wall_count<=SSA_VISIBILITY_WALL_LIMIT;
}
static int emit(SsaActorProjector *p,const SsaActorSource *s,int32_t x,int32_t y,
                int32_t w,int32_t h,uint32_t frame,int32_t scale,int32_t depth,int fixed){
    const SsaBody *b=&s->entity->body;SsaActorProjection *v;int32_t left=x,width=w;unsigned index;int actor;
    if(x>119 || add(x,w)<0 || y>119 || add(y,h)<0)return 1;
    if(left<0){width=add(width,left);left=0;}
    if(add(left,width)>120)width=sub(add(width,120),add(left,width));
    if(width<=0)return 1;
    index=p->queue->projection_count++;v=&p->queue->projections[index];
    v->x=half((uint32_t)x);v->y=half((uint32_t)y);v->width=half((uint32_t)w);v->height=half((uint32_t)h);
    v->frame_flags=frame;v->scale=scale;
    v->world_x=fixed?0:half((uint32_t)b->x>>16);v->world_y=fixed?0:half((uint32_t)b->y>>16);
    v->bottom_z=half((uint32_t)b->z>>16);v->top_z=half((uint32_t)add(b->z,b->height)>>16);
    v->animation=s->animation;v->clip_offset=0;v->left=half((uint32_t)left);v->clipped_width=half((uint32_t)width);
    actor=ssa_actor_queue_allocate(p->queue);if(actor<0)return 1;
    if(fixed)p->queue->fixed_screen_mask|=UINT32_C(1)<<(unsigned)actor;
    p->queue->actors[actor].projection=(uint16_t)index;
    p->queue->actors[actor].clip_offset=0;p->queue->actors[actor].depth=depth;
    return ssa_actor_queue_insert(p->queue,(unsigned)actor)<0?-1:1;
}
static int project(SsaActorProjector *p,const SsaActorSource *s,int fixed,int screen_mirror,int screen_x,int screen_y){
    const SsaBody *b;const SsaActorResource *r;const SsaActorImage *im;
    int32_t dx,dy,lateral,depth,scale,mscale=0,x,y,w,h,origin,dir,mip=0;
    uint32_t image;unsigned flip=0,dirs;uint16_t facing;
    if(!s || !s->entity)return -1;
    if(!s->animation || s->animation==65535 || s->frame>=65534)return 1;
    if(!fixed && (((s->flags&64) && (p->game_flags&1)) || (s->flags&128)))return 1;
    if(p->queue->projection_count>=32)return 1;
    if(s->animation>=SSA_ACTOR_RESOURCE_COUNT)return -1;
    r=&p->assets->resources[s->animation];b=&s->entity->body;
    if(fixed){
        image=(uint32_t)s->frame+1;if(image>3000)return 1;
        im=ssa_actor_assets_image(p->assets,s->animation,image);if(!im)return -1;
        x=60+asr(im->origin_x,1);w=im->width/2;
        if(screen_mirror && (s->flags&SSA_ACTOR_SCREEN_MIRROR)){
            x=120-x-w;image|=UINT32_C(0x80000000);}
        return emit(p,s,add(x,screen_x),add(60+asr(mul((int32_t)im->origin_y*4096,3072,12),12),screen_y),
            w,asr(mul(3072,(int32_t)im->height*4096,12),12),image,2048,65536,1);
    }
    dx=sub(b->x,p->camera->x);dy=sub(b->y,p->camera->y);
    lateral=dot(dx,dy,p->camera->cosine,p->camera->sine);
    depth=dot(dx,dy,sub(0,p->camera->sine),p->camera->cosine);
    if(depth<0)return 1;
    scale=bits(p->assets->reciprocal[(uint32_t)depth>>16]>>8);
    scale=bits((uint32_t)mul(0x55b05a,scale,16)>>8);
    if(r->mip_flags&1){++mip;mscale=mul(scale,0xaaaa,16);if(scale>0x5fff)goto chosen;}
    if(r->mip_flags&2){++mip;mscale=scale;if(scale>0x3fff)goto chosen;}
    if(r->mip_flags&4){++mip;mscale=mul(scale,0x15555,16);if(scale>0x2fff)goto chosen;}
    if(r->mip_flags&8){++mip;mscale=bits((uint32_t)scale<<1);if(scale>0x1fff)goto chosen;}
    if(r->mip_flags&16){++mip;mscale=bits((uint32_t)scale<<2);}
chosen:
    --mip;mscale=asr(mscale,3);
    x=add(mul(asr(lateral,16),scale,16),60);
    y=asr(sub(60*4096,mul(asr(sub(b->z,p->camera->z),4),add(scale,asr(scale,1)),16)),12);
    facing=(uint16_t)((unsigned)s->facing<<8);if(s->flags&16)facing=(uint16_t)(facing+0x8000);
    dir=(int32_t)(((uint32_t)(uint16_t)(facing-ssa_direction(p->math,half((uint32_t)dx>>16),half((uint32_t)dy>>16)))+0x800)>>12);
    dir&=15;if(dir>8){dir=16-dir;flip=1;}
    if(!r->directions || !r->mip_flags)return -1;
    dirs=(uint16_t)(r->directions-1);
    if(!dirs){dir=0;flip=0;}else while(dirs<=7){dirs=(uint16_t)(dirs*2);dir=asr(dir,1);}
    image=(uint32_t)((int32_t)s->frame+1+dir*r->frames+mip*(int32_t)(r->frames*r->directions));
    if(bits(image)>3000)return 1;
    im=ssa_actor_assets_image(p->assets,s->animation,image);if(!im)return -1;
    origin=im->origin_x;if(flip)origin=-(origin+im->width);
    w=mul(mscale,im->width,12);x=add(x,mul(origin,mscale,12));
    if(x>119 || add(x,w)<0)return 1;
    h=im->height;
    if(b->step_adjust){int32_t cut=b->floor+b->step_adjust-asr(b->z,16);if(cut>0)h=sub(h,cut);}
    depth=scale;scale=add(mscale,asr(mscale,1));
    h=mul(scale,h,12);y=add(y,mul(im->origin_y,scale,12));
    return emit(p,s,x,y,w,h,image|((uint32_t)flip<<31),mscale,depth,0);
}
int ssa_actor_project(SsaActorProjector *p,const SsaActorSource *s,int fixed){
    int rc;if(!valid(p))return -1;p->updating=1;rc=project(p,s,fixed!=0,0,0,0);p->updating=0;return rc;
}
static int collect_offsets(SsaActorProjector *p,const SsaWorld *world,const SsaActorSource *sources,size_t count,const int32_t *z_offsets,int screen_mirror,int screen_x,int screen_y){
    size_t i,j,steps=0;unsigned sector;int rc=1;const SsaWorldVisibility *v;
    if(!valid(p)||!world||world->query_active||!world->collision.map||!world->sectors||(!sources&&count))return -1;
    v=p->queue->visibility;if(!v||v->updating||v->map!=world->collision.map||!v->sector_visited||v->sector_words<((size_t)v->map->sector_count+15)/16)return -1;
    for(i=0;i<count;++i){if(!sources[i].entity)return -1;for(j=0;j<i;++j)if(sources[i].entity==sources[j].entity)return -1;}
    p->updating=1;p->queue->clip_cursor=0;
    for(sector=0;sector<v->map->sector_count;++sector){const SsaEntity *e;
        if(!(v->sector_visited[sector/16]&(1u<<(sector%16))))continue;
        for(e=world->sectors[sector];e;e=e->sector_next){SsaActorSource second,view;SsaEntity display;
            if(++steps>world->entity_count||e->owner!=world||e->sector!=(int)sector){rc=-1;goto done;}
            for(i=0;i<count&&sources[i].entity!=e;++i){}
            if(i==count){rc=-1;goto done;}
            view=sources[i];
            if(z_offsets && z_offsets[i]){int64_t z=(int64_t)e->body.z+z_offsets[i];
                display=*e;display.body.z=z<INT32_MIN?INT32_MIN:z>INT32_MAX?INT32_MAX:(int32_t)z;view.entity=&display;}
            rc=project(p,&view,(view.flags&0x2000)!=0,screen_mirror,screen_x,screen_y);if(rc<0)goto done;
            if(view.flags&16){second=view;second.animation=(uint16_t)(second.animation+5u*(second.owner+1u));rc=project(p,&second,0,0,0,0);if(rc<0)goto done;}
        }
    }
done:p->updating=0;return rc;
}
int ssa_actor_collect_offsets(SsaActorProjector *p,const SsaWorld *world,const SsaActorSource *sources,size_t count,const int32_t *z_offsets)
{return collect_offsets(p,world,sources,count,z_offsets,0,0,0);}
int ssa_actor_collect_mirrored(SsaActorProjector *p,const SsaWorld *world,const SsaActorSource *sources,size_t count,const int32_t *z_offsets)
{return collect_offsets(p,world,sources,count,z_offsets,1,0,0);}
int ssa_actor_collect_screen(SsaActorProjector *p,const SsaWorld *world,const SsaActorSource *sources,size_t count,const int32_t *z_offsets,unsigned mirror,int x,int y)
{return collect_offsets(p,world,sources,count,z_offsets,mirror!=0,x,y);}
int ssa_actor_collect(SsaActorProjector *p,const SsaWorld *world,const SsaActorSource *sources,size_t count){
    return ssa_actor_collect_offsets(p,world,sources,count,NULL);
}
