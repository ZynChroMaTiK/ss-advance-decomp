#include "ssa/pc_scene.h"
#include "ssa/native_game.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

typedef struct SsaPcRayHit {double distance,u;unsigned line;} SsaPcRayHit;
typedef struct SsaPcRayLine {double ax,ay,vx,vy,numerator;} SsaPcRayLine;
typedef struct Camera {double x,y,z,sine,cosine,focal,ps,pc,sky_shift,clip_z;unsigned clip,top,bottom;uint32_t palette[256];const SsaSurfaceDecals *decals;SsaWaterRender *water;} Camera;
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
                    r->pixels[at]=rgb(ssa_water_rgb(c->water,(uint8_t)color,wx,wy,height,d,at));
                    r->depth[at]=d>0 && d<FLT_MAX?(float)d:FLT_MAX;continue;}
                if(c->decals && !(height<c->z && ssa_surface_decals_water(c->decals,color,height)))
                pixel=ssa_surface_decals_sample(c->decals,height>c->z,(uint8_t)color,
                    wx,wy,d);}
            r->pixels[at]=c->palette[pixel];r->depth[at]=d>0 && d<FLT_MAX?(float)d:FLT_MAX;}
    }
}
static const SsaTextureImage *texture(const SsaNativeGame *g,unsigned side,unsigned layer)
{
    unsigned index;if(side>=g->map.side_count)return NULL;
    index=u16(g->map.data+g->map.sides+(size_t)side*12+4+layer*2);
    return index==65535?NULL:ssa_map_texture_image(&g->bindings,index,0);
}
static void wall(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,unsigned x,int top,int bottom,
    double distance,double along,double origin,unsigned side,unsigned layer)
{
    const SsaTextureImage *im=texture(g,side,layer);int y;unsigned sx;
    if(!im || !im->pixels || !im->width || !im->height){
        /* A missing surface image must not reveal actors behind its geometry. */
        for(y=top;y<bottom;++y)r->depth[(size_t)y*r->width+x]=(float)distance;
        return;
    }
    sx=wrap(along/2,im->width);
    for(y=top;y<bottom;++y){double z=c->z+((double)r->height/2-y-0.5)*distance/c->focal;
        unsigned sy=wrap((origin-z)/2,im->height);size_t at=(size_t)y*r->width+x;
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
        unsigned direction,side_index,flags=u16(l+26);int other,ceil_y,floor_y,upper,lower;double d=hit->distance,along,len,origin;
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
         double vx=s16(v)-s16(a),vy=s16(v+2)-s16(a+2);len=length(vx,vy);}
        along=(direction?1-hit->u:hit->u)*len+s16(side);
        if(!(flags&4)){
            const SsaTextureImage *im=texture(g,side_index,SSA_MAP_SIDE_MIDDLE);
            origin=(flags&16) && im?ff+im->height*2.0:fc;
            wall(r,g,c,x,top,bottom,d,along,origin+s16(side+2),side_index,SSA_MAP_SIDE_MIDDLE);return;
        }
        other=(int)u16(l+12+(direction^1u)*2);if((unsigned)other>=m->sector_count)break;
        b=m->data+m->sectors+(size_t)other*20;bc=s16(b+2);bf=s16(b);
        upper=edge((double)r->height/2-(bc-c->z)*c->focal/d,r->height);
        lower=edge((double)r->height/2-(bf-c->z)*c->focal/d,r->height);
        if(bc<fc){const SsaTextureImage *im=texture(g,side_index,SSA_MAP_SIDE_UPPER);
            origin=(flags&8) || !u16(l+36)?fc:bc+(im?im->height*2.0:0);
            wall(r,g,c,x,top,upper<bottom?upper:bottom,d,along,origin+s16(side+2),side_index,SSA_MAP_SIDE_UPPER);}
        if(bf>ff)wall(r,g,c,x,lower>top?lower:top,bottom,d,along,bf+s16(side+2),side_index,SSA_MAP_SIDE_LOWER);
        if(upper>top)top=upper;
        if(lower<bottom)bottom=lower;
        sector=other;
    }
    if(top<bottom){const uint8_t *s=m->data+m->sectors+(size_t)sector*20;int middle=(int)(r->height/2);
        if(top<middle)plane(r,g,c,x,top,bottom<middle?bottom:middle,s16(s+2),s[11]);
        if(bottom>middle)plane(r,g,c,x,top>middle?top:middle,bottom,s16(s),s[10]);}
}
static void actor(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,const SsaActorSource *s)
{
    const SsaActorResource *resource;const SsaActorImage *im;const SsaBody *b;
    double dx,dy,d,lateral,scale,x,y,w,h,unit;unsigned image,flip=0,dirs,bit;int dir,left,right,top,bottom,xx,yy,cut=0;
    uint16_t facing;
    if(!s->entity || !s->animation || s->animation>=SSA_ACTOR_RESOURCE_COUNT || s->frame>=65534 ||
       (s->flags&128) || ((s->flags&64) && (g->frame_game.flags&1)))return;
    b=&s->entity->body;
    resource=&g->actor_assets.resources[s->animation];if(!resource->directions || !resource->mip_flags)return;
    dx=b->x/65536.0-c->x;dy=b->y/65536.0-c->y;
    d=dx*c->sine+dy*c->cosine;lateral=dx*c->cosine-dy*c->sine;
    if(s->flags&0x2000){image=s->frame+1u;scale=(double)r->height/160;d=0.0001;x=r->width/2.0;y=r->height/2.0;
        double hud_scale=r->width/240.0<scale?r->width/240.0:scale;
        flip=g->alternates.enabled && (s->flags&SSA_ACTOR_SCREEN_MIRROR)!=0;
        x+=g->weapon_bob_x/256.0*hud_scale;
        y+=g->weapon_bob_y/256.0*hud_scale;}
    else {
        if(d<=0.01)return;
        facing=(uint16_t)((unsigned)s->facing*256+((s->flags&16)?32768:0));
        dir=((unsigned)(uint16_t)(facing-ssa_direction(&g->math,(int16_t)(b->x/65536-(int)c->x),
            (int16_t)(b->y/65536-(int)c->y)))+0x800)>>12;dir&=15;
        if(dir>8){dir=16-dir;flip=1;}
        dirs=resource->directions-1u;if(!dirs){dir=0;flip=0;}else while(dirs<=7){dirs*=2;dir/=2;}
        image=(unsigned)s->frame+1+(unsigned)dir*resource->frames;
        bit=0;while(bit<5 && !(resource->mip_flags&(1u<<bit)))++bit;
        unit=bit==0?4.0/3:bit==1?2:bit==2?8.0/3:bit==3?4:8;
        scale=c->focal/d*unit;x=r->width/2.0+lateral*c->focal/d;y=r->height/2.0-(b->z/65536.0-c->z)*c->focal/d;
        if(b->step_adjust){cut=b->floor+b->step_adjust-b->z/65536;if(cut<0)cut=0;}
    }
    im=ssa_actor_assets_image(&g->actor_assets,s->animation,image);if(!im || cut>=(int)im->height)return;
    x+=(flip?-(im->origin_x+(int)im->width):im->origin_x)*scale;y+=im->origin_y*scale;
    w=im->width*scale;h=(im->height-cut)*scale;
    left=edge(x,r->width);right=edge(x+w,r->width);top=edge(y,r->height);bottom=edge(y+h,r->height);
    for(yy=top;yy<bottom;++yy){unsigned sy=(unsigned)(((double)yy+0.5-y)/scale);if(sy>=im->height)continue;
        for(xx=left;xx<right;++xx){unsigned sx=(unsigned)(((double)xx+0.5-x)/scale);uint8_t index;size_t at=(size_t)yy*r->width+(unsigned)xx;
            if(sx>=im->width || d>r->depth[at]+0.001)continue;
            if(flip)sx=im->width-1-sx;
            index=im->pixels[(size_t)sx*im->height+sy];
            if(index){r->pixels[at]=c->palette[index];r->depth[at]=(float)d;++r->actor_pixels;}
        }
    }
}
/* The Classic240x160 layer is a layout atlas. Its disjoint regions retain
 * pixel-art scaling while their anchors follow the actual viewport edges.
 * Header panels follow the top edge, and inbox movement interpolates between
 * the right-edge anchor and the same upper-center anchor as analysis. */
