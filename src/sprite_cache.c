#include "ssa/sprite_cache.h"
#include <string.h>
#include <limits.h>

static int16_t half(int n)
{uint16_t u=(uint16_t)n;return u<=INT16_MAX?(int16_t)u:(int16_t)(-1-(int)(uint16_t)~u);}
static int ready(const SsaSpriteCache *c)
{return c && c->loaded==1 && c->free_count<=50 && c->used_count<=50 &&
 c->image_count<=50 && c->read_cursor<50 && c->write_cursor<50 && c->transfer_count<=10;}
static int objects_valid(const SsaSpriteDispatch *d,const SsaSpriteAssets *a,unsigned local)
{
    unsigned i;
    if(!d || d->updating || !a || a->loaded!=1 || local>3 || d->active_count>84)return 0;
    for(i=0;i<d->active_count;++i)
        if(d->active[i]>=84 || !d->objects[d->active[i]])return 0;
    return 1;
}
static int selected(const SsaWeaponObject *o,unsigned local)
{return o->type && (!o->owner || o->owner-1u==local);}
int ssa_sprite_cache_init(SsaSpriteCache *c)
{
    if(!c || c->loaded || c->updating)return -1;
    memset(c,0,sizeof(*c));c->free_blocks[0]=(SsaSpriteTileBlock){160,512};
    c->free_count=1;c->budget=10000;c->loaded=1;return 1;
}
int ssa_sprite_cache_free(SsaSpriteCache *c)
{if(!ready(c) || c->updating)return -1;memset(c,0,sizeof(*c));return 1;}
int ssa_sprite_cache_reset(SsaSpriteCache *c,SsaSpriteDispatch *d)
{
    unsigned i;
    if(!ready(c) || c->updating || !d || d->updating || d->active_count>84)return -1;
    for(i=0;i<d->active_count;++i)if(d->active[i]>=84 || !d->objects[d->active[i]])return -1;
    c->updating=1;c->free_blocks[0]=(SsaSpriteTileBlock){160,512};
    c->free_count=1;c->used_count=c->image_count=c->read_cursor=c->write_cursor=0;
    c->allocation_failures=c->transfer_count=0;c->budget=10000;
    memset(c->used_images,0,50*sizeof(uint16_t));
    memset(c->entries,0,sizeof(c->entries));
    for(i=0;i<d->active_count;++i)d->objects[d->active[i]]->previous_frame=0;
    c->updating=0;return 1;
}
int ssa_sprite_visibility(SsaSpriteDispatch *d,const SsaSpriteAssets *a,unsigned local)
{
    unsigned i;
    if(!objects_valid(d,a,local))return -1;
    for(i=0;i<d->active_count;++i) {
        const SsaWeaponObject *o=d->objects[d->active[i]];unsigned f=o->frame&0x3fff;
        if(selected(o,local) && o->type!=2 && o->animation!=65535 && f!=16383 && f>=311)return -1;
    }
    for(i=0;i<d->active_count;++i) {
        SsaWeaponObject *o=d->objects[d->active[i]];unsigned f=o->frame&0x3fff;
        int16_t left=o->screen_x,right=left,top=o->screen_y,bottom=top;
        if(!selected(o,local) || o->type==2)continue;
        o->flags|=4;
        if(o->animation!=65535 && f!=16383) {
            const int8_t *b=a->images[f].bounds;
            int x0=b[0],x1=b[2],y0=b[1],y1=b[3],t;
            if(x0>x1){t=x0;x0=x1;x1=t;}if(y0>y1){t=y0;y0=y1;y1=t;}
            left=half(o->screen_x+((o->frame&0x8000)?-x1:x0));
            right=half(o->screen_x+((o->frame&0x8000)?-x0:x1));
            top=half(o->screen_y+((o->frame&0x4000)?-y1:y0));
            bottom=half(o->screen_y+((o->frame&0x4000)?-y0:y1));
        }
        if(right<0 || left>240 || bottom<0 || top>160)o->flags&=~UINT32_C(4);
    }
    return 1;
}
static int allocate(SsaSpriteCache *c,uint16_t count)
{
    unsigned i;uint16_t tile;
    for(i=0;i<c->free_count && c->free_blocks[i].count<count;++i){}
    if(i==c->free_count)return -1;
    if(c->used_count>=50)return -1;
    tile=c->free_blocks[i].start;
    c->used_blocks[c->used_count++]=(SsaSpriteTileBlock){tile,count};
    if(c->free_blocks[i].count!=count) {
        c->free_blocks[i].count=(uint16_t)(c->free_blocks[i].count-count);
        c->free_blocks[i].start=(uint16_t)(tile+count);
    } else {
        unsigned j;for(j=i;j+1<c->free_count;++j)c->free_blocks[j]=c->free_blocks[j+1];
        --c->free_count;
    }
    return tile;
}
static int release(SsaSpriteCache *c,uint16_t tile)
{
    unsigned i,j,k,remaining;uint16_t count;
    for(i=0;i<c->used_count && c->used_blocks[i].start!=tile;++i){}
    if(i==c->used_count)return 1;
    if(c->free_count>=50)return -1;
    count=c->used_blocks[i].count;
    /* E2B0 copies through count inclusive, retaining its two stale records. */
    for(j=i;j<=c->used_count;++j)c->used_blocks[j]=c->used_blocks[j+1];
    --c->used_count;
    for(i=0;i<c->free_count && c->free_blocks[i].start<=tile;++i){}
    for(j=c->free_count;j>i;--j)c->free_blocks[j]=c->free_blocks[j-1];
    ++c->free_count;c->free_blocks[i]=(SsaSpriteTileBlock){tile,count};
    /* E1CC's cursor/remaining behavior intentionally differs from a generic
     * sorted-interval merge. Preserve the copied stale tail and rescan order. */
    i=0;remaining=c->free_count;
    while(remaining>1) {
        if((uint16_t)(c->free_blocks[i].start+c->free_blocks[i].count)==c->free_blocks[i+1].start) {
            c->free_blocks[i+1].start=c->free_blocks[i].start;
            c->free_blocks[i+1].count=(uint16_t)(c->free_blocks[i].count+c->free_blocks[i+1].count);
            for(k=0;k<remaining;++k)c->free_blocks[i+k]=c->free_blocks[i+k+1];
            if(remaining!=c->free_count)--i;
            else {remaining-=2;++i;}
            --c->free_count;
        } else {++i;--remaining;}
    }
    return 1;
}
int ssa_sprite_cache_allocate(SsaSpriteCache *c,uint16_t count)
{return !ready(c) || c->updating?-1:allocate(c,count);}
int ssa_sprite_cache_release(SsaSpriteCache *c,uint16_t tile)
{return !ready(c) || c->updating?-1:release(c,tile);}
static int transfer(SsaSpriteCache *c,const SsaSpriteAssets *a,unsigned image,uint16_t tile)
{
    unsigned count;
    if(image>=311 || c->transfer_count>=10)return -1;
    count=a->images[image].tile_count;
    if(!count || !a->images[image].tiles)return -1;
    c->transfers[c->transfer_count++]=(SsaSpriteTransfer){(uint16_t)image,tile,(uint16_t)count,0};
    c->budget-=(uint32_t)count*64;return 1;
}
static int request(SsaSpriteCache *c,SsaSpriteDispatch *d,const SsaSpriteAssets *a,unsigned local)
{
    unsigned i;
    for(i=0;i<d->active_count;++i) {
        SsaWeaponObject *o=d->objects[d->active[i]];unsigned f=o->frame&0x3fff;
        SsaSpriteCacheEntry *e;
        if(!selected(o,local))continue;
        if(o->type==2) {
            if(f>=311)return -1;
            c->entries[f].resident=3;
            if(o->previous_frame!=o->frame) {
                o->previous_frame=o->frame;o->flags|=4;
                if(transfer(c,a,f,0)<0)return -1;
            }
            continue;
        }
        if(!(o->flags&4) || f==16383)continue;
        if(f>=311)return -1;
        e=&c->entries[f];
        if(e->resident) {o->previous_frame=o->frame;e->resident=3;}
        else if(!e->pending) {
            e->pending=1;c->requests[c->write_cursor]=(uint16_t)f;
            c->write_cursor=(uint16_t)((c->write_cursor+1)%50);
        }
        f=o->previous_frame&0x3fff;
        /* FAE8's invalid previous-frame sentinel has no native cache entry.
         * The original retention probe reads beyond its allocated table here.
         * Treat it as no retained image rather than touching unrelated memory. */
        if(f==16383)continue;
        if(f>=311)return -1;
        if(c->entries[f].resident==1)c->entries[f].resident=2;
    }
    return 1;
}
static int age(SsaSpriteCache *c)
{
    unsigned i=0,remaining=c->image_count,j;
    while(remaining) {
        unsigned f=c->used_images[i];SsaSpriteCacheEntry *e;
        if(f>=311)return -1;
        e=&c->entries[f];--e->resident;
        if(!e->resident) {
            if(release(c,e->tile)<0)return -1;
            e->tile=0;--c->image_count;
            if(remaining!=1) {
                for(j=0;j<remaining;++j)c->used_images[i+j]=c->used_images[i+j+1];
                --remaining;
            }
        }
        ++i;--remaining;
    }
    return 1;
}
static int process(SsaSpriteCache *c,const SsaSpriteAssets *a)
{
    unsigned i;c->budget=10000;
    for(i=0;i<8;++i) {
        unsigned f,count;int tile;SsaSpriteCacheEntry *e;
        if(c->read_cursor==c->write_cursor)continue;
        f=c->requests[c->read_cursor]&0x3fff;
        c->read_cursor=(uint16_t)((c->read_cursor+1)%50);
        if(f>=311)return -1;
        e=&c->entries[f];e->pending=0;
        count=a->images[f].tile_count;
        if(c->budget<(uint32_t)count*64) {e->resident=0;continue;}
        if(c->image_count>=50)return -1;
        tile=allocate(c,(uint16_t)count);
        if(tile<0) {++c->allocation_failures;e->resident=0;continue;}
        c->used_images[c->image_count++]=(uint16_t)f;e->resident=3;e->tile=(uint16_t)tile;
        if(transfer(c,a,f,(uint16_t)tile)<0)return -1;
    }
    return 1;
}
int ssa_sprite_cache_prepare(SsaSpriteCache *c,SsaSpriteDispatch *d,const SsaSpriteAssets *a,unsigned local)
{
    int result=-1;
    if(!ready(c) || c->updating || !objects_valid(d,a,local))return -1;
    c->updating=1;
    if(ssa_sprite_visibility(d,a,local)<0 || request(c,d,a,local)<0 || age(c)<0 || process(c,a)<0)goto done;
    result=1;
done:c->updating=0;return result;
}
int ssa_sprite_cache_flush(SsaSpriteCache *c,const SsaSpriteAssets *a)
{
    unsigned i,j;int result=-1;
    if(!ready(c) || c->updating || !a || a->loaded!=1)return -1;
    c->updating=1;
    for(i=0;i<c->transfer_count;++i) {
        SsaSpriteTransfer *t=&c->transfers[i];const SsaSpriteImage *im;
        unsigned count=t->count>160?160:t->count;
        if(t->image>=311)goto done;
        im=&a->images[t->image];
        if(!im->tiles || count>im->tile_count)goto done;
        for(j=0;j<count*64;++j)c->tiles[((unsigned)t->destination*64+j)&32767]=im->tiles[j];
        t->count=0;
    }
    c->transfer_count=0;result=1;
done:c->updating=0;return result;
}
