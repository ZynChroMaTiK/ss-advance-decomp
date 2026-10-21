#include "ssa/pc_scene.h"
#include "ssa/native_game.h"
#include "ssa/settings.h"
#include "ssa/godrays.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

typedef struct SsaPcRayHit {double distance,u;unsigned line;} SsaPcRayHit;
typedef struct SsaPcRayLine {double ax,ay,vx,vy,numerator;} SsaPcRayLine;
typedef struct Camera {double x,y,z,sine,cosine,focal,zoom,ps,pc,sky_shift,clip_z;unsigned clip,top,bottom;uint32_t palette[256];SsaSurfaceSampleCache *samples;const SsaSpriteShadows *shadows;const SsaSurfaceDecals *decals;SsaWaterRender *water;} Camera;
double ssa_pc_scene_fov_scale(unsigned offset)
{
    /*70 is the rounded original horizontal angle at3:2. Keep its exact
     * projection; other choices are calibrated relative to that same view. */
    const double radians=3.14159265358979323846/360.0;
    if(offset==SSA_FOV_DEFAULT_OFFSET || offset>SSA_FOV_RANGE)return 1.0;
    return tan(70.0*radians)/tan((SSA_FOV_MIN+offset)*radians);
}
static void geometry_free(SsaPcRenderer *);
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int s16(const uint8_t *p){unsigned n=u16(p);return n<32768?(int)n:(int)n-65536;}
static uint32_t rgb(unsigned n){unsigned r=n&31,g=(n>>5)&31,b=(n>>10)&31;
 return (((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));}
static int edge(double y,unsigned height){int n;if(y<=0.5)return 0;if(y>=(double)height+0.5)return (int)height;
 y-=0.5;n=(int)y;return n+(y>n);}
static unsigned wrap(double n,unsigned size){int64_t whole,remainder;
 if(n<=-9223372036854775808.0 || n>=9223372036854775807.0)return 0;
 whole=(int64_t)n;if(n<whole)--whole;
 /* Texture dimensions are commonly powers of two. Unsigned conversion keeps
  * negative coordinates identical to the signed modulo/wrap fallback. */
 if(!(size&(size-1)))return (unsigned)((uint64_t)whole&(size-1));
 remainder=whole%(int64_t)size;if(remainder<0)remainder+=size;return (unsigned)remainder;}
static double length(double x,double y){
#if defined(__x86_64__) || defined(_M_X64)
 return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(),_mm_set_sd(x*x+y*y)));
#else
 double n=x*x+y*y,r=(x<0?-x:x)+(y<0?-y:y);unsigned i;
 if(!r)return 0;
 for(i=0;i<8;++i)r=(r+n/r)/2;
 return r;
#endif
}
int ssa_pc_renderer_resize(SsaPcRenderer *r,unsigned w,unsigned h)
{
    size_t n;uint32_t *pixels;float *depth;
    if(!r || r->updating || !w || !h || w>INT_MAX || h>INT_MAX ||
       (size_t)w>SIZE_MAX/h || (size_t)w*h>SIZE_MAX/(sizeof(*pixels)+sizeof(*depth)))return -1;
    if(r->pixels && r->depth && w==r->width && h==r->height)return 1;
    n=(size_t)w*h;pixels=malloc(n*sizeof(*pixels));if(!pixels)return -1;
    depth=malloc(n*sizeof(*depth));if(!depth){free(pixels);return -1;}
    free(r->pixels);free(r->depth);r->pixels=pixels;r->depth=depth;r->width=w;r->height=h;
    return 1;
}
void ssa_pc_renderer_free(SsaPcRenderer *r)
{if(!r || r->updating)return;if(r->reflection){ssa_pc_renderer_free(r->reflection);free(r->reflection);}
 ssa_water_free(&r->water);geometry_free(r);free(r->pixels);free(r->depth);free(r->hits);free(r->ray_lines);memset(r,0,sizeof(*r));}
