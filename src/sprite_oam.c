#include "ssa/sprite_oam.h"
#include <string.h>

static int32_t s32(uint32_t v) { return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v; }
static int16_t s16(uint16_t v) { return v<32768u?(int16_t)v:(int16_t)((int32_t)v-65536); }
static int32_t shift(uint32_t v,unsigned n) {
    return s32((v>>n)|((v&0x80000000u)?~(UINT32_MAX>>n):0u));
}
static int32_t fixed(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;
    if(p<0&&p%65536)--q;
    return s32((uint32_t)q);
}
static int enabled(const SsaSpriteOam *o,const SsaWeaponObject *p) {
    return (p->flags&1u)&&p->animation!=65535u&&(p->previous_frame&0x3fffu)!=0x3fffu&&
        (!p->owner||(unsigned)p->owner-1u==o->local_index)&&(p->flags&4u)&&
        !((o->counter&4u)&&(p->flags&2u));
}
int ssa_sprite_oam_init(SsaSpriteOam *o,const void *source,size_t size) {
    SsaSpriteOam n;unsigned i;
    if(!o||o->updating||!source||size<0x44bbfcu)return -1;
    memset(&n,0,sizeof(n));memcpy(n.layer_bucket,(const uint8_t *)source+0x44bbec,16);
    for(i=0;i<16;i++)if(n.layer_bucket[i]>15)return -1;
    n.loaded=1;*o=n;return 1;
}
int ssa_sprite_oam_build(SsaSpriteOam *o,SsaSpriteDispatch *d,const SsaSpriteAssets *assets,
    const SsaSpriteCacheEntry cache[SSA_SPRITE_IMAGE_COUNT],const SsaMathTables *math) {
    uint8_t seen[SSA_SPRITE_CAPACITY]={0};unsigned i,matrix_cursor=0;uint32_t ref;
    if(!o||o->loaded!=1||o->updating||!d||d->updating||d->active_count>SSA_SPRITE_CAPACITY||
       !assets||assets->loaded!=1||!cache||!math)return -1;
    for(i=0;i<16;i++)if(o->layer_bucket[i]>15)return -1;
    for(i=0;i<d->active_count;i++) {
        unsigned slot=d->active[i],j,id;SsaWeaponObject *p;const SsaSpriteImage *im;
        if(slot>=SSA_SPRITE_CAPACITY||seen[slot]||!d->objects[slot])return -1;
        seen[slot]=1;p=d->objects[slot];if(p->layer>15)return -1;
        for(j=0;j<i;j++)if(d->objects[d->active[j]]==p)return -1;
        if(!enabled(o,p))continue;
        id=p->previous_frame&0x3fffu;if(id>=SSA_SPRITE_IMAGE_COUNT)return -1;
        if(!cache[id].resident)continue;
        im=&assets->images[id];
        if(!im->parts||im->part_count>128)return -1;
        for(j=0;j<im->part_count;j++)if(!im->parts[j].width||!im->parts[j].height)return -1;
    }
    o->updating=1;o->flags|=1u;o->transfer_flag=0;o->count=0;o->affine_count=0;
    for(i=0;i<128;i++) {
        o->entries[i].attribute0=0x200;o->entries[i].attribute1=0;o->entries[i].attribute2=0;
        o->entries[i].affine=(uint16_t)((i%4==0||i%4==3)?256:0);
    }
    o->mosaic_register=(uint16_t)(((uint32_t)o->mosaic<<12)|((uint32_t)o->mosaic<<8));
    for(i=0;i<16;i++)o->layer_heads[i]=i+2u;
    o->layer_heads[16]=0;
    for(i=0;i<d->active_count;i++) {
        unsigned slot=d->active[i];SsaWeaponObject *p=d->objects[slot];unsigned bucket=o->layer_bucket[p->layer];
        p->link=o->layer_heads[bucket];o->layer_heads[bucket]=0x80000000u|(slot+1u);
    }
    ref=o->layer_heads[0];
    while(ref&&o->count<128) {
        SsaWeaponObject *p;const SsaSpriteImage *im;unsigned id,part,affine=0,scaled,doubled;
        int32_t sx,sy;uint16_t frame;
        if(!(ref&0x80000000u)){ref=o->layer_heads[ref-1u];continue;}
        p=d->objects[(ref&0x7fffffffu)-1u];ref=p->link;
        p->screen_x=s16((uint16_t)shift((uint32_t)p->x,8));
        p->screen_y=s16((uint16_t)shift((uint32_t)p->y,8));
        if(!enabled(o,p))continue;
        sx=s16(p->scale_x);sy=s16(p->scale_y);if(!sx)sx=4096;if(!sy)sy=4096;
        scaled=(unsigned)(sx!=4096||sy!=4096);doubled=(unsigned)(scaled&&(sx>4096||sy>4096));
        frame=p->previous_frame;id=frame&0x3fffu;if(!cache[id].resident)continue;
        im=&assets->images[id];
        for(part=0;part<im->part_count&&o->count<128;part++) {
            const SsaSpritePart *piece=&im->parts[part];int32_t x,y,py,rotation=s16(p->unknown2c);
            uint16_t a0,a1,a2;unsigned half;
            y=piece->attribute0&511u;if(y&256)y-=512;
            if(frame&0x4000u)y=-y-piece->height;
            half=piece->height>>1;
            if(sy!=4096)y=shift((uint32_t)(y+(int32_t)half)*(uint32_t)sy,12)-(int32_t)half;
            if(doubled)y-=(int32_t)half;
            py=p->screen_y+y;if((uint32_t)(py+64)>224u)continue;
            x=piece->attribute1&511u;if(x&256)x-=512;
            if(frame&0x8000u)x=-x-piece->width;
            half=piece->width>>1;
            if(sx!=4096)x=shift((uint32_t)(x+(int32_t)half)*(uint32_t)sx,12)-(int32_t)half;
            if(doubled)x-=(int32_t)half;
            a0=(uint16_t)((piece->attribute0&0xfe00u)|((uint32_t)py&255u)|0x2000u);
            a1=(uint16_t)((piece->attribute1&0xfe00u)|((uint32_t)(p->screen_x+x)&511u));
            a2=(uint16_t)((cache[id].tile+piece->tile)*2u);
            if(p->layer==0)a2|=0xc00u;else if(p->layer==1)a2|=0x800u;else if(p->layer<=14)a2|=0x400u;
            if(o->affine_count<32&&(scaled||rotation)) {
                affine=1;a0|=0x100u;a1|=(uint16_t)(o->affine_count<<9);if(doubled)a0|=0x200u;
                if(part==0) {
                    int32_t ax,ay,pa,pb,pc,pd;
                    if(scaled) { ax=16777216/((frame&0x8000u)?-sx:sx);ay=16777216/((frame&0x4000u)?-sy:sy); }
                    else { ax=(frame&0x8000u)?-4096:4096;ay=(frame&0x4000u)?-4096:4096; }
                    if(rotation) {
                        int32_t sine,cosine;ssa_sincos(math,(uint16_t)rotation,&sine,&cosine);
                        pa=fixed(cosine,ax);pb=fixed(sine,ax);pc=fixed(-sine,ay);pd=fixed(cosine,ay);
                    } else { pa=ax;pb=0;pc=0;pd=ay; }
                    o->entries[matrix_cursor*4].affine=(uint16_t)shift((uint32_t)pa,4);
                    o->entries[matrix_cursor*4+1].affine=(uint16_t)shift((uint32_t)pb,4);
                    o->entries[matrix_cursor*4+2].affine=(uint16_t)shift((uint32_t)pc,4);
                    o->entries[matrix_cursor*4+3].affine=(uint16_t)shift((uint32_t)pd,4);matrix_cursor++;
                }
            }
            if(!affine) { if(frame&0x8000u)a1|=0x1000u;if(frame&0x4000u)a1|=0x2000u; }
            if(p->flags&0x04000000u)a0|=0x400u;
            o->entries[o->count].attribute0=a0;o->entries[o->count].attribute1=a1;o->entries[o->count].attribute2=a2;o->count++;
        }
        if(affine)o->affine_count++;
    }
    o->flags&=254u;o->updating=0;return 1;
}
