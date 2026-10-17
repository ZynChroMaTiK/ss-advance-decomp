#include "ssa/actor_assets.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16; }
static int16_t s16(uint16_t x) { return x<32768?(int16_t)x:(int16_t)((int32_t)x-65536); }
static int range(size_t size,size_t offset,size_t count) { return offset<=size && count<=size-offset; }
static int pointer(uint32_t p,size_t size,size_t bytes,size_t *out) {
    if(p<0x08000000u || p>=0x0a000000u)return 0;
    *out=(size_t)(p-0x08000000u);return range(size,*out,bytes);
}
void ssa_actor_assets_free(SsaActorAssets *a) {
    unsigned g,i;if(!a)return;
    for(g=0;g<SSA_ACTOR_RESOURCE_COUNT;g++) {
        SsaActorResource *r=&a->resources[g];
        if(r->images)for(i=0;i<r->count;i++)free(r->images[i].pixels);
        free(r->images);
    }
    memset(a,0,sizeof(*a));
}
int ssa_actor_assets_load(SsaActorAssets *a,const void *source,size_t size) {
    const uint8_t *rom=(const uint8_t *)source;SsaActorAssets *next;
    size_t total=0;unsigned g,i;
    if(!a||!rom||!range(size,0x449d2c,SSA_ACTOR_RESOURCE_COUNT*12u)||
       !range(size,0x34934,65536u*4u)||!range(size,0x44aa74,121))return -1;
    next=(SsaActorAssets *)calloc(1,sizeof(*next));if(!next)return -1;
    memcpy(next->closed_top,rom+0x44a9fc,121);
    memcpy(next->closed_bottom,rom+0x44aa74,121);
    for(i=0;i<65536;i++)next->reciprocal[i]=u32(rom+0x34934+4u*i);
    for(g=0;g<SSA_ACTOR_RESOURCE_COUNT;g++) {
        SsaActorResource *r=&next->resources[g];const uint8_t *d=rom+0x449d2c+12u*g;
        uint32_t count;unsigned bits=0,m;size_t table,animation;
        r->directions=u16(d+8);r->mip_flags=u16(d+10);
        if(!pointer(u32(d+4),size,2,&animation))goto fail;
        r->animation_offset=(uint32_t)animation;
        if(!u32(d))continue;
        if(!pointer(u32(d),size,4,&table))goto fail;
        count=u32(rom+table);
        for(m=r->mip_flags;m;m>>=1)bits+=m&1u;
        if(!count||count>3000||!r->directions||!bits||!range(size,table,4u*(count+1u)))goto fail;
        r->table_offset=(uint32_t)table;r->count=(uint16_t)count;
        r->frames=(uint8_t)((count/r->directions)/bits);
        r->images=(SsaActorImage *)calloc(count,sizeof(*r->images));if(!r->images)goto fail;
        for(i=0;i<count;i++) {
            SsaActorImage *im=&r->images[i];size_t start,base,bytes;unsigned x;
            if(!pointer(u32(rom+table+4u*(i+1u)),size,32,&start))goto fail;
            if(memcmp(rom+start,"AGB*",4))goto fail;
            im->rom_offset=(uint32_t)start;im->origin_x=s16(u16(rom+start+24));
            im->origin_y=s16(u16(rom+start+26));im->width=u16(rom+start+28);im->height=u16(rom+start+30);
            if(!im->width||!im->height||im->width>1024||im->height>1024||
               !range(size,start+32,(size_t)im->width*2))goto fail;
            bytes=(size_t)im->width*im->height;
            if(bytes>64u*1024u*1024u-total)goto fail;
            total+=bytes;im->pixels=(uint8_t *)malloc(bytes);if(!im->pixels)goto fail;
            base=start+32+(size_t)im->width*2;
            for(x=0;x<im->width;x++) {
                size_t pos=base+u16(rom+start+32+2u*x);unsigned y=0;
                while(y<im->height) {
                    unsigned packet,n;
                    if(!range(size,pos,1))goto fail;
                    packet=rom[pos++];n=packet&127u;
                    if(!n||n>(unsigned)im->height-y)goto fail;
                    if(packet&128u)memset(im->pixels+(size_t)x*im->height+y,0,n);
                    else {
                        if(!range(size,pos,n))goto fail;
                        memcpy(im->pixels+(size_t)x*im->height+y,rom+pos,n);pos+=n;
                    }
                    y+=n;
                }
            }
        }
    }
    next->loaded=1;ssa_actor_assets_free(a);*a=*next;free(next);return 1;
fail:
    ssa_actor_assets_free(next);free(next);return -1;
}
const SsaActorImage *ssa_actor_assets_image(const SsaActorAssets *a,unsigned r,uint32_t i) {
    if(!a||!a->loaded||r>=SSA_ACTOR_RESOURCE_COUNT||!i||i>a->resources[r].count||
       !a->resources[r].images)return NULL;
    return &a->resources[r].images[i-1u];
}
int ssa_actor_image_indices(const SsaActorImage *im,uint8_t *out,size_t stride,size_t size) {
    unsigned x,y;if(!im||!out||!im->pixels||!im->width||!im->height||stride<im->width||
        (size_t)(im->height-1u)>(SIZE_MAX-im->width)/stride||
        (size_t)(im->height-1u)*stride+im->width>size)return -1;
    for(y=0;y<im->height;y++)for(x=0;x<im->width;x++)out[(size_t)y*stride+x]=im->pixels[(size_t)x*im->height+y];
    return 1;
}
