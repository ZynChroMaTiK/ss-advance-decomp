#include "ssa/projectile_runtime.h"
#include "ssa/projectile_effect.h"
#include "ssa/projectile_extended.h"
#include "ssa/sector_clip.h"
#include "projectile_runtime_internal.h"
#include <limits.h>
#include <string.h>

static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
static int bound(const SsaProjectileRuntime *r) {
    SsaScriptSpawn *s=r && r->combat?r->combat->spawn:NULL;
    return s && r->math && s->world && s->level && s->level->map &&
        s->world->cells && s->world->sectors && s->world->collision.map==s->level->map &&
        r->combat->damage && r->combat->player_count && r->combat->player_count<=4;
}
static int idle(const SsaProjectileRuntime *r) {
    unsigned i;
    if(!bound(r) || r->updating || r->creator.updating || r->bounce.updating ||
       r->combat->updating || r->combat->damage->updating || r->combat->explosion.updating ||
       r->combat->spawn->updating || r->combat->spawn->world->query_active ||
       r->combat->spawn->level->updating || r->combat->spawn->level->script.updating ||
       r->combat->spawn->level->movers.updating || r->combat->spawn->level->switches.updating) return 0;
    for(i=0;i<r->combat->player_count;++i)
        if(!r->combat->players[i] || r->combat->players[i]->updating) return 0;
    return 1;
}
static int slot_of(const SsaProjectileRuntime *r,const SsaProjectile *p) {
    unsigned i;
    if(!p) return -1;
    for(i=0;i<100;++i) if(p==&r->projectiles[i]) return (int)i;
    return -1;
}
static int live(const SsaProjectileRuntime *r,unsigned slot) {
    const SsaScriptSpawn *s=r->combat->spawn;
    return slot<100 && r->active[slot] &&
        s->pool.active_count<=100 && s->pool.positions[slot]<s->pool.active_count &&
        s->pool.active_slots[s->pool.positions[slot]]==slot &&
        s->pool.occupied[slot] && s->actors[slot].owned &&
        r->projectiles[slot].entity==&s->actors[slot].entity &&
        s->actors[slot].activation.entity==&s->actors[slot].entity &&
        (!r->effects[slot].type || r->effects[slot].type==s->actors[slot].entity.type) &&
        !s->actors[slot].activation.updating;
}
static int registered(const SsaProjectileRuntime *r,const SsaEntity *e) {
    unsigned i;const SsaScriptSpawn *s=r->combat->spawn;
    if(!e) return 0;
    for(i=0;i<s->reserved;++i) if(!s->actors[i].owned && e==r->combat->reserved[i]) return e->owner==s->world;
    for(i=0;i<100;++i) if(e==&s->actors[i].entity)
        return s->pool.active_count<=100 && s->pool.positions[i]<s->pool.active_count &&
            s->pool.active_slots[s->pool.positions[i]]==i && s->pool.occupied[i] &&
            s->actors[i].owned && e->owner==s->world &&
            s->actors[i].activation.entity==e;
    return 0;
}
static int stable_source(const SsaProjectileRuntime *r,const SsaEntity *e) {
    unsigned i;
    if(registered(r,e)) return 1;
    for(i=0;i<100;++i) if(e==&r->combat->spawn->actors[i].entity) return 1;
    return 0;
}
static void sync_out(SsaProjectileRuntime *r,unsigned slot) {
    SsaProjectile *p=&r->projectiles[slot];SsaScriptActor *a=&r->combat->spawn->actors[slot];
    a->activation.health=p->health;a->activation.timer=p->timer;a->activation.state=p->state;
    a->activation.facing=p->facing;a->parameter34=p->parameter34;
    a->script.flags=a->entity.body.flags;a->script.timer=p->timer;a->script.variant=p->state;
    a->script.facing=p->facing;a->script.type=a->entity.type;
    a->animation=p->animation;a->animation_ticks=p->animation_period;
    a->animation_flags=p->animation_done;a->entity.player_index=p->owner;
}
static void sync_in(SsaProjectileRuntime *r,unsigned slot) {
    SsaProjectile *p=&r->projectiles[slot];SsaScriptActor *a=&r->combat->spawn->actors[slot];
    p->health=a->activation.health;p->timer=a->activation.timer;
    p->state=a->activation.state;p->facing=a->activation.facing;p->parameter34=a->parameter34;
    p->animation=a->animation;p->animation_period=a->animation_ticks;p->animation_done=a->animation_flags;
}
static int release_inner(SsaProjectileRuntime *r,unsigned slot) {
    SsaScriptSpawn *s=r->combat->spawn;SsaEntity *e;
    if(!live(r,slot) || r->projectiles[slot].updating) return -1;
    e=&s->actors[slot].entity;
    if(e->owner && (e->owner!=s->world || !ssa_world_remove(s->world,e))) return -1;
    if(ssa_entity_pool_release(&s->pool,slot)!=1) return -1;
    memset(&s->actors[slot],0,sizeof(s->actors[slot]));
    memset(&r->projectiles[slot],0,sizeof(r->projectiles[slot]));
    memset(&r->effects[slot],0,sizeof(r->effects[slot]));r->active[slot]=0;
    r->time_strike_phase[slot]=0;r->time_strike_tick_phase[slot]=0;return 0;
}
int ssa_projectile_runtime_init(SsaProjectileRuntime *r,SsaCombatRuntime *combat,const SsaMathTables *math) {
    if(!r || r->combat || r->updating || !combat || !math) return -1;
    r->combat=combat;r->math=math;
    if(!idle(r) || ssa_projectile_bounce_init(&r->bounce,combat->spawn->level->map)!=1) {
        r->combat=NULL;r->math=NULL;return -1;
    }
    return 0;
}
int ssa_projectile_runtime_release(SsaProjectileRuntime *r,unsigned slot) {
    int result;if(!idle(r)) return -1;
    r->updating=1;r->combat->spawn->updating=1;result=release_inner(r,slot);
    r->combat->spawn->updating=0;r->updating=0;return result;
}
int ssa_projectile_runtime_free(SsaProjectileRuntime *r) {
    unsigned i;
    if(!idle(r)) return -1;
    r->updating=1;r->combat->spawn->updating=1;
    for(i=0;i<100;++i) if(r->active[i] && release_inner(r,i)<0) {
        r->combat->spawn->updating=0;r->updating=0;return -1;
    }
    r->combat->spawn->updating=0;r->updating=0;
    if(ssa_projectile_bounce_free(&r->bounce)<0) return -1;
    memset(r,0,sizeof(*r));return 0;
}
static int allocate(void *context,SsaProjectile **out) {
    SsaProjectileRuntime *r=context;SsaScriptSpawn *s=r->combat->spawn;unsigned slot;int result;
    *out=NULL;
    if(s->world->entity_count>=s->world->capacity) return 0;
    result=ssa_entity_pool_allocate(&s->pool,&slot);
    if(result!=1) return result<0?-1:0;
    if(s->actors[slot].owned || r->active[slot]) {
        ssa_entity_pool_release(&s->pool,slot);return -1;
    }
    memset(&s->actors[slot],0,sizeof(s->actors[slot]));
    memset(&r->projectiles[slot],0,sizeof(r->projectiles[slot]));
    memset(&r->effects[slot],0,sizeof(r->effects[slot]));
    r->time_strike_phase[slot]=0;r->time_strike_tick_phase[slot]=0;
    s->actors[slot].owned=1;s->actors[slot].activation.entity=&s->actors[slot].entity;
    r->projectiles[slot].entity=&s->actors[slot].entity;r->active[slot]=1;
    *out=&r->projectiles[slot];return 0;
}
static int insert_create(void *context,SsaProjectile *p) {
    SsaProjectileRuntime *r=context;int slot=slot_of(r,p);
    if(slot<0 || !live(r,(unsigned)slot)) return -1;
    r->effects[slot].type=p->entity->type;
    sync_out(r,(unsigned)slot);
    return ssa_world_insert(r->combat->spawn->world,p->entity)?0:-1;
}
static int particle_animation(void *context,SsaProjectile *p) {
    SsaProjectileRuntime *r=context;int slot=slot_of(r,p);
    if(slot<0 || !live(r,(unsigned)slot)) return -1;
    sync_out(r,(unsigned)slot);
    r->combat->spawn->actors[slot].animation_frame=0;
    r->combat->spawn->actors[slot].animation_counter=0;
    return 0;
}
static int particle_sound(void *context,SsaProjectile *p,unsigned sound) {
    SsaProjectileRuntime *r=context;
    return r->particle_sound(r->particle_context,r,p,sound);
}
static int particle_inner(SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    SsaParticleCreateOps ops={allocate,particle_animation,insert_create,particle_sound,r};
    SsaProjectile *p=NULL;int result,slot;
    if(!r->particle_rules || !r->particle_rules->loaded || !r->particle_sound) return -1;
    result=ssa_particle_create(&r->creator,r->particle_rules,id,x,y,z,&ops,&p);
    if(p) {
        slot=slot_of(r,p);if(slot<0) return -1;
        sync_out(r,(unsigned)slot);
    }
    return result<0?-1:0;
}
int ssa_projectile_runtime_bind_particles(SsaProjectileRuntime *r,const SsaParticleRules *rules,
    int (*sound)(void *,const SsaProjectileRuntime *,const SsaProjectile *,unsigned),void *context) {
    if(!idle(r) || !rules || !rules->loaded || !sound) return -1;
    r->particle_rules=rules;r->particle_sound=sound;r->particle_context=context;return 0;
}
int ssa_projectile_runtime_particle_scoped(SsaProjectileRuntime *r,SsaCombatScope *scope,
    unsigned id,int32_t x,int32_t y,int32_t z) {
    int result;
    if(!bound(r) || r->updating || r->creator.updating || !scope ||
       !r->combat->updating || !r->combat->spawn->updating ||
       r->combat->spawn->world->query_active) return -1;
    r->updating=1;result=particle_inner(r,id,x,y,z);r->updating=0;return result;
}
int ssa_projectile_runtime_particle_held(SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z) {
    SsaScriptSpawn *s;int result;
    if(!bound(r) || r->updating || r->creator.updating || r->bounce.updating) return -1;
    s=r->combat->spawn;
    if(!s->updating || s->world->query_active ||
       (!r->combat->updating && !(s->level->updating && s->level->script.updating))) return -1;
    r->updating=1;result=particle_inner(r,id,x,y,z);r->updating=0;return result;
}
typedef struct ParticleCall { SsaProjectileRuntime *r;unsigned id;int32_t x,y,z; } ParticleCall;
static int particle_scope(void *context,SsaCombatScope *scope) {
    ParticleCall *c=context;
    return ssa_projectile_runtime_particle_scoped(c->r,scope,c->id,c->x,c->y,c->z);
}
int ssa_projectile_runtime_particle(SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z,
    const SsaCombatRuntimeOps *ops) {
    ParticleCall c={r,id,x,y,z};
    if(!idle(r)) return -1;
    return ssa_combat_scope_run(r->combat,ops,particle_scope,&c);
}
int ssa_projectile_runtime_enemy_create_scoped(SsaProjectileRuntime *r,SsaCombatScope *scope,
    const SsaProjectileCreateRequest *request,const SsaEntity *target,int aimed,SsaProjectile **out) {
    SsaProjectileCreateOps ops={allocate,insert_create,r};SsaProjectile *p=NULL;int result,slot;
    int16_t health;
    if(!out)return -1;
    *out=NULL;
    if(!bound(r) || r->updating || r->creator.updating || !scope || !request ||
       !r->combat->updating || !r->combat->spawn->updating ||
       r->combat->spawn->world->query_active || !registered(r,request->source) ||
       /* Navigation can retain an original pool-slot pointer after its actor
        * expires. The slot storage remains valid, including through reuse by
        * this allocation; 1097C reads that storage after creating the shot. */
       (target && !stable_source(r,target)) || (aimed!=0 && aimed!=1) ||
       ssa_combat_scope_health(scope,request->source,&health)<0) return -1;
    if(request->type!=0x64 && request->type!=0x65 && request->type!=0x66 && request->type!=0x67 &&
       request->type!=0x68 && request->type!=0x69 && request->type!=0x6a &&
       request->type!=0x6b && request->type!=0xe3) return -1;
    r->updating=1;
    result=aimed?ssa_projectile_create_target(&r->creator,request,target,r->math,&ops,&p):
        ssa_projectile_create(&r->creator,request,r->math,&ops,&p);
    if(p) {
        slot=slot_of(r,p);
        if(slot<0) result=-1;else sync_out(r,(unsigned)slot);
    }
    *out=p;r->updating=0;return result<0?-1:0;
}
int ssa_projectile_runtime_enemy_scoped(SsaProjectileRuntime *r,SsaCombatScope *scope,
    const SsaProjectileCreateRequest *request,const SsaEntity *target,int aimed) {
    SsaProjectile *p=NULL;
    return ssa_projectile_runtime_enemy_create_scoped(r,scope,request,target,aimed,&p);
}
int ssa_projectile_runtime_enemy_commit_scoped(SsaProjectileRuntime *r,SsaCombatScope *scope,SsaProjectile *p) {
    int slot;
    if(!bound(r) || !scope || r->updating || !r->combat->updating || !r->combat->spawn->updating ||
       r->combat->spawn->world->query_active || (slot=slot_of(r,p))<0 || !live(r,(unsigned)slot) || p->updating)return -1;
    sync_out(r,(unsigned)slot);return 0;
}
static void effect_copy(SsaWeaponEffect *e,const SsaProjectile *p) {
    const SsaBody *b=&p->entity->body;
    *e=(SsaWeaponEffect){b->x,b->y,b->z,b->velocity_x,b->velocity_y,b->velocity_z,p->speed,
        p->entity->radius,p->source,p->entity->type,b->flags,p->state,p->owner,p->weapon};
}
int ssa_projectile_runtime_weapon(SsaProjectileRuntime *r,SsaCombatScope *scope,
                                  SsaEntity *source,SsaWeaponEvent *event) {
    SsaProjectileCreateOps ops={allocate,insert_create,r};SsaProjectile *p=NULL;int result=-1;
    unsigned i;int16_t health;
    if(!bound(r) || r->updating || r->creator.updating || !scope || !event ||
       !r->combat->updating || !r->combat->spawn->updating ||
       r->combat->spawn->world->query_active || !registered(r,source) ||
       ssa_combat_scope_health(scope,source,&health)<0) return -1;
    r->updating=1;
    if(event->kind==SSA_WEAPON_PROJECTILE_CREATE) {
        SsaProjectileCreateRequest request={(uint32_t)event->args[0],source,event->args[1],
            event->args[2],event->args[3],(uint32_t)event->args[4],event->args[5]};
        event->effect=NULL;
        if(request.type!=0x65 && request.type!=0x67 && request.type!=0x69) goto done;
        result=ssa_projectile_create_aimed(&r->creator,&request,r->math,&ops,&p);
        if(p) {
            int slot=slot_of(r,p);
            if(slot<0) {result=-1;goto done;}
            sync_out(r,(unsigned)slot);effect_copy(&r->effects[slot],p);
            event->effect=&r->effects[slot];
        }
        result=result<0?-1:0;
    } else if(event->kind==SSA_WEAPON_EFFECT_ALLOCATE) {
        event->effect=NULL;
        if(allocate(r,&p)<0)goto done;
        if(p){
            int slot=slot_of(r,p);if(slot<0)goto done;
            p->source=source;event->effect=&r->effects[slot];
        }
        result=0;
    } else if(event->kind==SSA_WEAPON_EFFECT_ANIMATE || event->kind==SSA_WEAPON_EFFECT_INSERT) {
        SsaWeaponEffect *e=event->effect;SsaScriptSpawn *s=r->combat->spawn;
        for(i=0;i<100;++i)if(e==&r->effects[i])break;
        if(i==100 || !r->active[i] || s->pool.active_count>100 || !s->pool.occupied[i] || !s->actors[i].owned ||
           s->pool.positions[i]>=s->pool.active_count || s->pool.active_slots[s->pool.positions[i]]!=i ||
           s->actors[i].activation.entity!=&s->actors[i].entity || s->actors[i].activation.updating ||
           r->projectiles[i].entity!=&s->actors[i].entity || e->type!=0xe3 ||
           e->target!=event->target || !registered(r,e->target))goto done;
        p=&r->projectiles[i];
        if(p->entity->owner || p->updating)goto done;
        if(event->kind==SSA_WEAPON_EFFECT_ANIMATE){
            if(p->source!=source || p->entity->type || event->args[0]<0x60 ||
               event->args[0]>0x62 || event->args[1]!=5)goto done;
            p->entity->type=e->type;p->entity->body.x=e->x;p->entity->body.y=e->y;p->entity->body.z=e->z;
            p->entity->body.velocity_x=e->vx;p->entity->body.velocity_y=e->vy;p->entity->body.velocity_z=e->vz;
            p->entity->body.flags=e->flags;p->entity->radius=e->radius;
            p->state=e->state;p->source=e->target;p->owner=e->owner;p->weapon=e->weapon;p->speed=e->travel;
            p->animation=(uint16_t)event->args[0];p->animation_period=(uint8_t)event->args[1];
            sync_out(r,i);s->actors[i].animation_frame=s->actors[i].animation_counter=0;result=0;
        }else{
            if(!live(r,i) || p->entity->type!=0xe3 || p->source!=e->target || p->animation_period!=5)goto done;
            result=insert_create(r,p);
        }
    } else if(event->kind==SSA_WEAPON_PROJECTILE_COMMIT) {
        for(i=0;i<100;++i) if(event->effect==&r->effects[i]) break;
        if(i==100 || !live(r,i) || r->projectiles[i].source!=source ||
           r->projectiles[i].entity->owner!=r->combat->spawn->world ||
           event->effect->owner>r->combat->player_count) goto done;
        p=&r->projectiles[i];p->speed=event->effect->travel;
        p->owner=event->effect->owner;p->weapon=event->effect->weapon;
        if(p->owner && ((r->ballistic_player_rockets && p->entity->type==0x65) ||
                       (r->ballistic_player_prion && p->entity->type==0x69))){
            p->entity->body.flags&=UINT16_C(0xbfff);
            event->effect->flags=p->entity->body.flags;
        }
        sync_out(r,i);result=0;
    }
done:
    r->updating=0;return result;
}
typedef struct StepCall {
    SsaProjectileRuntime *r;SsaCombatScope *scope;const SsaProjectileRuntimeOps *ops;
    SsaProjectileMotionGame motion;SsaProjectileControllerGame controller;
    unsigned slot;
} StepCall;
static int move(void *context,SsaProjectile *p,int32_t x,int32_t y) {
    StepCall *c=context;SsaWorld *w=c->r->combat->spawn->world;
    /* Gunrilla's80-unit muzzle is inside its90+16-unit collision bounds.
     * Let its cannonball leave the shooter, without hiding any other contact
     * or extending immunity after their current bounds have separated. */
    const SsaEntity *source=p->source;
    if(!p->owner && p->entity->type==0x67 && source && source->owner==w && source->type==0xd6 &&
       ssa_entity_contact(p->entity,source,p->entity->body.x,p->entity->body.y)==0)
        return ssa_world_try_move_ignoring(w,p->entity,x,y,source);
    return ssa_world_try_move(w,p->entity,x,y);
}
static int vertical(void *context,SsaProjectile *p) {
    StepCall *c=context;return ssa_vertical_step(&p->entity->body,NULL,c->motion.ticks);
}
static int blocker(void *context,SsaProjectile *p,SsaEntity **out) {
    StepCall *c=context;(void)p;*out=c->r->combat->spawn->world->blocking_entity;return 0;
}
static int health(void *context,const SsaEntity *e,int16_t *out) {
    StepCall *c=context;return ssa_combat_scope_health(c->scope,(SsaEntity *)e,out);
}
static int credits(StepCall *c,uint32_t credit,uint32_t weapon) {
    SsaCombatDamage *d=c->r->combat->damage;
    if(credit>c->r->combat->player_count || weapon>10 ||
       (credit && c->r->combat->player_count>1 && !c->r->credited_kills[credit-1]) ||
       (credit && weapon && !c->r->hit_counts[credit-1])) return -1;
    d->credit_player=credit;d->shot_hit_count=credit && weapon?&c->r->hit_counts[credit-1][weapon-1]:NULL;
    d->credited_kills=credit?c->r->credited_kills[credit-1]:NULL;return 0;
}
static int damage(void *context,SsaProjectile *p,SsaEntity *e,uint32_t amount) {
    StepCall *c=context;int result,slot=slot_of(c->r,p),scoped_credit=p->entity->type==0x68;
    if(slot<0) return -1;
    if(scoped_credit && credits(c,c->motion.credit,c->motion.hit_weapon)<0) return -1;
    sync_out(c->r,(unsigned)slot);
    result=ssa_combat_scope_damage(c->scope,e,p->entity,p->source,amount);
    if(scoped_credit) {
        c->r->combat->damage->credit_player=0;c->r->combat->damage->shot_hit_count=NULL;
    }
    return result;
}
static int freeze(void *context,SsaProjectile *p,SsaEntity *target) {
    StepCall *c=context;SsaProjectileRuntime *r=c->r;SsaScriptSpawn *s=r->combat->spawn;
    SsaProjectile *f=NULL;unsigned i;int result;(void)p;
    for(i=0;i<100;++i) if(target==&s->actors[i].entity && s->actors[i].owned) break;
    if(i==100 || !registered(r,target)) return -1;
    s->actors[i].parameter34=30;s->actors[i].script.flags=target->body.flags;
    result=allocate(r,&f);if(result<0 || !f) return result;
    f->source=target;f->entity->type=0xe2;f->entity->body.x=target->body.x;
    f->entity->body.y=target->body.y;
    f->entity->body.z=bits((uint32_t)target->body.z+(uint32_t)target->body.height+0x100000u);
    f->entity->body.flags|=0x20;f->animation=0x5b;f->animation_period=5;
    return insert_create(r,f);
}
static int unlink_motion(void *context,SsaProjectile *p) {
    StepCall *c=context;return ssa_world_remove(c->r->combat->spawn->world,p->entity)?0:-1;
}
static int insert_motion(void *context,SsaProjectile *p) {
    StepCall *c=context;return ssa_world_insert(c->r->combat->spawn->world,p->entity)?0:-1;
}
static int bounce(void *context,SsaProjectile *p) {
    StepCall *c=context;return ssa_projectile_bounce(&c->r->bounce,&p->entity->body,p->entity->radius,
        c->r->math,c->motion.ticks);
}
static SsaProjectileMotionOps motion_ops(StepCall *c) {
    SsaProjectileMotionOps ops={move,vertical,blocker,health,damage,freeze,unlink_motion,insert_motion,bounce,c};
    return ops;
}
static int motion(void *context,SsaProjectile *p) {
    StepCall *c=context;SsaProjectileMotionOps ops=motion_ops(c);int result,slot=slot_of(c->r,p);
    if(slot<0) return -1;
    c->motion.credit=c->controller.credit;c->motion.hit_weapon=c->controller.hit_weapon;
    result=c->r->unlimited_player_range?ssa_projectile_motion_unlimited(p,&c->motion,c->r->math,&ops):
        ssa_projectile_motion(p,&c->motion,c->r->math,&ops);
    c->controller.credit=c->motion.credit;c->controller.hit_weapon=c->motion.hit_weapon;
    sync_out(c->r,(unsigned)slot);return result;
}
static int controller_event(void *context,const SsaProjectileController *controller,SsaProjectileEvent kind,
    int32_t a,int32_t b,int32_t d) {
    StepCall *c=context;SsaProjectile *p=controller->projectile;int result,slot=slot_of(c->r,p);
    if(slot<0) return -1;
    sync_out(c->r,(unsigned)slot);
    if(kind==SSA_PROJECTILE_ANIMATION) {
        SsaScriptActor *actor=&c->r->combat->spawn->actors[slot];
        if((uint16_t)a<=0x8d || (uint16_t)a==0xffff)actor->animation_frame=actor->animation_counter=0;
        return 0;
    }
    if(kind==SSA_PROJECTILE_EXPLOSION) {
        if(credits(c,c->controller.credit,c->controller.hit_weapon)<0) return -1;
        result=ssa_combat_scope_explode(c->scope,p->entity,p->entity,p->source,(uint32_t)a,(uint32_t)b,d);
        c->r->combat->damage->credit_player=0;c->r->combat->damage->shot_hit_count=NULL;
        return result;
    }
    if(kind==SSA_PROJECTILE_PARTICLE) {
        if(c->r->particle_rules) return particle_inner(c->r,(unsigned)a,
            p->entity->body.x,p->entity->body.y,p->entity->body.z);
        return c->ops->particle(c->ops->context,c->r,(unsigned)a,
            p->entity->body.x,p->entity->body.y,p->entity->body.z);
    }
    return kind==SSA_PROJECTILE_DESTROY?0:-1;
}
static int create_child(void *context,const SsaProjectileCreateRequest *request,SsaProjectile **out) {
    StepCall *c=context;SsaProjectileCreateOps ops={allocate,insert_create,c->r};
    int result=ssa_projectile_create(&c->r->creator,request,c->r->math,&ops,out),slot;
    if(*out) {
        slot=slot_of(c->r,*out);if(slot<0)return -1;
        sync_out(c->r,(unsigned)slot);
    }
    return result;
}
static int finish_child(void *context,const SsaProjectileController *controller) {
    StepCall *c=context;int slot=slot_of(c->r,controller->projectile);
    if(slot<0 || !live(c->r,(unsigned)slot))return -1;
    sync_out(c->r,(unsigned)slot);
    return controller->destroy_requested?release_inner(c->r,(unsigned)slot):0;
}
static int relocate(void *context,SsaProjectile *p) {
    StepCall *c=context;SsaSectorClip state;SsaWorld *world=c->r->combat->spawn->world;
    int result=ssa_sector_relocate(world->collision.map,c->r->math,p->entity,&state);
    if(result<0)return -1;
    return ssa_world_sector_move(world,p->entity,state.end_x,state.end_y,state.end_z,
        state.leaf,state.sector,result)<0?-1:0;
}
static int step_scope(void *context,SsaCombatScope *scope) {
    StepCall *c=context;SsaProjectileRuntime *r=c->r;SsaProjectile *p=&r->projectiles[c->slot];
    SsaCombatDamage *d=r->combat->damage;uint32_t old_credit=d->credit_player;
    uint32_t *old_hits=d->shot_hit_count;uint8_t *old_kills=d->credited_kills;
    SsaProjectileController controller={p,0,0};SsaProjectileControllerOps ops={motion,controller_event,c};
    unsigned i;int result;
    c->scope=scope;c->motion.player_count=(uint8_t)r->combat->player_count;
    for(i=0;i<r->combat->player_count;++i) {
        c->motion.players[i]=&r->combat->players[i]->player->entity;
        c->motion.headings[i]=r->combat->players[i]->player->motion.movement.heading;
    }
    sync_in(r,c->slot);
    if(p->entity->type==0x96) result=ssa_particle_step(p);
    else if(p->entity->type==0xe2) {
        SsaProjectileMotionOps follow=motion_ops(c);
        if(!registered(r,p->source) || (uint16_t)(p->source->type-0xcd)>9) result=-1;
        else {
            result=ssa_projectile_effect_follow(p,&follow);
            if(result>=0) result=result?0:1;
        }
    } else if(p->entity->type==0x64 || p->entity->type==0x68 || p->entity->type==0x6a || p->entity->type==0xe3) {
        SsaProjectileExtendedOps extended={ops,create_child,finish_child,relocate,c};
        result=ssa_projectile_extended_step(&controller,&c->controller,r->math,c->motion.ticks,&extended);
    } else result=ssa_projectile_controller_step(&controller,&c->controller,&ops);
    sync_out(r,c->slot);
    d->credit_player=old_credit;d->shot_hit_count=old_hits;d->credited_kills=old_kills;
    if(result==1 || controller.destroy_requested) {
        if(release_inner(r,c->slot)<0) return -1;
        return result<0?-1:1;
    }
    return result;
}
int ssa_projectile_runtime_render_z(const SsaProjectileRuntime *r,unsigned slot,int32_t *z) {
    const SsaProjectile *p;int64_t next,floor,phase;
    const int64_t period=(int64_t)SSA_TIME_STRIKE_GBA_REFRESHES*280896*60;
    if(!r || !z || !bound(r) || !live(r,slot) || r->updating || r->time_strike_gba_clock>1)return -1;
    p=&r->projectiles[slot];*z=p->entity->body.z;
    if(!r->time_strike_gba_clock || p->entity->type!=0xe3 || p->state)return 1;
    next=(int64_t)*z+((int64_t)p->entity->body.velocity_z-2*65536)*
        ((r->time_strike_tick_phase[slot]+SSA_TIME_STRIKE_GBA_REFRESHES)/SSA_TIME_STRIKE_GBA_INTERVALS);
    floor=(int64_t)p->entity->body.floor*65536;
    if(next<floor+65536)next=floor; /* Original integer-Z landing comparison. */
    if(next>INT32_MAX)next=INT32_MAX;
    if(next<INT32_MIN)next=INT32_MIN;
    phase=r->time_strike_phase[slot]>(uint64_t)period?period:(int64_t)r->time_strike_phase[slot];
    *z=(int32_t)((int64_t)*z+(next-*z)*phase/period);return 1;
}
int ssa_projectile_runtime_step(SsaProjectileRuntime *r,unsigned slot,unsigned ticks,
                               const SsaProjectileRuntimeOps *ops) {
    StepCall c;int result;
    if(!idle(r) || !live(r,slot) || ticks>255 || r->time_strike_gba_clock>1 || !ops || !ops->combat ||
       !ops->combat->event || (!ops->particle && !r->particle_rules) || r->projectiles[slot].updating ||
       r->projectiles[slot].entity->owner!=r->combat->spawn->world ||
       (r->projectiles[slot].entity->type!=0x96 && !stable_source(r,r->projectiles[slot].source))) return -1;
    memset(&c,0,sizeof(c));c.r=r;c.ops=ops;c.slot=slot;c.motion.ticks=(uint8_t)ticks;
    r->updating=1;
    if(r->time_strike_gba_clock && r->projectiles[slot].entity->type==0xe3) {
        const uint64_t period=(uint64_t)SSA_TIME_STRIKE_GBA_REFRESHES*280896*60;
        /* Gravity subtracts2 ONCE per original controller call, not per tick.
         * Admit measured GBA updates with rational wall time, and pass their
         * elapsed6/7 ticks to the unchanged original controller. */
        r->time_strike_phase[slot]+=(uint64_t)ticks*16777216*SSA_TIME_STRIKE_GBA_INTERVALS;
        result=0;
        while(r->time_strike_phase[slot]>=period) {
            unsigned elapsed=r->time_strike_tick_phase[slot]+SSA_TIME_STRIKE_GBA_REFRESHES;
            r->time_strike_phase[slot]-=period;
            c.motion.ticks=(uint8_t)(elapsed/SSA_TIME_STRIKE_GBA_INTERVALS);
            r->time_strike_tick_phase[slot]=(uint8_t)(elapsed%SSA_TIME_STRIKE_GBA_INTERVALS);
            result=ssa_combat_scope_run(r->combat,ops->combat,step_scope,&c);
            if(result!=0)break;
        }
    }else result=ssa_combat_scope_run(r->combat,ops->combat,step_scope,&c);
    r->updating=0;return result;
}
