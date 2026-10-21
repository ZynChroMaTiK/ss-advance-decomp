#include "ssa/water.h"
#include "ssa/render_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "water_ripples.inc"
enum { WATER_FRAME_TICKS=6 }; /* All water texture animations advance at10FPS. */
enum { WATER_REFLECTION_FADE=384, WATER_REFLECTION_END=768 };
static uint32_t hash(uint32_t n){n^=n>>16;n*=UINT32_C(0x7feb352d);n^=n>>15;n*=UINT32_C(0x846ca68b);return n^(n>>16);}
#include "water_edges.inc"
static unsigned nearest(const uint16_t *palette,unsigned r,unsigned g,unsigned b){unsigned i,best=1,error=UINT32_MAX;
    for(i=1;i<256;++i){int dr=(int)(palette[i]&31)-(int)r,dg=(int)((palette[i]>>5)&31)-(int)g,db=(int)((palette[i]>>10)&31)-(int)b;
        unsigned e=(unsigned)(dr*dr+dg*dg+db*db);if(e<error){error=e;best=i;}}
    return best;}
static unsigned water_light(const uint16_t *palette,unsigned color){
    unsigned a=palette[color],r=a&31,g=(a>>5)&31,b=(a>>10)&31,i,best=color,error=UINT32_MAX;
    unsigned tr=r+(31-r+2)/3,tg=g+(31-g+2)/3,tb=b+(31-b+2)/3;
    /* A lighter original-palette tint, without shifting blue water toward
     * purple/green. Resolve once against the base palette so fades stay live. */
    for(i=1;i<256;++i){unsigned c=palette[i],cr=c&31,cg=(c>>5)&31,cb=(c>>10)&31;int dr,dg,db;unsigned e;
        if(cr<r || cg<g || cb<b || cr+cg+cb<=r+g+b)continue;
        if((r<=g && cr>cg) || (g<=b && cg>cb) || (r<=b && cr>cb))continue;
        dr=(int)cr-(int)tr;dg=(int)cg-(int)tg;db=(int)cb-(int)tb;e=(unsigned)(dr*dr+dg*dg+db*db);
        if(e<error){error=e;best=i;}}
    return best;
}
int ssa_water_scene_init(SsaWaterScene *s,const SsaMapTextureBindings *b,const SsaMathTables *math,const SsaSkyAssets *sky,const uint16_t palette[256]){
    unsigned white,i;
    if(!s || !b || !b->map || !b->map->data || !math || !sky || !palette)return -1;
    s->edges=edges_init(b->map);if(!s->edges)return -1;
    /* Three artwork inks: base water, lighter water, and white accents. */
    white=nearest(palette,31,31,31);
    memset(s->glints,(int)white,sizeof(s->glints));
    for(i=0;i<256;++i)s->ripples[i]=(uint8_t)water_light(palette,i);
    memset(s->foam,(int)white,sizeof(s->foam));
    return 1;
}
void ssa_water_scene_free(SsaWaterScene *s){if(s){edges_free(s->edges);memset(s,0,sizeof(*s));}}
void ssa_water_free(SsaWaterRender *r){if(r){free(r->mask);memset(r,0,sizeof(*r));}}
int ssa_water_begin(SsaWaterRender *r,const SsaWaterScene *s,const SsaSurfaceDecals *d,const uint16_t palette[256],double x,double y,double z,uint32_t frame,unsigned animated_decals,unsigned reflections,unsigned width,unsigned height){
    size_t count;
    if(!r || !palette || !width || !height || (size_t)width>SIZE_MAX/height ||
       (size_t)width*height>SIZE_MAX/sizeof(*r->mask))return -1;
    count=(size_t)width*height;
    if(reflections && count>r->capacity){uint32_t *mask=malloc(count*sizeof(*mask));if(!mask)return -1;
        free(r->mask);r->mask=mask;r->capacity=count;}
    if(r->mask)memset(r->mask,0,(count<r->capacity?count:r->capacity)*sizeof(*r->mask));
    memset(r->valid,0,sizeof(r->valid));r->passes=0;r->actor_pixels=0;r->width=width;r->height=height;
    r->scene=s;r->decals=d;r->palette=palette;r->eye_x=x;r->eye_y=y;r->eye_z=z;r->frame=frame;r->animated_decals=animated_decals;r->reflections=reflections;
    return 1;
}
/* Both animation types share a fixed half-empty64-unit tile layout. Empty
 * tiles stay clear across animation epochs, retaining plain/reflected water. */