static int compare(const void *a,const void *b)
{const SsaPcRayHit *x=a,*y=b;return x->distance<y->distance?-1:x->distance>y->distance?1:x->line<y->line?-1:x->line>y->line;}
static int prepare_ray_lines(SsaPcRenderer *r,const SsaMapAssets *m,const Camera *c)
{
    unsigned i;
    if(r->ray_line_capacity<m->line_count){SsaPcRayLine *p=malloc((size_t)m->line_count*sizeof(*p));
        if(!p)return -1;
        free(r->ray_lines);r->ray_lines=p;r->ray_line_capacity=m->line_count;}
    /* Refresh once per draw, including map edits/reloads at the same address. */
    for(i=0;i<m->line_count;++i){
        const uint8_t *l=m->data+m->lines+(size_t)i*40;
        const uint8_t *a=m->data+m->vertices+(size_t)u16(l)*4,*b=m->data+m->vertices+(size_t)u16(l+2)*4;
        double ax=s16(a),ay=s16(a+2),vx=s16(b)-ax,vy=s16(b+2)-ay;
        r->ray_lines[i]=(SsaPcRayLine){ax-c->x,ay-c->y,vx,vy,(ax-c->x)*vy-(ay-c->y)*vx};
    }
    return 1;
}
static size_t intersections(SsaPcRenderer *r,const SsaMapAssets *m,double dx,double dy)
{
    size_t count=0;unsigned i;
    for(i=0;i<m->line_count;++i){const SsaPcRayLine *l=&r->ray_lines[i];
        double den=dx*l->vy-dy*l->vx,t,u;
        if(den>-1e-12 && den<1e-12)continue;
        t=l->numerator/den;
        u=(l->ax*dy-l->ay*dx)/den;
        if(t<=0.001 || u<0 || u>=1)continue;
        r->hits[count++]=(SsaPcRayHit){t,u,i};
    }
    qsort(r->hits,count,sizeof(*r->hits),compare);return count;
}
static unsigned sky_column(const SsaNativeGame *g,const SsaPcRenderer *r,unsigned x)
{
    /* Backdrop artwork keeps the original70-degree framing at every world FOV. */
    double col=(double)g->pc_camera.heading*640.0/65536.0+60.0+
        ((double)x+0.5-(double)r->width/2)*80.0/r->height;
    return wrap(col,640);
}
static unsigned sky_row(const SsaPcRenderer *r,unsigned y,double shift)
{
    /* Translate the flat artwork with the projected horizon, keeping its size.
     * At level aim it covers the upper three quarters (rounded up). Clamp both
     * edges so newly exposed space repeats that column's edge pixel. */
    unsigned top=r->height-r->height/4;double row=(double)y-shift;
    if(row<0)row=0;
    if(row>top-1)row=top-1;
    return (unsigned)(row*60/top);
}
static void plane(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,unsigned x,
    int begin,int end,int height,unsigned color)
{
    int y;double middle=(double)r->height/2;unsigned sx=sky_column(g,r,x);
    const uint8_t *sky=g->sky.pixels[ssa_texture_assets_family(g->level)];
    for(y=begin;y<end;++y){size_t at=(size_t)y*r->width+x;
        if(!color){unsigned sy=sky_row(r,(unsigned)y,c->sky_shift);
            r->pixels[at]=c->palette[sky[sy*768+sx]];r->depth[at]=FLT_MAX;}
        else {double d=(c->z-height)*c->focal/((double)y+0.5-middle);unsigned pixel=color;
            if(c->decals || c->water){double lateral=(x+0.5-r->width/2.0)/c->focal;
                double wx=c->x+(c->sine+c->cosine*lateral)*d,wy=c->y+(c->cosine-c->sine*lateral)*d;
                if(c->water && height<c->z && ssa_surface_decals_water(c->water->decals,color,height)){
                    r->pixels[at]=rgb(ssa_water_rgb_cached(c->water,c->samples,(uint8_t)color,wx,wy,height,d,at));
                    r->depth[at]=d>0 && d<FLT_MAX?(float)d:FLT_MAX;continue;}
                if(c->decals && !(height<c->z && ssa_surface_decals_water(c->decals,color,height)))
                pixel=ssa_surface_decals_sample_cached(c->decals,c->samples,height>c->z,(uint8_t)color,
                    wx,wy,height,d);
                }
            r->pixels[at]=c->palette[pixel];r->depth[at]=d>0 && d<FLT_MAX?(float)d:FLT_MAX;}
    }
}
static const SsaTextureImage *texture(const SsaNativeGame *g,unsigned side,unsigned layer)
{
    unsigned index;if(side>=g->map.side_count)return NULL;
    index=u16(g->map.data+g->map.sides+(size_t)side*12+4+layer*2);
    return index==65535?NULL:ssa_map_texture_image(&g->bindings,index,0);
}
static unsigned mip_level(double footprint){return footprint>=4?2:footprint>=2?1:0;}
static const SsaTextureImage *texture_level(const SsaNativeGame *g,unsigned side,unsigned layer,unsigned level){
    unsigned index=u16(g->map.data+g->map.sides+(size_t)side*12+4+layer*2);
    return index==65535?NULL:ssa_map_texture_image(&g->bindings,index,level);
}
static double actor_unit(unsigned bit){return bit==0?4.0/3:bit==1?2:bit==2?8.0/3:bit==3?4:8;}
static unsigned actor_mip(const SsaNativeGame *g,const SsaActorResource *resource,double projected,unsigned *image){
    unsigned bit=0,next,ordinal=0,selected=0;
    while(bit<5 && !(resource->mip_flags&(1u<<bit)))++bit;
    if(g->mipmaps || g->gba_view)for(next=bit+1;next<5;++next)if(resource->mip_flags&(1u<<next)){
        ++ordinal;if(projected*actor_unit(next)>1.0)break;bit=next;selected=ordinal;}
    *image+=selected*resource->frames*resource->directions;return bit;
}
static void wall(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,unsigned x,int top,int bottom,
    double distance,double along,double origin,unsigned side,unsigned layer,double footprint)
{
    const SsaTextureImage *base=texture(g,side,layer),*im=base;int y;unsigned sx;double us=1,vs=1;
    if(base && (g->mipmaps || g->gba_view)){im=texture_level(g,side,layer,mip_level(footprint));
        if(!im)im=base;
        us=(double)im->width/base->width;vs=(double)im->height/base->height;}
    if(!im || !im->pixels || !im->width || !im->height){
        /* A missing surface image must not reveal actors behind its geometry. */
        for(y=top;y<bottom;++y)r->depth[(size_t)y*r->width+x]=(float)distance;
        return;
    }
    sx=wrap(along/2*us,im->width);
    for(y=top;y<bottom;++y){double z=c->z+((double)r->height/2-y-0.5)*distance/c->focal;
        unsigned sy=wrap((origin-z)/2*vs,im->height);size_t at=(size_t)y*r->width+x;
        r->pixels[at]=c->palette[im->pixels[(size_t)sx*im->height+sy]];r->depth[at]=(float)distance;++r->wall_pixels;
    }
}
static void column(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,unsigned x)
{
    const SsaMapAssets *m=&g->map;double lateral=((double)x+0.5-(double)r->width/2)/c->focal;
    double dx=c->sine+c->cosine*lateral,dy=c->cosine-c->sine*lateral;
    int sector=g->pc_camera.sector,top=0,bottom=(int)r->height;size_t i,n=intersections(r,m,dx,dy);
    ++r->rays;
    for(i=0;i<n && top<bottom;++i){
        const SsaPcRayHit *hit=&r->hits[i];const uint8_t *l=m->data+m->lines+(size_t)hit->line*40,*s,*b,*side;
        unsigned direction,side_index,flags=u16(l+26);int other,ceil_y,floor_y,upper,lower;double d=hit->distance,along,len,origin,footprint;
        int fc,ff,bc,bf;
        if((unsigned)sector==u16(l+12))direction=0;
        else if((flags&4) && (unsigned)sector==u16(l+14))direction=1;
        else continue;
        s=m->data+m->sectors+(size_t)sector*20;fc=s16(s+2);ff=s16(s);
        ceil_y=edge((double)r->height/2-(fc-c->z)*c->focal/d,r->height);
        floor_y=edge((double)r->height/2-(ff-c->z)*c->focal/d,r->height);
        if(ceil_y>top){int end=ceil_y<bottom?ceil_y:bottom;plane(r,g,c,x,top,end,fc,s[11]);top=end;}
        if(floor_y<bottom){int start=floor_y>top?floor_y:top;plane(r,g,c,x,start,bottom,ff,s[10]);bottom=start;}
        if(top>=bottom)break;
        side_index=u16(l+8+direction*2);if(side_index>=m->side_count)break;
        side=m->data+m->sides+(size_t)side_index*12;
        {const uint8_t *a=m->data+m->vertices+(size_t)u16(l)*4,*v=m->data+m->vertices+(size_t)u16(l+2)*4;
         double vx=s16(v)-s16(a),vy=s16(v+2)-s16(a+2);len=length(vx,vy);footprint=d/(2*c->focal)*fmax(1,len/fmax(1e-12,fabs(dx*vy-dy*vx)));}
        along=(direction?1-hit->u:hit->u)*len+s16(side);
        if(!(flags&4)){
            const SsaTextureImage *im=texture(g,side_index,SSA_MAP_SIDE_MIDDLE);
            origin=(flags&16) && im?ff+im->height*2.0:fc;
            wall(r,g,c,x,top,bottom,d,along,origin+s16(side+2),side_index,SSA_MAP_SIDE_MIDDLE,footprint);return;
        }
        other=(int)u16(l+12+(direction^1u)*2);if((unsigned)other>=m->sector_count)break;
        b=m->data+m->sectors+(size_t)other*20;bc=s16(b+2);bf=s16(b);
        upper=edge((double)r->height/2-(bc-c->z)*c->focal/d,r->height);
        lower=edge((double)r->height/2-(bf-c->z)*c->focal/d,r->height);
        if(bc<fc){const SsaTextureImage *im=texture(g,side_index,SSA_MAP_SIDE_UPPER);
            origin=(flags&8) || !u16(l+36)?fc:bc+(im?im->height*2.0:0);
            wall(r,g,c,x,top,upper<bottom?upper:bottom,d,along,origin+s16(side+2),side_index,SSA_MAP_SIDE_UPPER,footprint);}
        if(bf>ff)wall(r,g,c,x,lower>top?lower:top,bottom,d,along,bf+s16(side+2),side_index,SSA_MAP_SIDE_LOWER,footprint);
        if(upper>top)top=upper;
        if(lower<bottom)bottom=lower;
        sector=other;
    }
    if(top<bottom){const uint8_t *s=m->data+m->sectors+(size_t)sector*20;int middle=(int)(r->height/2);
        if(top<middle)plane(r,g,c,x,top,bottom<middle?bottom:middle,s16(s+2),s[11]);
        if(bottom>middle)plane(r,g,c,x,top>middle?top:middle,bottom,s16(s),s[10]);}
}
/* Fixed-screen weapon effects are composed after all world billboards. */
static void screen_actor(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,const SsaActorSource *s)
{
    const SsaActorImage *im;double scale=(double)r->height/160,x=r->width/2.0,y=r->height/2.0,w,h;
    double hud_scale=r->width/240.0<scale?r->width/240.0:scale;
    unsigned flip=g->alternates.enabled && (s->flags&SSA_ACTOR_SCREEN_MIRROR)!=0;
    int left,right,top,bottom,xx,yy;
    if(!s->entity || !s->animation || s->animation>=SSA_ACTOR_RESOURCE_COUNT || s->frame>=65534 ||
       (s->flags&128) || ((s->flags&64) && (g->frame_game.flags&1)))return;
    im=ssa_actor_assets_image(&g->actor_assets,s->animation,s->frame+1u);if(!im)return;
    x+=g->weapon_bob_x/256.0*hud_scale;y+=g->weapon_bob_y/256.0*hud_scale;
    x+=(flip?-(im->origin_x+(int)im->width):im->origin_x)*scale;y+=im->origin_y*scale;
    w=im->width*scale;h=im->height*scale;
    left=edge(x,r->width);right=edge(x+w,r->width);top=edge(y,r->height);bottom=edge(y+h,r->height);
    for(yy=top;yy<bottom;++yy){unsigned sy=(unsigned)(((double)yy+0.5-y)/scale);if(sy>=im->height)continue;
        for(xx=left;xx<right;++xx){unsigned sx=(unsigned)(((double)xx+0.5-x)/scale);uint8_t index;size_t at=(size_t)yy*r->width+(unsigned)xx;
            if(sx>=im->width)continue;
            if(flip)sx=im->width-1-sx;
            index=im->pixels[(size_t)sx*im->height+sy];
            if(index){r->pixels[at]=c->palette[index];r->depth[at]=0.0001f;++r->actor_pixels;}
        }
    }
}

