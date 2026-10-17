#include "ssa/actor_queue.h"
#include "ssa/actor_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned assertions,guards,frames,draws;
#define CHECK(x) do{++assertions;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define GUARD(x) do{++guards;CHECK(x);}while(0)
static void fresh(SsaActorQueue *q,SsaWorldVisibility *v)
{
    v->head=v->tail=0;v->wall_count=1;v->walls[0].previous=v->walls[0].next=65535;
    v->walls[0].left=0;v->walls[0].right=120;v->walls[0].depth_left=10000;v->walls[0].depth_right=30000;
    CHECK(ssa_actor_queue_begin(q,v)==1);q->wall_clips[0].flags=0;
}
static void actor(SsaActorQueue *q,const SsaActorAssets *a,unsigned resource,unsigned image,int left,int depth)
{
    const SsaActorImage *im=ssa_actor_assets_image(a,resource,image);SsaActorProjection *p;int index;
    CHECK(im && q->projection_count<32);p=q->projections+q->projection_count;
    memset(p,0,sizeof(*p));p->x=p->left=(int16_t)left;p->y=8;
    p->width=p->clipped_width=(int16_t)(im->width<120-left?im->width:120-left);
    p->height=(int16_t)(im->height*4096u/(a->reciprocal[6144]>>8));if(p->height>100)p->height=100;
    p->frame_flags=image;p->scale=4096;p->animation=(uint16_t)resource;p->top_z=64;
    index=ssa_actor_queue_allocate(q);CHECK(index>=0);
    q->actors[index].depth=depth;q->actors[index].projection=q->projection_count++;
    CHECK(ssa_actor_queue_insert(q,(unsigned)index)==1);
}
int main(void)
{
    FILE *f=fopen("orig/SSA.gba","rb");size_t size;uint8_t *rom,*fb=malloc(14400),*expected=malloc(14400);
    SsaActorAssets *a=calloc(1,sizeof(*a));SsaActorQueue *q=calloc(1,sizeof(*q));SsaWorldVisibility *v=calloc(1,sizeof(*v));
    SsaWallRenderTables *wall_tables=calloc(1,sizeof(*wall_tables));SsaWallRenderResult results[1]={{0}};
    SsaWallRenderer renderer={0};uint8_t openings[240]={0};
    CHECK(f && fb && expected && a && q && v && wall_tables);CHECK(!fseek(f,0,SEEK_END));size=(size_t)ftell(f);rewind(f);
    rom=malloc(size);CHECK(rom && fread(rom,1,size,f)==size);fclose(f);CHECK(ssa_actor_assets_load(a,rom,size)==1);
    wall_tables->loaded=1;renderer.tables=wall_tables;renderer.openings=openings;renderer.opening_capacity=sizeof(openings);
    for(unsigned level=0;level<16;++level) {
        SsaMapAssets map={0};CHECK(ssa_map_assets_load(&map,rom,size,level)==1);v->map=&map;
        for(unsigned frame=0;frame<64;++frame) {
            fresh(q,v);
            q->wall_clips[0].bottom_height=32;q->wall_clips[0].top_height=48;
            q->wall_clips[0].top=a->closed_top;q->wall_clips[0].top_size=121;q->wall_clips[0].top_x=0;
            q->wall_clips[0].bottom=a->closed_bottom;q->wall_clips[0].bottom_size=121;q->wall_clips[0].bottom_x=0;
            for(unsigned i=0;i<8;++i) {
                unsigned resource=4+(frame*8+i)%138;
                while(!a->resources[resource].count)resource=4+(resource-3)%138;
                actor(q,a,resource,1+(frame%a->resources[resource].count),(int)(i*9),(int)(5000+i*5000));
                if(i&1)q->projections[i].frame_flags|=0x80000000u;
            }
            memset(fb,0x31,14400);memcpy(expected,fb,14400);
            CHECK(ssa_actor_queue_order(q)==1 && q->draw_count==8);
            for(unsigned i=q->draw_count;i>0;--i) {
                SsaActorRecord *r=q->actors+q->draw_order[i-1];
                CHECK(ssa_actor_raster_draw(a,q->projections+r->projection,q->clip_words+r->clip_offset,
                    SSA_ACTOR_CLIP_WORDS-r->clip_offset,expected,14400)==1);
            }
            CHECK(ssa_actor_queue_draw(q,a,fb,14400)==1 && !memcmp(fb,expected,14400));++frames;draws+=8;
        }
        fresh(q,v);
        results[0].top_clip=wall_tables->closed_top;results[0].bottom_clip=wall_tables->closed_bottom;
        results[0].sprite_clip_flags=3;results[0].bottom_height=32;results[0].top_height=64;
        CHECK(ssa_actor_queue_bind_clips(q,results,1,&renderer,a)==1);
        CHECK(q->wall_clips[0].top==a->closed_top && q->wall_clips[0].top_size==121 &&
            q->wall_clips[0].bottom==a->closed_bottom && q->wall_clips[0].bottom_size==121);
        results[0].top_clip=openings+23;results[0].top_clip_x=19;results[0].bottom_clip=NULL;
        CHECK(ssa_actor_queue_bind_clips(q,results,1,&renderer,a)==1);
        CHECK(q->wall_clips[0].top==openings+23 && q->wall_clips[0].top_size==217 &&
            q->wall_clips[0].top_x==19 && !q->wall_clips[0].bottom_size);
        GUARD(ssa_actor_queue_bind_clips(q,results,0,&renderer,a)==-1);
        results[0].top_clip=(const uint8_t *)q;
        GUARD(ssa_actor_queue_bind_clips(q,results,1,&renderer,a)==-1);
        GUARD(ssa_actor_queue_bind_clips(q,NULL,1,&renderer,a)==-1);
        GUARD(ssa_actor_queue_bind_clips(q,results,1,NULL,a)==-1);
        GUARD(ssa_actor_queue_bind_clips(q,results,1,&renderer,NULL)==-1);
        fresh(q,v);
        q->updating=1;
        GUARD(ssa_actor_queue_begin(q,v)==-1);GUARD(ssa_actor_queue_allocate(q)==-1);
        GUARD(ssa_actor_queue_insert(q,0)==-1);GUARD(ssa_actor_queue_order(q)==-1);
        GUARD(ssa_actor_queue_draw(q,a,fb,14400)==-1);q->updating=0;
        v->updating=1;GUARD(ssa_actor_queue_allocate(q)==-1);v->updating=0;
        GUARD(ssa_actor_queue_begin(NULL,v)==-1);GUARD(ssa_actor_queue_begin(q,NULL)==-1);
        v->walls[0].next=0;GUARD(ssa_actor_queue_begin(q,v)==-1);v->walls[0].next=65535;
        v->tail=1;GUARD(ssa_actor_queue_begin(q,v)==-1);v->tail=0;
        GUARD(ssa_actor_queue_insert(q,0)==-1);
        CHECK(ssa_actor_queue_allocate(q)==0);GUARD(ssa_actor_queue_insert(q,0)==-1);
        q->projection_count=1;q->actors[0].projection=0;q->actors[0].depth=5000;
        q->projections[0].left=0;q->projections[0].clipped_width=1;
        q->clip_cursor=SSA_ACTOR_CLIP_WORDS;GUARD(ssa_actor_queue_insert(q,0)==-1);q->clip_cursor=0;
        q->projections[0].left=-1;GUARD(ssa_actor_queue_insert(q,0)==-1);q->projections[0].left=0;
        q->projections[0].clipped_width=121;GUARD(ssa_actor_queue_insert(q,0)==-1);q->projections[0].clipped_width=1;
        q->wall_clips[0].flags=1;q->wall_clips[0].bottom_height=32767;q->wall_clips[0].bottom=NULL;
        GUARD(ssa_actor_queue_insert(q,0)==-1 && !q->clip_cursor && !q->updating);
        q->wall_clips[0].flags=0;CHECK(ssa_actor_queue_insert(q,0)==1);
        GUARD(ssa_actor_queue_insert(q,0)==-1 && !q->updating);
        q->actors[0].next=0x8000;GUARD(ssa_actor_queue_order(q)==-1 && !q->updating);
        q->actors[0].next=0x801f;GUARD(ssa_actor_queue_order(q)==-1);q->actors[0].next=65535;
        q->actor_count=32;GUARD(ssa_actor_queue_allocate(q)==-1);q->actor_count=1;
        GUARD(ssa_actor_queue_draw(q,a,fb,14399)==-1);
        /* A later bad image preserves the preceding actor's genuine pixels. */
        fresh(q,v);actor(q,a,4,1,10,6000);actor(q,a,4,1,20,5000);
        CHECK(ssa_actor_queue_order(q)==1 && q->draw_count==2);
        {
            SsaActorRecord *good=q->actors+q->draw_order[1],*bad=q->actors+q->draw_order[0];
            memset(fb,0x55,14400);memcpy(expected,fb,14400);
            CHECK(ssa_actor_raster_draw(a,q->projections+good->projection,q->clip_words+good->clip_offset,
                SSA_ACTOR_CLIP_WORDS-good->clip_offset,expected,14400)==1);
            q->projections[bad->projection].animation=142;
            GUARD(ssa_actor_queue_draw(q,a,fb,14400)==-1 && !q->updating && !memcmp(fb,expected,14400));
        }
        ssa_map_assets_free(&map);
    }
    ssa_actor_assets_free(a);free(a);free(q);free(v);free(wall_tables);free(rom);free(fb);free(expected);
    printf("{\"assertions\":%u,\"guards\":%u,\"native_frames\":%u,\"native_actor_draws\":%u}\n",assertions,guards,frames,draws);
    return 0;
}
