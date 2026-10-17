#include "ssa/texture_assets.h"
#include <stdlib.h>
#include <string.h>

static uint16_t get16(const uint8_t *p)
{return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);}
static uint32_t get32(const uint8_t *p)
{return (uint32_t)get16(p)|(uint32_t)get16(p+2)<<16;}
static int span(size_t size,size_t at,size_t length)
{return at<=size && length<=size-at;}
static unsigned fold(unsigned c)
{return c>='A' && c<='Z'?c+32u:c;}
static int equal_name(const char *a,const char *b)
{
    unsigned i;
    for(i=0;i<8;++i) {
        unsigned x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(fold(x)!=fold(y))return 0;
        if(!x || !y)return 1;
    }
    return 1;
}
int ssa_texture_archive_find(const SsaTextureArchive *a,const char name[8])
{
    unsigned i;
    if(!a || !name || !a->images)return -1;
    for(i=0;i<a->count;++i)if(equal_name(a->images[i].name,name))return (int)i;
    return -1;
}
void ssa_texture_assets_free(SsaTextureAssets *a)
{
    unsigned f,m,i;
    if(!a)return;
    for(f=0;f<2;++f) {
        for(m=0;m<3;++m) {
            SsaTextureArchive *v=&a->families[f].archives[m];
            if(v->images)for(i=0;i<v->count;++i)free(v->images[i].pixels);
            free(v->images);
        }
        free(a->families[f].mip_indices);
    }
    memset(a,0,sizeof(*a));
}
static int archive_load(SsaTextureArchive *a,const uint8_t *rom,size_t size,
                        unsigned resource,size_t *budget)
{
    uint32_t address=get32(rom+0x44AEF4u+resource*4u),records;
    size_t off;
    unsigned i;
    if(address<UINT32_C(0x08000000))return 0;
    off=(size_t)(address-UINT32_C(0x08000000));
    if(!span(size,off,72) || memcmp(rom+off,"WOD0",4))return 0;
    a->rom_offset=(uint32_t)off;a->count=get16(rom+off+8);
    records=get32(rom+off+32);
    if(!a->count || a->count>1024 || records<72 ||
       !span(size-off,records,(size_t)a->count*16u))return 0;
    a->images=(SsaTextureImage *)calloc(a->count,sizeof(*a->images));
    if(!a->images)return 0;
    for(i=0;i<a->count;++i) {
        const uint8_t *r=rom+off+records+i*16u;
        SsaTextureImage *im=&a->images[i];
        uint32_t payload=get32(r+12);
        size_t bytes;
        memcpy(im->name,r,8);im->width=get16(r+8);im->height=get16(r+10);
        if(!im->width || !im->height || im->width>1024 || im->height>1024)return 0;
        bytes=(size_t)im->width*im->height;
        /* All actual WOD0 payloads precede the descriptor table. Require the
         * same bounded layout rather than accepting metadata as pixel bytes. */
        if(payload<72 || !span(records,payload,bytes) || bytes>*budget)return 0;
        im->rom_offset=(uint32_t)(off+payload);
        im->pixels=(uint8_t *)malloc(bytes);
        if(!im->pixels)return 0;
        memcpy(im->pixels,rom+off+payload,bytes);*budget-=bytes;
    }
    return 1;
}
int ssa_texture_assets_load(SsaTextureAssets *out,const void *data,size_t size)
{
    SsaTextureAssets a={0};
    const uint8_t *rom=(const uint8_t *)data;
    size_t budget=16u*1024u*1024u;
    unsigned f,m,i;
    if(!out || !rom || out->loaded || !span(size,0x44AEF4u,22u*4u))return -1;
    /* The zero-init contract also prevents replacing partial caller ownership. */
    for(f=0;f<2;++f) {
        if(out->families[f].mip_indices)return -1;
        for(m=0;m<3;++m)if(out->families[f].archives[m].images)return -1;
    }
    for(f=0;f<2;++f) {
        SsaTextureFamily *v=&a.families[f];
        for(m=0;m<3;++m)if(!archive_load(&v->archives[m],rom,size,16+f*3+m,&budget))goto fail;
        v->mip_indices=(uint16_t (*)[3])calloc(v->archives[0].count,sizeof(*v->mip_indices));
        if(!v->mip_indices)goto fail;
        for(i=0;i<v->archives[0].count;++i) {
            v->mip_indices[i][0]=(uint16_t)i;
            for(m=1;m<3;++m) {
                int found=ssa_texture_archive_find(&v->archives[m],v->archives[0].images[i].name);
                v->mip_indices[i][m]=(uint16_t)(found<0?SSA_TEXTURE_MISSING:found);
            }
        }
    }
    a.loaded=1;*out=a;return 1;
fail:
    ssa_texture_assets_free(&a);return -1;
}
const SsaTextureImage *ssa_texture_assets_image(const SsaTextureAssets *a,
    unsigned family,unsigned base,unsigned mip)
{
    const SsaTextureFamily *f;
    unsigned index;
    if(!a || !a->loaded || family>=2 || mip>=3)return NULL;
    f=&a->families[family];
    if(!f->mip_indices || base>=f->archives[0].count)return NULL;
    index=f->mip_indices[base][mip];
    if(index==SSA_TEXTURE_MISSING){mip=0;index=base;}
    if(!f->archives[mip].images || index>=f->archives[mip].count)return NULL;
    return &f->archives[mip].images[index];
}
int ssa_texture_assets_family(unsigned level)
{return level>=16?-1:(level<=5 || level==12 || level==13?0:1);}
int ssa_texture_image_indices(const SsaTextureImage *im,uint8_t *dst,
    size_t stride,size_t capacity)
{
    unsigned x,y;
    if(!im || !dst || !im->pixels || !im->width || !im->height ||
       im->width>1024 || im->height>1024 || stride<im->width ||
       capacity<im->width || (size_t)(im->height-1)>(capacity-im->width)/stride)return -1;
    for(y=0;y<im->height;++y)for(x=0;x<im->width;++x)
        dst[y*stride+x]=im->pixels[(size_t)x*im->height+y];
    return 1;
}
