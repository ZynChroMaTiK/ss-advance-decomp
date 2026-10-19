#include "ssa/display.h"
#include <stdint.h>

static int overlaps(const void *a,size_t n,const void *b,size_t m)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(n>UINTPTR_MAX-x || m>UINTPTR_MAX-y)return 1;
    return x<y+m && y<x+n;
}
static int hidden(const SsaSpriteOamEntry *o)
{return (o->attribute0&0x300)==0x200;}
static void dimensions(const SsaSpriteOamEntry *o,unsigned *w,unsigned *h)
{
    static const uint8_t widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static const uint8_t heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    unsigned shape=o->attribute0>>14,size=o->attribute1>>14;
    *w=widths[shape][size];*h=heights[shape][size];
}
static int signed_half(uint16_t v)
{return v<=32767?(int)v:(int)v-65536;}
static int floor256(int value)
{return value>=0?value/256:-1-((-1-value)/256);}
int ssa_display_registers_default(SsaDisplayRegisters *r)
{
    if(!r)return -1;
    *r=(SsaDisplayRegisters){0x1441,0x6085,128,0,0,192,0,0,0,0};return 1;
}
static int validate(const SsaDisplayRegisters *r,const SsaSpriteOam *o)
{
    unsigned i,y,budget=(r->dispcnt&32)?954:1210;
    if((r->dispcnt&~32u)!=0x1441 || (r->bg2cnt!=0x6085 && r->bg2cnt!=0x6089) ||
       (r->bldcnt&0xc0))return -2;
    if(r->x < -134217728 || r->x>134217727 || r->y < -134217728 || r->y>134217727)return -1;
    if(!o)return 1;
    if(o->loaded!=1 || o->updating || o->count>128 || o->affine_count>32)return -1;
    for(i=0;i<128;++i) {
        const SsaSpriteOamEntry *p=&o->entries[i];
        if(hidden(p))continue;
        if((p->attribute0>>14)==3 || (p->attribute0&0xc00)==0xc00)return -1;
        if(!(p->attribute0&0x2000) || (p->attribute0&0xc00) ||
           ((p->attribute0&0x1000) && (r->mosaic&0xff00)))return -2;
    }
    /* Conservative bound includes the complete128-entry scan, offscreen X,
     * and un-clipped affine boxes. Never pretend an overloaded line is exact. */
    for(y=0;y<160;++y) {
        unsigned cost=256;
        for(i=0;i<128;++i) {
            const SsaSpriteOamEntry *p=&o->entries[i];unsigned w,h,affine;
            if(hidden(p))continue;
            dimensions(p,&w,&h);affine=(p->attribute0>>8)&1;
            if(affine && (p->attribute0&0x200)){w*=2;h*=2;}
            if(((y-(p->attribute0&255))&255)>=h)continue;
            cost+=affine?10+2*w:w;
        }
        if(cost>budget)return -2;
    }
    return 1;
}
static int compose(const SsaDisplayRegisters *r,const uint8_t *world,size_t world_size,
    const uint16_t bg[256],const uint16_t obj[256],const SsaSpriteOam *o,
    const uint8_t *tiles,size_t tile_size,uint16_t *out,size_t output_pixels,
    int gameplay,int16_t health,int16_t armor)
{
    /* 0750C/0787C replace OBJ139..143 at VCOUNT130 and142. These are
     * the four original five-color ramps at0844A804..0844A82B. */
    static const uint16_t ramps[4][5]={
        {0x2400,0x3400,0x4442,0x50a5,0x6129},
        {0x0120,0x01a0,0x0a22,0x1685,0x2709},
        {0x0009,0x000d,0x1051,0x14b4,0x2538},
        {0x000c,0x0430,0x1475,0x1cfa,0x2d7f}};
    unsigned x,y,i;int result;
    if(!r || !world || world_size<14400 || !bg || !obj || !out || output_pixels<38400 ||
       (o && (!tiles || tile_size<32768)))return -1;
    if(overlaps(out,76800,r,sizeof(*r)) || overlaps(out,76800,world,14400) ||
       overlaps(out,76800,bg,512) || overlaps(out,76800,obj,512) ||
       (o && (overlaps(out,76800,o,sizeof(*o)) || overlaps(out,76800,tiles,32768))))return -1;
    result=validate(r,o);if(result<0)return result;
    for(y=0;y<160;++y) {
        uint8_t indices[240]={0},priorities[240]={0};
        if(o)for(i=0;i<128;++i) {
            const SsaSpriteOamEntry *p=&o->entries[i];unsigned w,h,bw,bh,dy,dx,affine,priority;
            if(hidden(p))continue;
            dimensions(p,&w,&h);bw=w;bh=h;affine=(p->attribute0>>8)&1;priority=(p->attribute2>>10)&3;
            if(affine && (p->attribute0&0x200)){bw*=2;bh*=2;}
            dy=(y-(p->attribute0&255))&255;if(dy>=bh)continue;
            for(dx=0;dx<bw;++dx) {
                unsigned screen=(dx+(p->attribute1&511))&511,offset;int sx,sy;uint8_t color;
                if(screen>=240 || (indices[screen] && priority>=priorities[screen]))continue;
                if(affine) {
                    unsigned matrix=((p->attribute1>>9)&31)*4;
                    int a=signed_half(o->entries[matrix].affine),b=signed_half(o->entries[matrix+1].affine);
                    int c=signed_half(o->entries[matrix+2].affine),d=signed_half(o->entries[matrix+3].affine);
                    int u=(int)dx-(int)(bw/2),v=(int)dy-(int)(bh/2);
                    sx=floor256(a*u+b*v+(int)w*128);sy=floor256(c*u+d*v+(int)h*128);
                    if(sx<0 || sy<0 || sx>=(int)w || sy>=(int)h)continue;
                } else {
                    sx=(int)((p->attribute1&0x1000)?w-1-dx:dx);
                    sy=(int)((p->attribute1&0x2000)?h-1-dy:dy);
                }
                offset=(p->attribute2&1023)*32+
                    ((unsigned)sy/8*(w/8)+(unsigned)sx/8)*64+((unsigned)sy&7)*8+((unsigned)sx&7);
                color=tiles[offset&32767];
                /* OBJ pixels are reduced before BG composition. Equal priority
                 * keeps the earlier OAM entry. A later higher-priority sample
                 * replaces its color when opaque; transparent samples retain
                 * the earlier color but promote its priority (GBA anomaly). */
                if(color || indices[screen])priorities[screen]=(uint8_t)priority;
                if(color)indices[screen]=color;
            }
        }
        for(x=0;x<240;++x) {
            int64_t fx=(int64_t)r->x+(int64_t)r->pa*x+(int64_t)r->pb*y;
            int64_t fy=(int64_t)r->y+(int64_t)r->pc*x+(int64_t)r->pd*y;
            unsigned sx=(unsigned)((uint64_t)fx>>8)&255,sy=(unsigned)((uint64_t)fy>>8)&255;
            uint8_t index=(sx<120 && sy<120)?world[sy*120+sx]:0;
            uint16_t color=bg[index];
            if(indices[x] && (!index || priorities[x]<=(r->bg2cnt&3))){
                unsigned n=indices[x];color=obj[n];
                if(gameplay && n>=139 && n<=143)
                    color=ramps[y>=142?(health>100?3:2):(y>=130 && armor?1:0)][n-139];
            }
            out[y*240+x]=(uint16_t)(color&0x7fff);
        }
    }
    return 1;
}
int ssa_display_compose(const SsaDisplayRegisters *r,const uint8_t *world,size_t world_size,
    const uint16_t bg[256],const uint16_t obj[256],const SsaSpriteOam *o,
    const uint8_t *tiles,size_t tile_size,uint16_t *out,size_t output_pixels)
{return compose(r,world,world_size,bg,obj,o,tiles,tile_size,out,output_pixels,0,0,0);}
int ssa_display_compose_gameplay(const SsaDisplayRegisters *r,const uint8_t *world,size_t world_size,
    const uint16_t bg[256],const uint16_t obj[256],const SsaSpriteOam *o,
    const uint8_t *tiles,size_t tile_size,uint16_t *out,size_t output_pixels,
    int16_t health,int16_t armor)
{return compose(r,world,world_size,bg,obj,o,tiles,tile_size,out,output_pixels,1,health,armor);}
int ssa_display_rgb888(const uint16_t *in,size_t count,uint32_t *out,size_t capacity)
{
    unsigned i;
    if(!in || !out || count<38400 || capacity<38400 || overlaps(in,76800,out,153600))return -1;
    for(i=0;i<38400;++i) {
        unsigned r=in[i]&31,g=(in[i]>>5)&31,b=(in[i]>>10)&31;
        r=(r<<3)|(r>>2);g=(g<<3)|(g>>2);b=(b<<3)|(b>>2);
        out[i]=(r<<16)|(g<<8)|b;
    }
    return 1;
}
