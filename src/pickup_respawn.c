#include "ssa/pickup_respawn.h"
#include <string.h>
#include <limits.h>
static int16_t half(uint16_t n) {return n<=INT16_MAX?(int16_t)n:(int16_t)(-1-(int32_t)(UINT16_MAX-n));}
int ssa_pickup_respawn_data_load(SsaPickupRespawnData *out,const void *data,size_t size) {
    const uint8_t *rom=data;SsaPickupRespawnData next;unsigned row,i;
    if(!out || !rom || size<0x44d314) return -1;
    memset(&next,0,sizeof(next));
    for(row=0;row<4;++row)for(i=0;i<34;++i) {
        size_t at=0x44d204+row*68+i*2;
        next.schedule[row][i]=(uint16_t)(rom[at]|(unsigned)rom[at+1]<<8);
    }
    next.loaded=1;*out=next;return 1;
}
int ssa_pickup_respawn_occupied(const SsaSpawnMarker *m,SsaEntity *const *entities,size_t count) {
    size_t i;if(!m || (count && !entities) || count>100) return -1;
    for(i=0;i<count;++i) {
        if(!entities[i]) return -1;
        if((uint16_t)((uint32_t)entities[i]->body.x>>16)==(uint16_t)m->x &&
           (uint16_t)((uint32_t)entities[i]->body.y>>16)==(uint16_t)m->y) return 1;
    }
    return 0;
}
int ssa_pickup_respawn_step(SsaPickupRespawn *s,const SsaPickupRespawnData *data,
    unsigned players,unsigned level,unsigned ticks,const SsaSpawnMarker *markers,
    size_t count,int32_t fallback_z,const SsaPickupRespawnOps *ops) {
    const uint16_t *table;const SsaSpawnMarker *marker;SsaLevelScriptEntity *e=NULL;
    uint16_t animation;int occupied,result=-1;int32_t x,y,z=fallback_z;
    if(!s || s->updating || players>255 || ticks>255 || level>65535) return -1;
    if(players==1 || level<12 || level>15) return 0;
    if(!data || !data->loaded || !markers || !count || count>65535 || s->marker>=count ||
       s->sequence>30 || (s->sequence&1) || !ops || !ops->occupied || !ops->allocate ||
       !ops->animation || !ops->insert || !ops->particle) return -1;
    table=data->schedule[level-12];s->updating=1;
    s->elapsed=(uint16_t)(s->elapsed+ticks);
    if(s->elapsed<=table[s->sequence]) {result=0;goto done;}
    marker=&markers[s->marker];s->marker=(uint16_t)(s->marker+1u);
    if(s->marker>=count)s->marker=0;
    animation=table[s->sequence+1];s->sequence=(uint16_t)(s->sequence+2u);
    if(!table[s->sequence])s->sequence=0;
    if(ops->occupied(ops->context,marker,&occupied)<0 || (occupied!=0 && occupied!=1))goto done;
    if(!occupied) {
        if(ops->allocate(ops->context,&e)<0)goto done;
        x=(int32_t)marker->x*65536;y=(int32_t)marker->y*65536;
        if(e) {
            e->x=x;e->y=y;e->type=0xe1;e->variant=0;e->facing=(uint8_t)marker->heading;
            e->trigger=0;e->timer=60;e->flags|=0x80;
            if(ops->animation(ops->context,e,animation,6)<0 || ops->insert(ops->context,e)<0)goto done;
            e->z=(int32_t)half(e->floor)*65536;z=e->z;
        }
        if(ops->particle(ops->context,5,x,y,z)<0)goto done;
    }
    s->elapsed=0;result=0;
done:
    s->updating=0;return result;
}
