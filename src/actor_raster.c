#include "ssa/actor_raster.h"

static int32_t signed32(uint32_t x) { return x<=0x7fffffffu?(int32_t)x:-(int32_t)(~x)-1; }
static int32_t asr(uint32_t x,unsigned n) {
    return signed32((x>>n)|((x&0x80000000u)?~(UINT32_MAX>>n):0u));
}
int ssa_actor_raster_draw(const SsaActorAssets *a,const SsaActorProjection *p,
    const int16_t *clips,size_t clip_count,uint8_t *fb,size_t fb_size) {
    const SsaActorImage *im;uint32_t dx,dy,fx=0,fy=0,scale,yscale;
    int y,h,pass,col;
    if(!a||!p||!a->loaded||!fb||fb_size<14400||p->clipped_width<0||p->left<0||
       p->left>120||p->clipped_width>120-p->left||p->height<0)return -1;
    im=ssa_actor_assets_image(a,p->animation,p->frame_flags&0x3fffffffu);
    if(!im||!im->pixels||!im->width||!im->height)return -1;
    if(!p->clipped_width||!p->height)return 1;
    if(!clips||clip_count<(size_t)p->clipped_width*2u)return -1;
    scale=(uint32_t)p->scale;
    if(scale==65536u) { dx=a->reciprocal[4096]>>8;dy=a->reciprocal[6144]>>8; }
    else {
        dx=scale<=65535u?a->reciprocal[scale]>>8:0;
        yscale=scale+(uint32_t)asr(scale,1);
        dy=yscale<=65535u?a->reciprocal[yscale]>>8:0;
    }
    if(p->frame_flags&0x80000000u) { fx=((uint32_t)im->width<<12)-1u;dx=0u-dx; }
    fx+=(uint32_t)((int32_t)p->left-p->x)*dx;
    y=p->y;h=p->height;
    if((uint32_t)(y+h)>120u)h=120-y;
    if(y<0) { fy=(uint32_t)(-y)*dy;h+=y;y=0; }
    if(!h)return 1;
    if(h<0||y<0||y+h>120)return -1;
    /* First pass validates every sample, so bad projections cannot partially draw. */
    for(pass=0;pass<2;pass++) {
        uint32_t xfrac=fx;
        for(col=0;col<p->clipped_width;col++,xfrac+=dx) {
            int32_t sx=asr(xfrac,12),base_y=asr(fy,12);
            int top=clips[2*col],bottom=clips[2*col+1],begin=y,end=y+h,row;
            uint32_t frac=0;
            if(sx<0||sx>=im->width)return -1;
            if(top>=begin) { if(top>=end)continue;frac=(uint32_t)(top-begin)*dy;begin=top; }
            if(bottom<=end) { end=bottom;if(end<=begin)continue; }
            if(begin<0||end>120)return -1;
            for(row=begin;row<end;row++,frac+=dy) {
                uint32_t sy=(uint32_t)base_y+(frac>>12);uint8_t pixel;
                if(sy>=im->height)return -1;
                if(!pass)continue;
                pixel=im->pixels[(size_t)sx*im->height+sy];
                if(pixel)fb[(size_t)row*120+(unsigned)p->left+(unsigned)col]=pixel;
            }
        }
    }
    return 1;
}