typedef struct ActorOrder {double depth;unsigned index;} ActorOrder;
static int actor_compare(const void *left,const void *right)
{
    const ActorOrder *a=left,*b=right;
    return a->depth>b->depth?-1:a->depth<b->depth?1:a->index<b->index?-1:a->index>b->index;
}
static void actor_order(const SsaNativeGame *g,const Camera *c,unsigned order[100])
{
    ActorOrder actors[100];unsigned n=g->spawn.pool.active_count,i,first,last,j;
    for(i=0;i<n;++i)order[i]=i;
    for(i=0;i<n;++i){const SsaEntity *e=g->sources[i].entity;
        actors[i]=(ActorOrder){e?(e->body.x/65536.0-c->x)*c->sine+
            (e->body.y/65536.0-c->y)*c->cosine:-DBL_MAX,i};}
    /* GBA paints far-to-near, with later submissions winning depth ties.
     * Software uses the same later-wins rule within0.001 units. Billboards
     * are parallel in world XY: sort before pitch, not by their height. */
    qsort(actors,n,sizeof(*actors),actor_compare);
    for(first=0;first<n;first=last){
        for(last=first+1;last<n && actors[first].depth-actors[last].depth<=0.001;++last){}
        for(i=first+1;i<last;++i){ActorOrder a=actors[i];j=i;
            while(j>first && actors[j-1].index>a.index){actors[j]=actors[j-1];--j;}actors[j]=a;}
    }
    for(i=0;i<n;++i)order[i]=actors[i].index;
}
#include "pc_renderer_pitch.inc"
#include "pc_godrays.inc"
/* Shared world-sprite submission, including attached shadows and body layers.
 * Only reflection exposes the otherwise hidden local first-person body. */
