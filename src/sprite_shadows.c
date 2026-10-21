#include "ssa/sprite_shadows.h"
#include "ssa/render_view.h"
#include <math.h>
#include <limits.h>
#include "sprite_shadow_texture.inc"
static int s16(const uint8_t *p){unsigned v=p[0]|(unsigned)p[1]<<8;return v<32768?(int)v:(int)v-65536;}
void ssa_sprite_shadow_palette(uint8_t colors[4][256],const uint16_t palette[256])
{
    unsigned level,i,j;
    for(level=0;level<4;++level)for(i=0;i<256;++i){unsigned best=i,error=UINT_MAX,gain=100-level*11;
        if(level)for(j=1;j<256;++j){int dr=(int)(palette[j]&31)*100-(int)(palette[i]&31)*(int)gain;
            int dg=(int)((palette[j]>>5)&31)*100-(int)((palette[i]>>5)&31)*(int)gain;
            int db=(int)((palette[j]>>10)&31)*100-(int)((palette[i]>>10)&31)*(int)gain;
            unsigned e=(unsigned)(dr*dr+dg*dg+db*db);if(e<error){best=j;error=e;}}
        colors[level][i]=(uint8_t)best;
    }
}
int ssa_sprite_shadows_prepare(SsaSpriteShadows *s,const SsaMapAssets *map,const SsaActorAssets *assets,
    const SsaActorSource *sources,unsigned count,const SsaEntity *player,const SsaSurfaceDecals *decals,const uint8_t *colors)
{
    unsigned i;if(!s || !map || !map->data || !assets || !colors || count>100 || (!sources && count))return -1;
    s->count=0;for(i=0;i<SSA_SHADOW_LIMIT;++i)s->indices[i]=-1;
    for(i=0;i<count;++i){const SsaActorSource *a=&sources[i];const SsaEntity *e=a->entity;
        const uint8_t *sector;double radius,air;unsigned x,y;SsaSpriteShadow *v;
        if(!e || (e!=player && (e->type<0xcd || (e->type>0xd8 && e->type!=0xe1))) ||
           (a->flags&0x2000) || !a->animation || a->animation>=SSA_ACTOR_RESOURCE_COUNT || a->frame>=65534)continue;
        if(e->sector<0 || (unsigned)e->sector>=map->sector_count)continue;
        sector=map->data+map->sectors+(size_t)e->sector*20;
        if(!sector[10] || ssa_surface_decals_water(decals,sector[10],s16(sector)))continue;
        radius=e->radius/65536.0*0.85;
        if(radius<1){const SsaActorResource *r=&assets->resources[a->animation];unsigned bit=0;
            const SsaActorImage *im=ssa_actor_assets_image(assets,a->animation,1);
            if(!im || !r->mip_flags)continue;
            while(bit<4 && !(r->mip_flags&(1u<<bit)))++bit;
            radius=im->width*(bit==0?4.0/3:bit==1?2:bit==2?8.0/3:bit==3?4:8)*0.3;}
        if(radius<6)radius=6;
        if(radius>96)radius=96;
        radius*=1.25; /* 25% larger than the original footprint, including pickups. */
        air=e->body.z/65536.0-s16(sector);if(air<0)air=0;if(air>=256)continue;
        v=&s->sprites[s->count];v->x=e->body.x;v->y=e->body.y;v->floor=(int16_t)s16(sector);v->sector=(int16_t)e->sector;
        v->radius=(unsigned)(radius*(1-air/512)+0.5);v->strength=(unsigned)(256-air);
        v->source=i;s->indices[i]=(int)s->count;
        for(x=0;x<SSA_SHADOW_WIDTH;++x)for(y=0;y<SSA_SHADOW_HEIGHT;++y){
            unsigned shade=(shadow_texture[y*4+2][x]*v->strength+128)/256;
            v->pixels[x*SSA_SHADOW_HEIGHT+y]=shade?colors[shade*256+sector[10]]:0;}
        ++s->count;
    }
    return 1;
}

