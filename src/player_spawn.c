#include "ssa/player_spawn.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) { unsigned v=u16(p); return (int16_t)(v<32768?(int)v:(int)v-65536); }

int ssa_players_spawn(SsaPlayerSpawn p[4],SsaWorld *w,const SsaSpawnPlan *plan,
                       unsigned count,unsigned local) {
    const SsaMapAssets *m;
    const uint8_t *start=NULL;
    unsigned i;
    size_t needed;
    if (!p || !w || w->query_active || !plan || !(m=plan->map) || !m->data ||
        w->collision.map!=m || !count || count>4 || local>=count ||
        m->things>m->size || m->thing_count>(m->size-m->things)/12 ||
        w->entity_count>w->capacity) return 0;
    for (i=0;i<4;++i) if (p[i].entity.owner) return 0;
    if (count>1) {
        if (!plan->player_starts || plan->player_count<count) return 0;
        needed=count;
    } else {
        for (i=0;i<m->thing_count;++i) {
            const uint8_t *t=m->data+m->things+12*i;
            if (u16(t+6)==1) { start=t; break; }
        }
        needed=start?1:0;
    }
    if (needed>w->capacity-w->entity_count) return 0;
    memset(p,0,4*sizeof(*p));
    for (i=0;i<needed;++i) {
        SsaPlayerSpawn *s=&p[i]; SsaEntity *e=&s->entity;
        if (count>1) {
            const SsaSpawnMarker *t=&plan->player_starts[i];
            s->motion.x=(int32_t)t->x*65536; s->motion.y=(int32_t)t->y*65536; s->motion.z=(int32_t)t->z*65536;
            s->motion.movement.heading=(uint16_t)((uint32_t)t->heading<<8);
        } else {
            s->motion.x=(int32_t)s16(start)*65536; s->motion.y=(int32_t)s16(start+2)*65536;
            s->motion.movement.heading=u16(start+4);
        }
        s->vertical.flags=4; s->motion.movement.flags=4; s->vertical.eye_offset=96*65536;
        e->type=(uint16_t)(0xc9+i); e->player_index=(uint8_t)i;
        e->radius=32*65536; e->body.height=100*65536;
        e->body.flags=(uint16_t)(i==local?0x9a:0x1a); s->entity_health=100;
        e->body.x=s->motion.x; e->body.y=s->motion.y; e->body.z=s->motion.z;
        s->motion.facing=(uint8_t)(s->motion.movement.heading>>8); s->animation=0x65; s->animation_ticks=4;
        if (!ssa_world_insert(w,e)) {
            while (i) { --i; ssa_world_remove(w,&p[i].entity); }
            memset(p,0,4*sizeof(*p)); return 0;
        }
        e->body.z=(int32_t)e->body.floor*65536; s->motion.z=e->body.z; s->active=1;
    }
    return 1;
}