static void draw_actors(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,const unsigned order[100],int reflection)
{
    unsigned draw;
    for(draw=0;draw<g->spawn.pool.active_count;++draw){unsigned i=order[draw];SsaActorSource s=g->sources[i];SsaEntity display;
        if(s.flags&0x2000)continue;
        if(reflection && s.entity==&g->players[0].entity)s.flags&=UINT16_C(0xff7f);
        if(g->source_z_offsets[i] && s.entity){int64_t z=(int64_t)s.entity->body.z+g->source_z_offsets[i];
            display=*s.entity;display.body.z=z<INT32_MIN?INT32_MIN:z>INT32_MAX?INT32_MAX:(int32_t)z;s.entity=&display;}
        pitched_shadow(r,g,c,&s,i);pitched_actor(r,g,c,&s);
        if(s.flags&16){s.animation=(uint16_t)(s.animation+5*(s.owner+1));pitched_actor(r,g,c,&s);}
    }
}
/* GPU rasterization returns visible plane coordinates for the existing exact
 * palette/decal/water rules. HUD and gameplay remain outside the backend. */
typedef struct HardwareShade {
    SsaPcRenderer *r;const Camera *c;const SsaPcSample *samples;HardwareRowStats *stats;
} HardwareShade;
static void hardware_shade(void *context,unsigned first,unsigned end)
{
    const HardwareShade *job=context;SsaPcRenderer *r=job->r;const Camera *c=job->c;
    const SsaPcSample *samples=job->samples;SsaSurfaceSampleCache texels={0};unsigned y;
    for(y=first;y<end;++y){size_t at=(size_t)y*r->width,row_end=at+r->width;unsigned actors=0,walls=0;
      double height_scale=(r->height/2.0-y-0.5)/c->focal*c->pc+c->ps;
      for(;at<row_end;++at){const SsaPcSample *s=&samples[at];unsigned code=(unsigned)s->code,color=code&255,decals=(code>>8)&3;
        r->depth[at]=s->z;
        actors+=(code&2048)!=0;walls+=(code&6144)==4096;
        /* ssa_water_begin already cleared the complete mask, including actors. */
        if(decals && (c->water || c->decals)){double z=c->z+height_scale*s->z;
            int height=(int)floor(z+0.5);
            if(decals==1 && c->water && ssa_surface_decals_water(c->water->decals,color,height))
                r->pixels[at]=rgb(ssa_water_rgb_cached(c->water,&texels,(uint8_t)color,s->u,s->v,height,s->z,at));
            else {unsigned shaded=color;
                if(c->decals && !(decals==1 && ssa_surface_decals_water(c->decals,color,height)))
                    shaded=ssa_surface_decals_sample_cached(c->decals,&texels,decals-1,(uint8_t)color,s->u,s->v,height,s->z);
                r->pixels[at]=c->palette[shaded];}
        }
      }
      job->stats[y]=(HardwareRowStats){actors,walls};
    }
}
static int hardware_finish(SsaPcRenderer *r,const Camera *c)
{
    const SsaPcSample *samples;SsaPcGeometry *cache=r->geometry;unsigned y;
    if(cache->row_capacity<r->height){HardwareRowStats *stats;
        if(SIZE_MAX/(size_t)r->height<sizeof(*stats))return -1;
        stats=malloc((size_t)r->height*sizeof(*stats));if(!stats)return -1;
        free(cache->row_stats);cache->row_stats=stats;cache->row_capacity=r->height;
    }
    if(r->hardware_error || r->hardware->end(r->hardware->context,r,&samples)<0)return -1;
    /* One read of each sample supplies depth, statistics and effects. Each
     * job owns its rows and counters; reduce only after synchronous dispatch. */
    {HardwareShade job={r,c,samples,cache->row_stats};
        if(r->hardware->rows)r->hardware->rows(r->hardware->context,c->top,c->bottom,hardware_shade,&job);
        else hardware_shade(&job,c->top,c->bottom);
    }
    for(y=c->top;y<c->bottom;++y){r->actor_pixels+=cache->row_stats[y].actors;r->wall_pixels+=cache->row_stats[y].walls;}
    return 1;
}
/* Center-aligned bilinear reconstruction of the reflection buffer.
 * Clamp at the image edges, including a1-pixel target or odd viewport size. */
