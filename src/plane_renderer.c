#include "ssa/plane_renderer.h"
#include "ssa/raster_fill.h"
#include <string.h>

_Static_assert(sizeof(SsaPlane)==252,"original plane size");
static int pool_valid(const SsaPlanePool *p)
{return p && p->capacity && p->capacity<=SSA_PLANE_LIMIT && p->count<=p->capacity;}
static int16_t signed16(uint32_t n)
{n&=65535u;return (int16_t)(n<32768u?(int32_t)n:(int32_t)n-65536);}
int ssa_plane_pool_init(SsaPlanePool *p,unsigned level)
{
    if(!p || level>=16)return -1;
    memset(p,0,sizeof(*p));p->capacity=level==2?80:64;return 1;
}
int ssa_plane_pool_reset(SsaPlanePool *p)
{
    if(!pool_valid(p))return -1;
    p->count=0;p->opening_cursor=0;return 1;
}
static void initialize_plane(SsaPlane *p,int16_t height,uint16_t color,int16_t left,int16_t right)
{
    p->height=height;p->color=color;p->min_x=left;p->max_x=right;
    memset(p->top+1,255,120);
}
SsaPlane *ssa_plane_find(SsaPlanePool *p,int32_t height,unsigned color)
{
    unsigned i;int16_t h=signed16((uint32_t)height);uint16_t c=(uint8_t)color;
    if(!pool_valid(p))return NULL;
    for(i=0;i<p->count;++i)
        if(p->records[i].height==h && p->records[i].color==c)return &p->records[i];
    if(p->count==p->capacity)return NULL;
    initialize_plane(&p->records[p->count],h,c,120,-1);
    return &p->records[p->count++];
}
SsaPlane *ssa_plane_check(SsaPlanePool *p,SsaPlane *plane,int32_t left,int32_t right)
{
    unsigned i;int32_t overlap_left,overlap_right,union_left,union_right,x;
    if(!pool_valid(p) || !plane || left<0 || right>=120 || left>right)return NULL;
    for(i=0;i<p->count && &p->records[i]!=plane;++i){}
    if(i==p->count)return NULL;
    if(plane->min_x<0 || plane->min_x>120 || plane->max_x< -1 || plane->max_x>=120)return NULL;
    overlap_left=left>plane->min_x?left:plane->min_x;
    overlap_right=right<plane->max_x?right:plane->max_x;
    union_left=left<plane->min_x?left:plane->min_x;
    union_right=right>plane->max_x?right:plane->max_x;
    for(x=overlap_left;x<=overlap_right;++x)if(plane->top[x+1]!=255)break;
    if(x>overlap_right) {
        plane->min_x=(int16_t)union_left;plane->max_x=(int16_t)union_right;return plane;
    }
    if(p->count==p->capacity)return NULL;
    initialize_plane(&p->records[p->count],plane->height,plane->color,(int16_t)left,(int16_t)right);
    return &p->records[p->count++];
}
int ssa_sky_assets_load(SsaSkyAssets *out,const void *data,size_t size)
{
    const uint8_t *rom=(const uint8_t *)data;
    if(!out || !rom || out->loaded || size<0x46DC5Cu ||
       SSA_SKY_SAMPLE_BYTES>size-0x46DC5Cu)return -1;
    memcpy(out->pixels[0],rom+0x46285Cu,SSA_SKY_SAMPLE_BYTES);
    memcpy(out->pixels[1],rom+0x46DC5Cu,SSA_SKY_SAMPLE_BYTES);
    out->loaded=1;return 1;
}
static int span(SsaPlaneRenderer *r,const SsaSkyAssets *sky,unsigned level,
    uint8_t *fb,size_t capacity,int32_t row,int32_t left,int32_t right)
{
    size_t dest,source;uint32_t count;unsigned family;
    if(row>119)return 1;
    if(row<0 || left<0 || right>=120 || left>right || !fb)return -1;
    count=(uint32_t)(right-left+1);dest=(size_t)row*120u+(unsigned)left;
    if(r->color)return ssa_raster_fill_row(fb,capacity,dest,count,r->color);
    if(!sky || !sky->loaded || level>=16 || r->sky_offset>639)return -1;
    family=level<=5 || level==12 || level==13?0u:1u;
    source=(size_t)row*768u+r->sky_offset+(unsigned)left;
    return ssa_raster_copy_sky(fb,capacity,dest,sky->pixels[family],SSA_SKY_SAMPLE_BYTES,source,count);
}
int ssa_plane_span(SsaPlaneRenderer *r,const SsaSkyAssets *sky,unsigned level,
    uint8_t *fb,size_t capacity,int32_t row,int32_t left,int32_t right)
{
    if(!r || r->updating)return -1;
    return span(r,sky,level,fb,capacity,row,left,right);
}
static int interval(unsigned top,unsigned bottom)
{return bottom<120 && (top<120 || top==255);}
static int edges(SsaPlaneRenderer *r,const SsaSkyAssets *sky,unsigned level,
    uint8_t *fb,size_t capacity,int32_t x,unsigned t1,unsigned b1,unsigned t2,unsigned b2)
{
    /* AA4C's four loops retain this exact close-top/close-bottom/open-top/
     * open-bottom order. Empty columns are represented by top > bottom. */
    while(t1<t2 && t1<=b1) {
        if(span(r,sky,level,fb,capacity,(int32_t)t1,r->row_starts[t1],x-1)<0)return -1;
        ++t1;
    }
    while(b1>b2 && b1>=t1) {
        if(span(r,sky,level,fb,capacity,(int32_t)b1,r->row_starts[b1],x-1)<0)return -1;
        --b1;
    }
    while(t2<t1 && t2<=b2)r->row_starts[t2++]=x;
    while(b2>b1 && b2>=t2)r->row_starts[b2--]=x;
    return 1;
}
int ssa_plane_edges(SsaPlaneRenderer *r,const SsaSkyAssets *sky,unsigned level,
    uint8_t *fb,size_t capacity,int32_t x,unsigned t1,unsigned b1,unsigned t2,unsigned b2)
{
    if(!r || r->updating || x<0 || x>120 || !interval(t1,b1) || !interval(t2,b2))return -1;
    return edges(r,sky,level,fb,capacity,x,t1,b1,t2,b2);
}
int ssa_plane_render(SsaPlaneRenderer *r,SsaPlanePool *pool,const SsaSkyAssets *sky,
    unsigned level,uint16_t heading,int16_t eye_z,uint8_t *fb,size_t capacity)
{
    unsigned i;int32_t x;
    if(!r || r->updating || !pool_valid(pool) || !fb || capacity<14400 || level>=16)return -1;
    /* Valid native plane storage excludes malformed out-of-viewport row
     * indices, which would alias unrelated original globals. Check before any
     * mutation; valid inactive columns retain their bottom values. */
    for(i=0;i<pool->count;++i) {
        SsaPlane *p=&pool->records[i];
        if(p->min_x>p->max_x)continue;
        if(p->min_x<0 || p->max_x>=120)return -1;
        for(x=p->min_x;x<=p->max_x;++x)
            if(!interval(p->top[x+1],p->bottom[x+1]))return -1;
        if(p->bottom[p->min_x]>=120 || p->bottom[p->max_x+2]>=120)return -1;
        if(!(uint8_t)p->color && (!sky || !sky->loaded))return -1;
    }
    r->updating=1;r->sky_offset=((uint32_t)heading*5u)>>9;
    for(i=0;i<pool->count;++i) {
        SsaPlane *p=&pool->records[i];int32_t distance;
        if(p->min_x>p->max_x)continue;
        distance=(int32_t)p->height-eye_z;
        r->height_distance=(uint32_t)(distance<0?-distance:distance);r->color=(uint8_t)p->color;
        p->top[p->min_x]=255;p->top[p->max_x+2]=255;
        for(x=p->min_x;x<=(int32_t)p->max_x+1;++x)
            if(edges(r,sky,level,fb,capacity,x,p->top[x],p->bottom[x],p->top[x+1],p->bottom[x+1])<0) {
                r->updating=0;return -1;
            }
    }
    r->updating=0;return 1;
}