typedef struct ShadowLayer {
    SsaSpriteShadows *shadows;const SsaRenderCamera *camera;
    const SsaActorSource *sources;const int32_t *offsets;uint8_t drawn[SSA_SHADOW_LIMIT];
} ShadowLayer;
static int draw_underlay(void *context,const SsaActorQueue *q,unsigned actor,uint8_t *fb,size_t capacity)
{
    ShadowLayer *l=context;const SsaActorRecord *record=&q->actors[actor];
    const SsaActorProjection *p=&q->projections[record->projection];const SsaSpriteShadow *v=NULL;
    int16_t clips[240];double scale,cx,cy,w,h,left,top;int x,y,x0,x1,y0,y1;unsigned i;
    if(capacity<14400)return -1;
    if(q->fixed_screen_mask&(UINT32_C(1)<<actor))return 1;
    /* Match only a base projection, never its secondary player-color layer. */
    for(i=0;i<l->shadows->count;++i){const SsaSpriteShadow *candidate=&l->shadows->sprites[i];
        const SsaActorSource *a=&l->sources[candidate->source];int64_t z=a->entity->body.z;
        if(l->offsets)z+=l->offsets[candidate->source];
        if(z<INT32_MIN)z=INT32_MIN;
        if(z>INT32_MAX)z=INT32_MAX;
        if(!l->drawn[i] && a->animation==p->animation && (int)floor(candidate->x/65536.0)==p->world_x &&
           (int)floor(candidate->y/65536.0)==p->world_y && (int)floor(z/65536.0)==p->bottom_z){
            v=candidate;l->drawn[i]=1;break;}}
    if(!v || record->depth<=0)return 1;
    scale=record->depth/65536.0;
    cx=60+((v->x/65536.0-l->camera->x/65536.0)*l->camera->cosine/65536.0+
           (v->y/65536.0-l->camera->y/65536.0)*l->camera->sine/65536.0)*scale;
    cy=60-(v->floor-l->camera->z/65536.0)*scale*1.5;
    w=v->radius*2*scale;h=v->radius*.5*scale*1.5;left=cx-w*.5;top=cy-h*.5;
    if(left>=120 || left+w<=0 || top>=120 || top+h<=0)return 1;
    x0=(int)fmax(0,ceil(left-.5));x1=(int)fmin(119,ceil(left+w-.5)-1);
    y0=(int)fmax(0,ceil(top-.5));y1=(int)fmin(119,ceil(top+h-.5)-1);
    if(x1<x0 || y1<y0)return 1;
    if(ssa_actor_queue_sprite_clips(q,record->depth,v->x,v->y,v->floor,v->floor,x0,x1,clips)<0)return -1;
    for(x=x0;x<=x1;++x)for(y=y0;y<=y1;++y){unsigned u,t;uint8_t pixel;
        if(y<clips[x*2] || y>=clips[x*2+1])continue;
        u=(unsigned)((x+.5-left)*SSA_SHADOW_WIDTH/w);t=(unsigned)((y+.5-top)*SSA_SHADOW_HEIGHT/h);
        if(u>=SSA_SHADOW_WIDTH || t>=SSA_SHADOW_HEIGHT)continue;
        pixel=v->pixels[u*SSA_SHADOW_HEIGHT+t];if(pixel)fb[y*120+x]=pixel;}
    return 1;
}
int ssa_sprite_shadows_reference(SsaSpriteShadows *s,SsaActorScene *scene,const SsaRenderCamera *camera,
    uint32_t flags,const SsaActorSource *sources,unsigned count,const int32_t *offsets)
{
    ShadowLayer layer={0};if(!s || !camera || count>SSA_SHADOW_LIMIT)return -1;
    layer.shadows=s;layer.camera=camera;layer.sources=sources;layer.offsets=offsets;
    return ssa_actor_scene_draw_layers(scene,camera,flags,sources,count,offsets,draw_underlay,&layer);
}