static unsigned reflection_axis(unsigned pixel,unsigned output,unsigned size,unsigned *weight)
{
    int64_t at=(int64_t)(((uint64_t)pixel*2+1)*size*128/output)-128;
    if(at<=0){*weight=0;return 0;}
    if((uint64_t)at>=(uint64_t)(size-1)*256){*weight=0;return size-1;}
    *weight=(unsigned)at&255;return (unsigned)(at/256);
}
static uint32_t reflection_sample(const SsaPcRenderer *r,unsigned x,unsigned y,unsigned fx,unsigned fy)
{
    unsigned x1=x+(fx!=0),y1=y+(fy!=0),channel;uint32_t out=0;
    const uint32_t *a=r->pixels+(size_t)y*r->width,*b=r->pixels+(size_t)y1*r->width;
    for(channel=0;channel<24;channel+=8){
        unsigned top=((a[x]>>channel)&255)*(256-fx)+((a[x1]>>channel)&255)*fx;
        unsigned bottom=((b[x]>>channel)&255)*(256-fx)+((b[x1]>>channel)&255)*fx;
        out|=((top*(256-fy)+bottom*fy+32768)>>16)<<channel;
    }
    return out;
}
typedef struct ReflectionBlend {
    const SsaPcRenderer *r;SsaWaterRender *water;unsigned key;
    uint32_t *output;uint8_t *indexed;
} ReflectionBlend;
static void reflection_blend_rows(void *context,unsigned first,unsigned end)
{
    const ReflectionBlend *job=context;const SsaPcRenderer *r=job->r;SsaWaterRender *water=job->water;
    unsigned y,width=r->width,height=r->height;
    for(y=first;y<end;++y){unsigned x,fy;
        unsigned sy=reflection_axis(water->height-1-y,water->height,height,&fy);
        size_t p=(size_t)y*water->width;
        for(x=0;x<water->width;++x,++p)if((water->mask[p]>>24) && (water->mask[p]&65535)==job->key){
            uint32_t color;uint16_t reflected,blend;
            /* Native pixels already align one-to-one; no resampling needed. */
            if(width==water->width && height==water->height)
                color=r->pixels[(size_t)(height-1-y)*width+x];
            else {unsigned fx,sx=reflection_axis(x,water->width,width,&fx);
                color=reflection_sample(r,sx,sy,fx,fy);}
            reflected=(uint16_t)(((color>>19)&31)|(((color>>11)&31)<<5)|(((color>>3)&31)<<10));
            blend=ssa_water_blend(water,water->mask[p],reflected);
            if(job->output)job->output[p]=rgb(blend);
            if(job->indexed)job->indexed[p]=ssa_water_reflection_index(water,blend,x,y);
        }
    }
}
static int reflect_prepared(SsaPcRenderer **owner,const SsaNativeGame *g,const SsaRenderCamera *view,
    int pitch,SsaWaterRender *water,uint32_t *output,uint8_t *indexed,const SsaSpriteShadows *prepared)
{
    size_t at,count;unsigned width,height,span_count=0,span_index,order[100];SsaPcRenderer *r;
    int buckets[256];SsaSpriteShadows shadows;
    if(!owner || !g || !view || !water || (!output && !indexed) || g->spawn.pool.active_count>100)return -1;
    if(!water->reflections)return 1;
    count=(size_t)water->width*water->height;
    if(!water->mask || count>water->capacity)return -1;
    for(at=0;at<count && !(water->mask[at]>>24);++at){}
    if(at==count)return 1; /* No visible reflective water: no second pass. */
    /* Native reflections match the viewport; GBA retains its240x160 view. */
    width=indexed?240:water->width;height=indexed?160:water->height;
    if(!*owner){*owner=calloc(1,sizeof(**owner));if(!*owner)return -1;}
    r=*owner;if(r->updating || ssa_pc_renderer_resize(r,width,height)<0)return -1;
    if(pitched_geometry(r,g)<0)return -1;
    for(span_index=0;span_index<256;++span_index)buckets[span_index]=-1;
    /* One stencil scan collects all height intervals in first-appearance order.
     * Scratch survives frames; heights and row extents are always refreshed. */
    for(;at<count;++at){unsigned key=water->mask[at]&65535,bucket;int index;ReflectionSpan *span;
        if(!(water->mask[at]>>24))continue;
        bucket=((key*UINT32_C(0x9e3779b9))>>24)&255;index=buckets[bucket];
        while(index>=0 && r->geometry->reflection_spans[index].key!=key)index=r->geometry->reflection_spans[index].next;
        if(index<0){SsaPcGeometry *cache=r->geometry;
            if(span_count==cache->reflection_capacity){size_t cap=cache->reflection_capacity?cache->reflection_capacity*2:16;
                ReflectionSpan *spans=realloc(cache->reflection_spans,cap*sizeof(*spans));if(!spans)return -1;
                cache->reflection_spans=spans;cache->reflection_capacity=cap;}
            index=(int)span_count++;span=&cache->reflection_spans[index];
            *span=(ReflectionSpan){key,(unsigned)(at/water->width),(unsigned)(at/water->width),buckets[bucket]};buckets[bucket]=index;
        }else r->geometry->reflection_spans[index].last=(unsigned)(at/water->width);
    }
    if(g->sprite_shadows_enabled && !prepared){
        if(ssa_sprite_shadows_prepare(&shadows,&g->map,&g->actor_assets,g->sources,g->spawn.pool.active_count,
            &g->players[0].entity,&g->surface_decals,g->shadow_colors[0])<0)return -1;
        prepared=&shadows;}
    for(span_index=0;span_index<span_count;++span_index){const ReflectionSpan *span=&r->geometry->reflection_spans[span_index];
        unsigned key=span->key,i,first=span->first,last=span->last;size_t p;int h=key<32768?(int)key:(int)key-65536;
        int32_t sine,cosine;SsaSurfaceSampleCache texels={0};Camera c={0};
        {unsigned weight;
            c.top=reflection_axis(water->height-1-last,water->height,height,&weight);
            c.bottom=reflection_axis(water->height-1-first,water->height,height,&weight)+1;
            if(weight)++c.bottom;
        }
        c.samples=&texels;c.x=view->x/65536.0;c.y=view->y/65536.0;c.z=2.0*h-view->z/65536.0;
        c.shadows=prepared;
        c.clip=1;c.clip_z=h;c.decals=g->surface_decals_enabled?&g->surface_decals:NULL;
        ssa_sincos(&g->math,view->heading,&sine,&cosine);c.sine=sine/65536.0;c.cosine=cosine/65536.0;
        ssa_sincos(&g->math,(uint16_t)-pitch,&sine,&cosine);c.ps=sine/65536.0;c.pc=cosine/65536.0;
        c.zoom=g->gba_view?1.0:ssa_pc_scene_fov_scale(g->fov_offset);
        c.focal=height*(0x55b05a/65536.0)/80.0;
        c.sky_shift=c.pc>0?c.focal*c.ps/c.pc:(c.ps>=0?(double)height:-(double)height);
        c.focal*=c.zoom;
        for(i=0;i<256;++i)c.palette[i]=rgb(g->palette_effect.colors[i]);
        r->updating=1;r->rays=r->wall_pixels=r->actor_pixels=r->godray_triangles=0;
        if(!r->hardware)for(p=(size_t)c.top*width;p<(size_t)c.bottom*width;++p)r->depth[p]=FLT_MAX;
        if(pitched_world(r,g,&c)<0){r->updating=0;return -1;}
        if(actor_clip_prepare(r,g,&c)<0){r->updating=0;return -1;}
        if(!span_index)actor_order(g,&c,order); /* Depth ordering depends on XY/yaw only. */
        draw_actors(r,g,&c,order,1);
        if(r->hardware && hardware_finish(r,&c)<0){r->updating=0;return -1;}
        if(g->godrays_enabled && godrays_draw(r,g,&c)<0){r->updating=0;return -1;}
        r->updating=0;++water->passes;water->actor_pixels+=r->actor_pixels;
        {ReflectionBlend job={r,water,key,output,indexed};
            /* Indexed GBA output updates a shared palette cache, so retains
             * serial composition. RGB jobs write disjoint output rows only. */
            if(r->hardware && r->hardware->rows && !indexed)
                r->hardware->rows(r->hardware->context,first,last+1,reflection_blend_rows,&job);
            else reflection_blend_rows(&job,first,last+1);
        }
    }
    return 1;
}
int ssa_pc_renderer_reflect(SsaPcRenderer **owner,const SsaNativeGame *g,const SsaRenderCamera *view,
    int pitch,SsaWaterRender *water,uint32_t *output,uint8_t *indexed)
{return reflect_prepared(owner,g,view,pitch,water,output,indexed,NULL);}
int ssa_pc_scene_draw(SsaPcRenderer *r,const SsaNativeGame *g)
{
    SsaSurfaceSampleCache texels={0};Camera c={0};SsaSpriteShadows shadows;int32_t sine,cosine;unsigned x,i,order[100];size_t at,n;
    if(!r || r->updating || !r->pixels || !r->depth || !g || !g->loaded || g->game.updating ||
       g->world.query_active || g->error_line || !g->map.data || g->pc_camera.sector<0 ||
       (unsigned)g->pc_camera.sector>=g->map.sector_count || g->spawn.pool.active_count>100)return -1;
    if(r->hit_capacity<g->map.line_count){SsaPcRayHit *h=malloc((size_t)g->map.line_count*sizeof(*h));if(!h)return -1;
        free(r->hits);r->hits=h;r->hit_capacity=g->map.line_count;}
    r->updating=1;r->rays=r->wall_pixels=r->actor_pixels=r->godray_triangles=0;
    c.samples=&texels;c.bottom=r->height;
    c.x=g->pc_camera.x/65536.0;c.y=g->pc_camera.y/65536.0;c.z=g->pc_camera.z/65536.0;
    if(g->sprite_shadows_enabled){
        if(ssa_sprite_shadows_prepare(&shadows,&g->map,&g->actor_assets,g->sources,g->spawn.pool.active_count,
            &g->players[0].entity,&g->surface_decals,g->shadow_colors[0])<0){r->updating=0;return -1;}
        c.shadows=&shadows;}
    c.decals=g->surface_decals_enabled?&g->surface_decals:NULL;
    c.water=NULL;
    if(g->surface_decals_enabled || g->water_reflections){
        if(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,c.x,c.y,c.z,
            g->frame_game.counter,g->surface_decals_enabled,g->water_reflections,r->width,r->height)<0){r->updating=0;return -1;}
        c.water=&r->water;}
    ssa_sincos(&g->math,g->pc_camera.heading,&sine,&cosine);c.sine=sine/65536.0;c.cosine=cosine/65536.0;
    c.zoom=ssa_pc_scene_fov_scale(g->fov_offset);
    c.focal=r->height*(0x55b05a/65536.0)/80.0;
    ssa_sincos(&g->math,(uint16_t)g->render_pitch,&sine,&cosine);
    c.ps=sine/65536.0;c.pc=cosine/65536.0;
    /* Keep backdrop pitch movement at the original focal distance. Positive
     * pitch looks up: the backdrop moves down. Normal
     * free look is limited to80 degrees; guard vertical/behind-camera inputs. */
    c.sky_shift=c.pc>0?c.focal*c.ps/c.pc:(c.ps>=0?(double)r->height:-(double)r->height);
    c.focal*=c.zoom;
    for(i=0;i<256;++i)c.palette[i]=rgb(g->palette_effect.colors[i]);
    n=(size_t)r->width*r->height;
    /* Hardware replaces every consumed depth sample; flat_sky initializes
     * all colors before upload. Only software needs the initial depth clear. */
    if(!r->hardware){
        if(!g->render_pitch)memset(r->pixels,0,n*sizeof(*r->pixels));
        for(at=0;at<n;++at)r->depth[at]=FLT_MAX;
    }
    if(g->render_pitch || r->hardware){
        if(pitched_geometry(r,g)<0 || pitched_world(r,g,&c)<0){r->updating=0;return -1;}
    }else {
        if(prepare_ray_lines(r,&g->map,&c)<0){r->updating=0;return -1;}
        for(x=0;x<r->width;++x)column(r,g,&c,x);
    }
    if(r->reflection)r->reflection->hardware=r->hardware;
    if(!r->hardware && c.water && reflect_prepared(&r->reflection,g,&g->pc_camera,g->render_pitch,c.water,r->pixels,NULL,c.shadows)<0){r->updating=0;return -1;}
    if(!r->geometry)r->geometry=calloc(1,sizeof(*r->geometry));
    if(!r->geometry || actor_clip_prepare(r,g,&c)<0){r->updating=0;return -1;}
    actor_order(g,&c,order);
    draw_actors(r,g,&c,order,0);
    if(r->hardware){
        if(hardware_finish(r,&c)<0){r->updating=0;return -1;}
        if(c.water){
            if(!r->reflection){r->reflection=calloc(1,sizeof(*r->reflection));if(!r->reflection){r->updating=0;return -1;}}
            r->reflection->hardware=r->hardware;
            if(reflect_prepared(&r->reflection,g,&g->pc_camera,g->render_pitch,c.water,r->pixels,NULL,c.shadows)<0){r->updating=0;return -1;}
        }
    }
    if(g->godrays_enabled && godrays_draw(r,g,&c)<0){r->updating=0;return -1;}
    for(i=0;i<g->spawn.pool.active_count;++i)if(g->sources[i].flags&0x2000)screen_actor(r,g,&c,&g->sources[i]);
    r->updating=0;return 1;
}
