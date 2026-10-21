#include "ssa/godrays_reference.h"
#include "ssa/actor_raster.h"
#include <string.h>

typedef struct ReferenceJob {
    SsaGodrayReference *effect;
    const SsaActorScene *scene;
    uint16_t walls[SSA_VISIBILITY_WALL_LIMIT];unsigned count,line;
    int16_t top[120],bottom[120];uint8_t valid[120];
    int error;
} ReferenceJob;
static unsigned word(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int clipbyte(const uint8_t *p,size_t size,int origin,unsigned x,int *value){
    if(!p || (int)x<origin || (size_t)((int)x-origin)>=size)return -1;
    *value=p[(int)x-origin]<128?p[(int)x-origin]:(int)p[(int)x-origin]-256;return 1;
}
static void reference_span(void *context,const SsaGodraySpan *span){
    ReferenceJob *j=context;const SsaActorQueue *q=&j->scene->queue;unsigned n;
    int64_t iq=span->q,iv=span->vq,iu=span->uq;uint8_t *fb=j->scene->geometry->framebuffer+SSA_SCENE_FRAME_PREFIX;
    if(j->line!=span->line){j->line=span->line;memset(j->valid,0,sizeof j->valid);}
    for(n=0;n<span->count;++n,iq+=span->dq,iv+=span->dvq,iu+=span->duq){unsigned x=span->x+n,k;uint32_t depth;size_t at=span->y*120+x;
        unsigned shade;
        (void)ssa_godray_sample(iq/65536,iv/65536,&depth);
        shade=ssa_godray_dither(iq/65536,iv/65536,iu/65536,span->bottom,span->top);
        int top=-1,bottom=120;int64_t scale;
        if(!shade || !depth || depth==UINT32_MAX)continue;
        if(j->effect->actor_depth[at]!=(uint16_t)65535 && depth/1024>=j->effect->actor_depth[at])continue;
        if(!j->valid[x]){
            scale=INT64_C(0x55b05a)*256/depth;
            for(k=0;k<j->count;++k){unsigned wi=j->walls[k];const SsaVisibleWall *w=&q->visibility->walls[wi];
                const SsaActorWallClip *clip=&q->wall_clips[wi];unsigned flags=clip->flags;int value;int64_t wall_scale;
                const SsaMapAssets *map=q->visibility->map;
                const uint8_t *segment=map->data+map->segments+(w->segment_flags&0x7fff)*16;
                if(word(segment+4)==span->line || x<w->left || x>w->right)continue;
                wall_scale=w->depth_left;
                if(w->right>w->left)wall_scale+=(int64_t)(w->depth_right-w->depth_left)*(x-w->left)/(w->right-w->left);
                if(scale>=wall_scale)continue;
                if(flags==3 && clip->bottom_height==32767 && clip->top_height==-32768){top=120;bottom=-1;break;}
                if(span->bottom>=clip->bottom_height)flags&=~1u;
                if(span->top<=clip->top_height)flags&=~2u;
                if(flags&1){if(clipbyte(clip->bottom,clip->bottom_size,clip->bottom_x,x,&value)<0){j->error=1;return;}
                    if(value!=-2 && value<bottom)bottom=value;}
                if(flags&2){if(clipbyte(clip->top,clip->top_size,clip->top_x,x,&value)<0){j->error=1;return;}
                    if(value!=-2 && value>top)top=value;}
            }
            j->top[x]=(int16_t)top;j->bottom[x]=(int16_t)bottom;j->valid[x]=1;
        }
        if((int)span->y>=j->top[x] && (int)span->y<j->bottom[x])fb[at]=j->effect->palette.blend[shade][fb[at]];
    }
}
int ssa_godrays_reference(SsaGodrayReference *e,const SsaActorScene *s,const SsaRenderCamera *c,const uint16_t colors[256]){
    ReferenceJob job={0};SsaGodrayCamera camera={0};SsaGodrayOps ops={reference_span,&job};unsigned i,index,steps=0;int result;
    if(!e || !s || !s->loaded || s->updating || !c || !s->geometry || s->queue.updating)return -1;
    if(!e->palette.loaded && ssa_godray_palette(&e->palette,colors)<0)return -1;
    memset(e->actor_depth,255,sizeof e->actor_depth);e->triangles=0;
    job.effect=e;job.scene=s;job.line=UINT32_MAX;
    index=s->queue.visibility->head;
    while(index!=65535){if(++steps>SSA_ACTOR_QUEUE_LIMIT+SSA_VISIBILITY_WALL_LIMIT)return -1;
        if(index&0x8000){i=index&0x7fff;if(i>=s->queue.actor_count)return -1;index=s->queue.actors[i].next;}
        else {if(index>=s->queue.visibility->wall_count || job.count>=SSA_VISIBILITY_WALL_LIMIT)return -1;
            job.walls[job.count++]=(uint16_t)index;index=s->queue.visibility->walls[index].next;}}
    for(i=s->queue.draw_count;i>0;--i){unsigned ai=s->queue.draw_order[i-1];const SsaActorRecord *a=&s->queue.actors[ai];uint16_t depth;
        if(a->depth<=0 || a->projection>=s->queue.projection_count || a->clip_offset>=SSA_ACTOR_CLIP_WORDS)return -1;
        {uint32_t d=(uint32_t)(INT64_C(0x55b05a)/a->depth/4);depth=(uint16_t)(d>65534?65534:d);}
        if(s->queue.fixed_screen_mask&(UINT32_C(1)<<ai))depth=0;
        if(ssa_actor_raster_depth(s->assets,&s->queue.projections[a->projection],s->queue.clip_words+a->clip_offset,
            SSA_ACTOR_CLIP_WORDS-a->clip_offset,e->actor_depth,14400,depth)<0)return -1;}
    camera.x=c->x/256;camera.y=c->y/256;camera.z=c->z/256;camera.sine=-c->sine;camera.cosine=c->cosine;
    camera.pitch_cosine=65536;camera.focal_x=0x55b05a/256;camera.focal_y=camera.focal_x*3/2;camera.width=camera.height=120;
    result=ssa_godrays_draw(s->geometry->visibility.map,&camera,&ops);
    if(result<0 || job.error)return -1;
    e->triangles=(unsigned)result;return 1;
}
