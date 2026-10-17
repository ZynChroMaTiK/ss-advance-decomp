#include "ssa/spawn.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) { unsigned n=u16(p); return (int16_t)(n<32768?(int)n:(int)n-65536); }

int ssa_spawn_definitions_load(SsaSpawnDefinitions *out,const uint8_t *rom,size_t size) {
    unsigned i;
    if (!out || !rom || size<0x44a5d4) return 0;
    for (i=0;i<64;++i) {
        const uint8_t *p=rom+0x44a3d4+8*i;
        out->thing[i].type=u16(p); out->thing[i].variant=u16(p+2);
        out->thing[i].animation=u16(p+4); out->thing[i].animation_ticks=u16(p+6);
    }
    return 1;
}

int ssa_spawn_plan_free(SsaSpawnPlan *p) {
    size_t i;
    if (!p) return 0;
    for (i=0;i<p->object_count;++i) if (p->objects[i].entity.owner) return 0;
    free(p->objects); free(p->player_starts); free(p->teleport_starts);
    memset(p,0,sizeof(*p)); return 1;
}

int ssa_spawn_plan_load(SsaSpawnPlan *out,const SsaMapAssets *m,const SsaSpawnDefinitions *defs,
                         unsigned level,unsigned players,size_t limit) {
    SsaSpawnPlan p={0}; unsigned type; size_t i;
    if (!out || out->map || out->objects || out->player_starts || out->teleport_starts ||
        !m || !m->data || !defs || level>15 || !players || players>4 || limit>100 ||
        m->things>m->size || m->thing_count>(m->size-m->things)/12) return 0;
    p.map=m;
    p.objects=calloc(limit?limit:1,sizeof(*p.objects));
    p.player_starts=calloc(m->thing_count?m->thing_count:1,sizeof(*p.player_starts));
    p.teleport_starts=calloc(m->thing_count?m->thing_count:1,sizeof(*p.teleport_starts));
    if (!p.objects || !p.player_starts || !p.teleport_starts) goto fail;
    for (type=0;type<64;++type) {
        const SsaSpawnDefinition *d=&defs->thing[type];
        if (d->type==0xc9 && players==1) continue;
        for (i=0;i<m->thing_count;++i) {
            const uint8_t *t=m->data+m->things+12*i;
            SsaSpawnObject *o; SsaSpawnMarker *marker=NULL; int sector;
            if (u16(t+6)!=type) continue;
            if (d->type>=0xc9 && d->type<=0xcc) marker=&p.player_starts[p.player_count++];
            else if (d->type==0xd9) marker=&p.teleport_starts[p.teleport_count++];
            if (marker) {
                marker->x=s16(t); marker->y=s16(t+2); marker->heading=u16(t+4)>>8;
                continue;
            }
            if (p.object_count==limit) { ++p.dropped_count; continue; }
            o=&p.objects[p.object_count++]; o->thing_index=(uint16_t)i;
            o->entity.body.x=(int32_t)s16(t)*65536; o->entity.body.y=(int32_t)s16(t+2)*65536;
            if (level==10 && (s16(t)==2240 || s16(t)==3008) && s16(t+2)==-2304)
                o->entity.body.y=-2108*65536;
            o->entity.type=d->type; o->variant=(uint8_t)d->variant;
            o->facing=(uint8_t)((u16(t+4)>>8)+128); o->trigger=u16(t+8);
            if (d->animation<=0x8d || d->animation==0xffff) {
                o->animation=d->animation; o->animation_ticks=(uint8_t)d->animation_ticks;
            }
            sector=ssa_map_sector(m,o->entity.body.x,o->entity.body.y);
            if (sector<0 || m->sectors>m->size || (size_t)(sector+1)>(m->size-m->sectors)/20) goto fail;
            o->entity.body.floor=s16(m->data+m->sectors+20*(size_t)sector);
            o->entity.body.ceiling=s16(m->data+m->sectors+20*(size_t)sector+2);
            o->entity.body.z=(int32_t)o->entity.body.floor*65536;
            o->entity.leaf=o->entity.sector=-1; o->entity.cell=SIZE_MAX;
        }
    }
    *out=p; return 1;
fail:
    ssa_spawn_plan_free(&p); return 0;
}

int ssa_spawn_plan_insert(SsaSpawnPlan *p,SsaWorld *w) {
    size_t i;
    if (!p || !w || !p->map || w->collision.map!=p->map || w->entity_count>w->capacity ||
        p->object_count>w->capacity-w->entity_count) return 0;
    for (i=0;i<p->object_count;++i) if (p->objects[i].entity.owner) return 0;
    for (i=0;i<p->object_count;++i) {
        if (!ssa_world_insert(w,&p->objects[i].entity)) {
            while (i) { --i; ssa_world_remove(w,&p->objects[i].entity); }
            return 0;
        }
        p->objects[i].entity.body.z=(int32_t)p->objects[i].entity.body.floor*65536;
    }
    return 1;
}