#include "pc_renderer_pitch.inc"
int ssa_pc_renderer_reflect(SsaPcRenderer **owner,const SsaNativeGame *g,const SsaRenderCamera *view,
    int pitch,SsaWaterRender *water,uint32_t *output,uint8_t *indexed)
{
    uint8_t seen[8192]={0};size_t at,count;unsigned width,height;SsaPcRenderer *r;
    if(!owner || !g || !view || !water || (!output && !indexed))return -1;
    if(!water->reflections)return 1;
    count=(size_t)water->width*water->height;
    if(!water->mask || count>water->capacity)return -1;
    for(at=0;at<count && !(water->mask[at]>>24);++at){}
    if(at==count)return 1; /* No visible reflective water: no second pass. */
    width=indexed?240:water->width;height=indexed?160:water->height;
    if(!*owner){*owner=calloc(1,sizeof(**owner));if(!*owner)return -1;}
    r=*owner;if(r->updating || ssa_pc_renderer_resize(r,width,height)<0)return -1;
    for(;at<count;++at){unsigned key=water->mask[at]&65535,i,y,first,last;size_t p;Camera c={0};int h;
        int32_t sine,cosine;
        if(!(water->mask[at]>>24) || (seen[key>>3]&(1u<<(key&7))))continue;
        seen[key>>3]|=(uint8_t)(1u<<(key&7));h=key<32768?(int)key:(int)key-65536;
        /* Only reflected rows sampled by visible water can affect this frame.
         * Keep the full projection and each scanline's interpolation unchanged. */
        first=last=(unsigned)(at/water->width);
        for(p=at+1;p<count;++p)if((water->mask[p]>>24) && (water->mask[p]&65535)==key)
            last=(unsigned)(p/water->width);
        c.top=(unsigned)(((uint64_t)(water->height-1-last)*2+1)*height/(2*(uint64_t)water->height));
        c.bottom=1+(unsigned)(((uint64_t)(water->height-1-first)*2+1)*height/(2*(uint64_t)water->height));
        c.x=view->x/65536.0;c.y=view->y/65536.0;c.z=2.0*h-view->z/65536.0;
        c.clip=1;c.clip_z=h;c.decals=g->surface_decals_enabled?&g->surface_decals:NULL;
        ssa_sincos(&g->math,view->heading,&sine,&cosine);c.sine=sine/65536.0;c.cosine=cosine/65536.0;
        ssa_sincos(&g->math,(uint16_t)-pitch,&sine,&cosine);c.ps=sine/65536.0;c.pc=cosine/65536.0;
        c.focal=height*(0x55b05a/65536.0)/80.0;
        c.sky_shift=c.pc>0?c.focal*c.ps/c.pc:(c.ps>=0?(double)height:-(double)height);
        for(i=0;i<256;++i)c.palette[i]=rgb(g->palette_effect.colors[i]);
        r->updating=1;r->rays=r->wall_pixels=r->actor_pixels=0;
        for(p=(size_t)c.top*width;p<(size_t)c.bottom*width;++p)r->depth[p]=FLT_MAX;
        if(pitched_world(r,g,&c)<0){r->updating=0;return -1;}
        for(i=0;i<g->spawn.pool.active_count;++i){SsaActorSource s=g->sources[i];SsaEntity display;
            if(s.flags&0x2000)continue; /* HUD and fixed-screen flashes never reflect. */
            if(s.entity==&g->players[0].entity)s.flags&=UINT16_C(0xff7f);
            if(g->source_z_offsets[i] && s.entity){int64_t z=(int64_t)s.entity->body.z+g->source_z_offsets[i];
                display=*s.entity;display.body.z=z<INT32_MIN?INT32_MIN:z>INT32_MAX?INT32_MAX:(int32_t)z;s.entity=&display;}
            pitched_actor(r,g,&c,&s);
            if(s.flags&16){s.animation=(uint16_t)(s.animation+5*(s.owner+1));pitched_actor(r,g,&c,&s);}}
        r->updating=0;++water->passes;water->actor_pixels+=r->actor_pixels;
        for(y=first;y<=last;++y){unsigned x;
            unsigned sy=(unsigned)(((uint64_t)(water->height-1-y)*2+1)*height/(2*(uint64_t)water->height));
            const uint32_t *row=r->pixels+(size_t)sy*width;
            for(x=0,p=(size_t)y*water->width;x<water->width;++x,++p)if((water->mask[p]>>24) && (water->mask[p]&65535)==key){
            unsigned sx=width==water->width?x:(unsigned)(((uint64_t)x*2+1)*width/(2*(uint64_t)water->width));
            uint32_t color=row[sx];
            uint16_t reflected=(uint16_t)(((color>>19)&31)|(((color>>11)&31)<<5)|(((color>>3)&31)<<10));
            uint16_t blend=ssa_water_multiply(water,water->mask[p],reflected);
            if(output)output[p]=rgb(blend);
            if(indexed)indexed[p]=ssa_water_palette_index(water,blend);
        }}
    }
    return 1;
}
int ssa_pc_scene_draw(SsaPcRenderer *r,const SsaNativeGame *g)
{
    Camera c={0};int32_t sine,cosine;unsigned x,i;size_t at,n;
    if(!r || r->updating || !r->pixels || !r->depth || !g || !g->loaded || g->game.updating ||
       g->world.query_active || g->error_line || !g->map.data || g->pc_camera.sector<0 ||
       (unsigned)g->pc_camera.sector>=g->map.sector_count)return -1;
    if(r->hit_capacity<g->map.line_count){SsaPcRayHit *h=malloc((size_t)g->map.line_count*sizeof(*h));if(!h)return -1;
        free(r->hits);r->hits=h;r->hit_capacity=g->map.line_count;}
    r->updating=1;r->rays=r->wall_pixels=r->actor_pixels=0;
    c.bottom=r->height;
    c.x=g->pc_camera.x/65536.0;c.y=g->pc_camera.y/65536.0;c.z=g->pc_camera.z/65536.0;
    c.decals=g->surface_decals_enabled?&g->surface_decals:NULL;
    c.water=NULL;
    if(g->surface_decals_enabled || g->water_reflections){
        if(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,c.x,c.y,c.z,
            g->frame_game.counter,g->surface_decals_enabled,g->water_reflections,r->width,r->height)<0){r->updating=0;return -1;}
        c.water=&r->water;}
    ssa_sincos(&g->math,g->pc_camera.heading,&sine,&cosine);c.sine=sine/65536.0;c.cosine=cosine/65536.0;
    c.focal=r->height*(0x55b05a/65536.0)/80.0;
    ssa_sincos(&g->math,(uint16_t)g->render_pitch,&sine,&cosine);
    c.ps=sine/65536.0;c.pc=cosine/65536.0;
    /* Positive pitch looks up: the horizon and backdrop move down. Normal
     * free look is limited to80 degrees; guard vertical/behind-camera inputs. */
    c.sky_shift=c.pc>0?c.focal*c.ps/c.pc:(c.ps>=0?(double)r->height:-(double)r->height);
    for(i=0;i<256;++i)c.palette[i]=rgb(g->palette_effect.colors[i]);
    n=(size_t)r->width*r->height;
    if(!g->render_pitch)memset(r->pixels,0,n*sizeof(*r->pixels));
    for(at=0;at<n;++at)r->depth[at]=FLT_MAX;
    if(g->render_pitch){
        if(pitched_world(r,g,&c)<0){r->updating=0;return -1;}
    }else {
        if(prepare_ray_lines(r,&g->map,&c)<0){r->updating=0;return -1;}
        for(x=0;x<r->width;++x)column(r,g,&c,x);
    }
    if(c.water && ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,c.water,r->pixels,NULL)<0){r->updating=0;return -1;}
    for(i=0;i<g->spawn.pool.active_count;++i){SsaActorSource s=g->sources[i];SsaEntity display;
        if(g->source_z_offsets[i] && s.entity){int64_t z=(int64_t)s.entity->body.z+g->source_z_offsets[i];
            display=*s.entity;display.body.z=z<INT32_MIN?INT32_MIN:z>INT32_MAX?INT32_MAX:(int32_t)z;s.entity=&display;}
        if(g->render_pitch && !(s.flags&0x2000))pitched_actor(r,g,&c,&s);else actor(r,g,&c,&s);
        if(s.flags&16){s.animation=(uint16_t)(s.animation+5*(s.owner+1));
            if(g->render_pitch && !(s.flags&0x2000))pitched_actor(r,g,&c,&s);else actor(r,g,&c,&s);}}
    r->updating=0;return 1;
}