static uint32_t water_tile(const SsaSurfaceDecals *d,int32_t tx,int32_t ty){
    return hash(((uint32_t)tx>>5)*UINT32_C(0x9e3779b9)^((uint32_t)ty>>5)*UINT32_C(0x85ebca6b)^d->seed);
}
static int glint_texel(uint32_t cell,int32_t tx,int32_t ty,double distance,uint32_t frame){
    static const uint8_t sprites[4][8]={{0,0,0,8,0,0,0,0},{0,0,8,28,8,0,0,0},
        {0,8,8,62,8,8,0,0},{8,8,28,127,28,8,8,0}};
    uint32_t clock,random;unsigned age,phase,px,py;int dx,dy,kind;
    clock=frame+(cell&127);age=clock&127;
    if(age>=8*WATER_FRAME_TICKS)return 0;
    random=hash(cell^(clock>>7)*UINT32_C(0x27d4eb2d));
    if(random%3)return 0;
    phase=age/WATER_FRAME_TICKS;if(phase>=4)phase=7-phase;
    px=4+(random>>8)%24;py=4+(random>>16)%24;
    dx=(int)((uint32_t)tx&31)-(int)px+3;dy=(int)((uint32_t)ty&31)-(int)py+3;
    if(dx<0 || dx>=8 || dy<0 || dy>=8)return 0;
    kind=(sprites[phase][dy]&(1u<<dx))?1:0;
    if(kind && abs(dx-3)+abs(dy-3)>1)kind=2; /* Tinted arms, white core. */
    return (distance<=768 || (hash((uint32_t)tx^(uint32_t)ty*131)&65535)<(1536-distance)*65536/768)?kind:0;
}
static int ripple_texel(const SsaSurfaceDecals *d,int32_t tx,int32_t ty,double distance,uint32_t frame){
    uint32_t cell,clock,random;unsigned age,px,py,variant;int dx,dy,kind;
    cell=hash(((uint32_t)tx>>5)*UINT32_C(0x517cc1b7)^((uint32_t)ty>>5)*UINT32_C(0x6c8e9cf5)^d->seed^UINT32_C(0x9e3779b9));
    clock=frame+(cell&255);age=clock&255;
    if(age>=72)return 0;
    random=hash(cell^(clock>>8)*UINT32_C(0x165667b1));
    if(random%3==0)return 0;
    /* Keep the larger24-texel artwork wholly inside its occupied32-texel
     * cell, so neighboring empty cells never cut off a growing ring. */
    px=12+(random>>8)%8;py=12+(random>>16)%8;variant=(random>>24)&(WATER_RIPPLE_VARIANTS-1);
    dx=(int)((uint32_t)tx&31)-(int)px+11;dy=(int)((uint32_t)ty&31)-(int)py+11;
    if(dx<0 || dx>=WATER_RIPPLE_SIZE || dy<0 || dy>=WATER_RIPPLE_SIZE)return 0;
    if(random&2)dx=WATER_RIPPLE_SIZE-1-dx;
    if(random&4)dy=WATER_RIPPLE_SIZE-1-dy;
    kind=(water_ripples[variant][age/WATER_FRAME_TICKS][dy]&(UINT32_C(1)<<dx))?2:
        (water_ripples_surface[variant][age/WATER_FRAME_TICKS][dy]&(UINT32_C(1)<<dx))?3:0;
    return (distance<=768 || (hash((uint32_t)tx^(uint32_t)ty*197)&65535)<(1536-distance)*65536/768)?kind:0;
}
int ssa_water_glint(const SsaSurfaceDecals *d,double x,double y,double distance,uint32_t frame){
    int32_t tx,ty;uint32_t tile;
    if(!d || !d->loaded || !(x>-65536 && x<65536 && y>-65536 && y<65536 && distance>=0 && distance<1536))return 0;
    tx=(int32_t)floor(x/2);ty=(int32_t)floor(y/2);tile=water_tile(d,tx,ty);
    return tile&256?0:glint_texel(tile,tx,ty,distance,frame);
}
int ssa_water_ripple(const SsaSurfaceDecals *d,double x,double y,double distance,uint32_t frame){
    int32_t tx,ty;
    if(!d || !d->loaded || !(x>-65536 && x<65536 && y>-65536 && y<65536 && distance>=0 && distance<1536))return 0;
    tx=(int32_t)floor(x/2);ty=(int32_t)floor(y/2);
    return water_tile(d,tx,ty)&256?0:ripple_texel(d,tx,ty,distance,frame);
}
static int water_ink(const SsaWaterRender *r,uint8_t color,double x,double y,double h,double distance)
{
    if(r->animated_decals && r->scene && r->decals->loaded &&
       x>-65536 && x<65536 && y>-65536 && y<65536 && distance>=0 && distance<1536){
        int32_t tx=(int32_t)floor(x/2),ty=(int32_t)floor(y/2);
        uint32_t tile=water_tile(r->decals,tx,ty);
        if(!(tile&256)){
            int glint=glint_texel(tile,tx,ty,distance,r->frame),ripple;
            if(glint)return glint==2?r->scene->ripples[color]:r->scene->glints[color];
            ripple=ripple_texel(r->decals,tx,ty,distance,r->frame);
            if(ripple)return ripple==3?color:r->scene->ripples[color];}
        {int edge=ssa_water_edge_decal(r->scene,r->decals,color,x,y,h,distance,r->frame);
            if(edge)return edge==3?color:edge==2?r->scene->ripples[color]:r->scene->foam[color];}}
    return -1;
}
uint16_t ssa_water_rgb_cached(SsaWaterRender *r,SsaSurfaceSampleCache *cache,uint8_t color,double x,double y,double h,double distance,size_t pixel)
{
    unsigned a;double fade=1;int ink,hit;SsaSurfaceSample *entry;
    if(!r || !r->palette)return 0;
    a=r->palette[color]&0x7fff;
    if(pixel<r->capacity)r->mask[pixel]=0;
    if(!ssa_surface_decals_water(r->decals,color,h))return (uint16_t)a;
    entry=ssa_surface_sample_slot(cache,x,y,h,distance,2,color,&hit);
    if(hit)ink=entry->value;
    else {ink=water_ink(r,color,x,y,h,distance);if(entry)entry->value=ink;}
    if(ink>=0)return r->palette[ink]&0x7fff;
    if(!r->reflections || pixel>=r->capacity || !(distance>=0 && distance<WATER_REFLECTION_END && r->eye_z>h && h>=-32768 && h<=32767))return (uint16_t)a;
    if(distance>WATER_REFLECTION_FADE){fade=(WATER_REFLECTION_END-distance)/(WATER_REFLECTION_END-WATER_REFLECTION_FADE);fade=fade*fade*(3-2*fade);}
    r->mask[pixel]=(uint16_t)(int)h|((uint32_t)color<<16)|((uint32_t)(fade*255+0.5)<<24);
    return (uint16_t)a;
}
uint16_t ssa_water_rgb(SsaWaterRender *r,uint8_t color,double x,double y,double h,double distance,size_t pixel)
{return ssa_water_rgb_cached(r,NULL,color,x,y,h,distance,pixel);}
uint16_t ssa_water_blend(const SsaWaterRender *r,uint32_t mask,uint16_t reflected){
    unsigned a=r->palette[(mask>>16)&255],fade=mask>>24,c,out=0;
    if(!fade)return (uint16_t)(a&0x7fff); /* Exact material color at fade completion. */
    for(c=0;c<3;++c){unsigned base=(a>>(c*5))&31,env=(reflected>>(c*5))&31;
        /* Half-alpha reflection, reduced by the existing distance fade.
         * Both dark and bright scenery mix with the original water color. */
        out|=((base*(510-fade)+env*fade+255)/510)<<(c*5);}
    return (uint16_t)out;
}
uint8_t ssa_water_palette_index(SsaWaterRender *r,uint16_t rgb){
    unsigned key=rgb&0x7fff,mask=1u<<(key&7);
    if(!(r->valid[key>>3]&mask)){r->blended[key]=(uint8_t)nearest(r->palette,key&31,(key>>5)&31,(key>>10)&31);r->valid[key>>3]|=(uint8_t)mask;}
    return r->blended[key];
}
uint8_t ssa_water_reflection_index(SsaWaterRender *r,uint16_t rgb,unsigned x,unsigned y){
    /* One palette entry per reflected color, independent of pixel position. */
    (void)x;(void)y;
    return ssa_water_palette_index(r,rgb);
}
uint8_t ssa_water_index(SsaWaterRender *r,uint8_t color,double x,double y,double h,double distance,size_t pixel){
    uint16_t rgb=ssa_water_rgb(r,color,x,y,h,distance,pixel);
    return rgb==(r->palette[color]&0x7fff)?color:ssa_water_palette_index(r,rgb);
}
int ssa_water_reference(SsaWaterRender *r,const SsaPlanePool *pool,const SsaRenderCamera *camera,uint8_t *fb,size_t capacity){
    unsigned i;double eye,sine,cosine;
    if(!r || !r->decals || !r->palette || !pool || pool->count>SSA_PLANE_LIMIT || !camera || !fb || capacity<14400)return -1;
    eye=camera->z/65536.0;sine=-camera->sine/65536.0;cosine=camera->cosine/65536.0;
    for(i=0;i<pool->count;++i){const SsaPlane *p=&pool->records[i];int x,y;
        if(p->height>=eye || !p->color || !ssa_surface_decals_water(r->decals,(uint8_t)p->color,p->height))continue;
        if(p->min_x>p->max_x)continue;
        if(p->min_x<0 || p->max_x>=120)return -1;
        for(x=p->min_x;x<=p->max_x;++x){unsigned top=p->top[x+1],bottom=p->bottom[x+1];
            if(top==255 || top>bottom)continue;
            if(bottom>=120)return -1;
            for(y=(int)top;y<=(int)bottom;++y){double distance=(eye-p->height)*(0x55b05a/65536.0)*1.5/(y+0.5-60);
                double lateral=(x+0.5-60)/(0x55b05a/65536.0);size_t at=(size_t)y*120+x;
                fb[at]=ssa_water_index(r,(uint8_t)p->color,r->eye_x+(sine+cosine*lateral)*distance,r->eye_y+(cosine-sine*lateral)*distance,p->height,distance,at);}}
    }
    return 1;
}
