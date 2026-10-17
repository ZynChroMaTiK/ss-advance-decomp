#include "ssa/player_motion.h"
#include <limits.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t asr(int32_t v,unsigned n) {
    return bits(((uint32_t)v>>n)|(v<0 ? UINT32_MAX<<(32-n) : 0));
}
static uint16_t direction_distance(const SsaMathTables *tables,int16_t x,int16_t y,int32_t *distance) {
    int32_t ax=x<0 ? -(int32_t)x : x, ay=y<0 ? -(int32_t)y : y;
    int32_t sine,cosine,denominator,numerator;
    uint16_t heading=ssa_direction(tables,x,y);
    *distance=0;
    if (!ax && !ay) return heading;
    ssa_sincos(tables,heading,&sine,&cosine);
    denominator=ax>ay ? sine : cosine;
    if (denominator<0) denominator=-denominator;
    numerator=bits((uint32_t)(ax>ay ? ax : ay)<<16);
    /* The signed numerator intentionally wraps at a 32768-unit axis. */
    *distance=numerator/denominator;
    return heading;
}

int ssa_player_motion_step(SsaPlayerMotion *s,SsaEntity *entity,SsaPlayerVertical *vertical,
                            const SsaMathTables *tables,const SsaMotionFrame *frame,
                            const SsaMotionOps *ops,const SsaMotionOptions *options) {
    unsigned i;
    int push;
    uint16_t heading;
    if (!s || !entity || !vertical || !tables || !frame || !ops || !ops->horizontal ||
        frame->player_count>4) return -1;
    push=frame->player_count>1 && entity->player_index==frame->local_player;
    if (push) {
        if (!frame->players || frame->slot>=frame->player_count || frame->players[frame->slot]!=entity) return -1;
        for (i=0;i<frame->player_count;++i) if (!frame->players[i]) return -1;
        s->movement.impulse=0;
        for (i=0;i<frame->player_count;++i) if (i!=frame->slot) {
            const SsaBody *other=&frame->players[i]->body;
            int32_t distance;
            uint16_t angle=direction_distance(tables,
                (int16_t)asr(sub(entity->body.x,other->x),16),
                (int16_t)asr(sub(entity->body.y,other->y),16),&distance);
            if (distance<80) {
                s->movement.impulse_heading=angle;
                s->movement.impulse=asr(bits((uint32_t)sub(80,distance)<<16),2);
            }
        }
    }
    ssa_movement_controls(&s->movement,frame->ticks);
    ssa_movement_vector(&s->movement,tables);
    heading=s->movement.heading;
    entity->body.x=s->x; entity->body.y=s->y; entity->body.z=s->z;
    entity->body.velocity_x=s->movement.velocity_x; entity->body.velocity_y=s->movement.velocity_y;
    entity->body.velocity_z=s->velocity_z; s->facing=(uint8_t)(heading>>8);
    if (ops->horizontal(ops->context,entity)) return -1;
    vertical->flags=s->movement.flags;
    if (options && options->jump)
        (void)ssa_player_vertical_step(&entity->body,vertical,options->jump,frame->ticks,
            options->jump_down,options->controls_enabled && !(s->movement.flags&SSA_PLAYER_CONTROL_LOCK));
    else (void)ssa_vertical_step(&entity->body,vertical,frame->ticks);
    s->movement.flags=vertical->flags;
    s->x=entity->body.x; s->y=entity->body.y; s->z=entity->body.z;
    s->movement.velocity_x=entity->body.velocity_x; s->movement.velocity_y=entity->body.velocity_y;
    s->velocity_z=entity->body.velocity_z;
    s->movement.heading=options && options->retain_heading ? heading : (uint16_t)((unsigned)s->facing<<8);
    return 0;
}

typedef struct WorldMotion {
    SsaWorld *world; SsaSlide *slide; const SsaMathTables *tables; const SsaMotionFrame *frame;
    unsigned player_clearance;
} WorldMotion;
static int horizontal(void *context,SsaEntity *entity) {
    WorldMotion *m=context;
    return (m->player_clearance?ssa_world_horizontal_step_player:ssa_world_horizontal_step)(m->world,entity,m->slide,m->tables,m->frame->ticks,
                                    m->frame->player_count,m->frame->local_player);
}
static int world_step(SsaPlayerMotion *s,SsaEntity *e,SsaPlayerVertical *v,
                                  SsaWorld *world,SsaSlide *slide,const SsaMathTables *tables,
                                  const SsaMotionFrame *frame,const SsaMotionOptions *options,unsigned player) {
    WorldMotion context={world,slide,tables,frame,player};
    SsaMotionOps ops={horizontal,&context};
    if (!s || !e || !world || world->query_active || e->owner!=world || !slide ||
        slide->map!=world->collision.map || s->x!=e->body.x || s->y!=e->body.y) return -1;
    return ssa_player_motion_step(s,e,v,tables,frame,&ops,options);
}
int ssa_player_motion_world_step(SsaPlayerMotion *s,SsaEntity *e,SsaPlayerVertical *v,
                                  SsaWorld *world,SsaSlide *slide,const SsaMathTables *tables,
                                  const SsaMotionFrame *frame,const SsaMotionOptions *options) {
    return world_step(s,e,v,world,slide,tables,frame,options,0);
}
int ssa_player_motion_world_step_pc(SsaPlayerMotion *s,SsaEntity *e,SsaPlayerVertical *v,
                                  SsaWorld *world,SsaSlide *slide,const SsaMathTables *tables,
                                  const SsaMotionFrame *frame,const SsaMotionOptions *options) {
    return world_step(s,e,v,world,slide,tables,frame,options,1);
}
