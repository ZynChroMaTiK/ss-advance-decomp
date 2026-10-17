#include "ssa/actor_animation_runtime.h"
#include <string.h>

static int idle(const SsaActorAnimationRuntime *r) {
    const SsaScriptSpawn *s=r?r->spawn:NULL;
    return r && r->fast_converted<=1 && !r->updating && s && !s->updating && s->world && s->level &&
        s->level->map && s->world->cells && s->world->sectors && !s->world->query_active &&
        !s->level->updating && !s->level->script.updating && !s->level->movers.updating &&
        !s->level->switches.updating && s->world->collision.map==s->level->map &&
        r->animations && r->animations->loaded==1 && s->reserved==r->reserved_count &&
        s->reserved<=100 && s->pool.active_count<=100 &&
        s->pool.active_count==s->world->entity_count;
}
static int project(const SsaActorAnimationRuntime *r,SsaActorAnimation out[100],uint16_t flags[100]) {
    SsaScriptSpawn *s=r->spawn;unsigned i,j,slot;SsaEntity *seen[100];
    memset(out,0,100*sizeof(*out));memset(flags,0,100*sizeof(*flags));
    for(i=0;i<s->pool.active_count;++i) {
        slot=s->pool.active_slots[i];
        if(slot>=100 || !s->pool.occupied[slot] || s->pool.positions[slot]!=i)return -1;
        if(!s->actors[slot].owned && slot<s->reserved) {
            if(!r->reserved[slot] || !r->states[slot])return -1;
            seen[i]=r->reserved[slot];out[slot]=*r->states[slot];
        } else {
            SsaScriptActor *a=&s->actors[slot];
            if(!a->owned || a->activation.updating)return -1;
            seen[i]=&a->entity;
            out[slot]=(SsaActorAnimation){a->animation,a->animation_display_frame,
                a->animation_frame,a->animation_counter,a->animation_ticks,a->animation_flags};
        }
        if(seen[i]->owner!=s->world)return -1;
        for(j=0;j<i;++j)if(seen[j]==seen[i])return -1;
        flags[slot]=seen[i]->body.flags;
    }
    return 0;
}
int ssa_actor_animation_runtime_init(SsaActorAnimationRuntime *r,SsaScriptSpawn *s,
    const SsaActorAnimations *data,SsaEntity *const *reserved,SsaActorAnimation *const *states,size_t count) {
    SsaActorAnimation projected[100];uint16_t flags[100];unsigned i,j;
    if(!r || r->spawn || r->updating || !s || count!=s->reserved || count>100 ||
       (count && (!reserved || !states)))return -1;
    for(i=0;i<count;++i) {
        if(!reserved[i] || (!s->actors[i].owned && !states[i]) ||
           (s->actors[i].owned && reserved[i]!=&s->actors[i].entity))return -1;
        for(j=0;j<i;++j)if((states[i] && states[j]==states[i]) || reserved[j]==reserved[i])return -1;
    }
    memset(r,0,sizeof(*r));r->spawn=s;r->animations=data;r->reserved_count=(unsigned)count;
    for(i=0;i<count;++i) {r->reserved[i]=reserved[i];r->states[i]=states[i];}
    if(!idle(r) || project(r,projected,flags)<0) {memset(r,0,sizeof(*r));return -1;}
    return 0;
}
int ssa_actor_animation_runtime_step(SsaActorAnimationRuntime *r,unsigned ticks) {
    SsaActorAnimation states[100];uint16_t flags[100];SsaScriptSpawn *s;unsigned i,slot;int result;
    if(!idle(r) || ticks>255 || project(r,states,flags)<0)return -1;
    s=r->spawn;r->updating=s->updating=1;
    result=ssa_actor_animation_pool_step(states,flags,&s->pool,r->animations,ticks);
    if(result>0 && r->fast_converted && ticks)for(i=0;i<s->pool.active_count;++i){
        SsaScriptActor *a;slot=s->pool.active_slots[i];a=&s->actors[slot];
        if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd6 &&
           (flags[slot]&0x400) && !(flags[slot]&0x20) && a->activation.health>0 &&
           ssa_actor_animation_step(&states[slot],r->animations,flags[slot],ticks)<0){result=-1;break;}
    }
    if(result>0)for(i=0;i<s->pool.active_count;++i) {
        slot=s->pool.active_slots[i];
        if(!s->actors[slot].owned && slot<s->reserved)*r->states[slot]=states[slot];
        else {
            SsaScriptActor *a=&s->actors[slot];SsaActorAnimation *p=&states[slot];
            a->animation=p->animation;a->animation_display_frame=p->frame;
            a->animation_frame=p->index;a->animation_counter=p->elapsed;
            a->animation_ticks=p->period;a->animation_flags=p->done;
        }
    }
    s->updating=r->updating=0;return result<0?-1:0;
}
int ssa_actor_animation_runtime_free(SsaActorAnimationRuntime *r) {
    if(!idle(r))return -1;
    memset(r,0,sizeof(*r));return 0;
}
