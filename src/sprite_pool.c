#include "ssa/sprite_pool.h"
#include <string.h>

static int bindings(const SsaSpriteDispatch *d)
{
    unsigned i,j;
    if(!d)return 0;
    for(i=0;i<SSA_SPRITE_CAPACITY;++i){
        if(!d->objects[i])return 0;
        for(j=0;j<i;++j)if(d->objects[i]==d->objects[j])return 0;
    }
    return 1;
}
static int valid(const SsaSpritePool *p)
{
    unsigned i,c;uint8_t seen[SSA_SPRITE_CAPACITY]={0};const SsaSpriteDispatch *d;
    if(!p || p->loaded!=1 || p->updating || !bindings(p->dispatch))return 0;
    d=p->dispatch;c=d->active_count;
    if(c>SSA_SPRITE_CAPACITY || p->free_count!=SSA_SPRITE_CAPACITY-c)return 0;
    for(i=0;i<c;++i){unsigned slot=d->active[i];
        if(slot>=SSA_SPRITE_CAPACITY || seen[slot] || d->objects[slot]->slot!=slot ||
           d->objects[slot]->active_index!=i)return 0;
        seen[slot]=1;
    }
    for(i=c;i<SSA_SPRITE_CAPACITY;++i){unsigned slot=p->free_slots[i];
        if(slot>=SSA_SPRITE_CAPACITY || seen[slot])return 0;
        seen[slot]=1;
    }
    return 1;
}
static void reset(SsaSpritePool *p)
{
    unsigned i;SsaSpriteDispatch *d=p->dispatch;
    d->counter=0;
    for(i=0;i<SSA_SPRITE_CAPACITY;++i){ssa_weapon_object_init(d->objects[i]);p->free_slots[i]=i;}
    p->free_count=SSA_SPRITE_CAPACITY;d->active_count=0;d->submitted_count=0;
}
int ssa_sprite_pool_init(SsaSpritePool *p,SsaSpriteDispatch *d)
{
    if(!p || p->loaded || p->updating || p->dispatch || !bindings(d) || d->updating)return -1;
    memset(p,0,sizeof(*p));p->dispatch=d;p->loaded=1;reset(p);return 1;
}
int ssa_sprite_pool_reset(SsaSpritePool *p)
{
    if(!valid(p) || p->dispatch->updating)return -1;
    p->updating=1;reset(p);p->updating=0;return 1;
}
int ssa_sprite_pool_allocate(SsaSpritePool *p,SsaWeaponObject **out)
{
    unsigned slot,index;SsaSpriteDispatch *d;SsaWeaponObject *o;
    if(!out || !valid(p))return -1;
    *out=NULL;if(!p->free_count)return 0;
    p->updating=1;d=p->dispatch;index=d->active_count;
    slot=p->free_slots[SSA_SPRITE_CAPACITY-p->free_count--];o=d->objects[slot];
    o->slot=(uint16_t)slot;d->active[index]=slot;o->active_index=(uint16_t)index;
    d->active_count=index+1;*out=o;p->updating=0;return 1;
}
static void release(SsaSpritePool *p,SsaWeaponObject *o)
{
    SsaSpriteDispatch *d=p->dispatch;unsigned index=o->active_index,last=d->active_count-1;
    p->free_slots[SSA_SPRITE_CAPACITY-++p->free_count]=o->slot;d->active_count=last;
    if(index!=last){unsigned slot=d->active[last];d->active[index]=slot;d->objects[slot]->active_index=(uint16_t)index;}
    ssa_weapon_object_init(o);
}
int ssa_sprite_pool_release(SsaSpritePool *p,SsaWeaponObject *o)
{
    unsigned slot;
    if(!valid(p) || !o)return -1;
    for(slot=0;slot<SSA_SPRITE_CAPACITY && p->dispatch->objects[slot]!=o;++slot){}
    if(slot==SSA_SPRITE_CAPACITY || o->slot!=slot || o->active_index>=p->dispatch->active_count ||
       p->dispatch->active[o->active_index]!=slot)return -1;
    p->updating=1;release(p,o);p->updating=0;return 1;
}
int ssa_sprite_pool_mark(SsaSpritePool *p)
{
    unsigned i;if(!valid(p))return -1;p->updating=1;
    for(i=0;i<p->dispatch->active_count;++i)p->dispatch->objects[p->dispatch->active[i]]->flags|=UINT32_C(0x04000000);
    p->updating=0;return 1;
}
int ssa_sprite_pool_remove_types(SsaSpritePool *p,uint16_t first,uint16_t last)
{
    unsigned i,cursor=0,count;SsaSpriteDispatch *d;
    if(!valid(p))return -1;
    p->updating=1;d=p->dispatch;count=d->active_count;
    for(i=0;i<count;++i){SsaWeaponObject *o=d->objects[d->active[cursor]];
        if(o->type>=first && o->type<=last)release(p,o);else ++cursor;
    }
    p->updating=0;return 1;
}
