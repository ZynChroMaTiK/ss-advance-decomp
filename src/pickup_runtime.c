#include "ssa/pickup_runtime.h"
#include <string.h>
static int bound(const SsaPickupRuntime *r)
{
    const SsaCombatRuntime *c=r?r->combat:NULL;const SsaScriptSpawn *s=c?c->spawn:NULL;unsigned i;
    if(!r || !r->loaded || !c || !s || !s->world || !s->level || !s->level->map ||
       !s->world->cells || !s->world->sectors || s->world->collision.map!=s->level->map ||
       !r->game.rules || !r->ops.event || r->pc_pickups>1 || !c->damage || !c->player_count || c->player_count>4 ||
       c->damage->player_count!=c->player_count || c->damage->local_player>=c->player_count ||
       c->reserved_count!=s->reserved || s->reserved>100)return 0;
    for(i=0;i<c->player_count;++i){const SsaPlayerRuntime *p=c->players[i];
        if(!p || !p->player || !p->player->active || p->world!=s->world ||
            p->player->entity.owner!=s->world || p->player->entity.player_index!=i)return 0;
    }
    return 1;
}
static int idle(const SsaPickupRuntime *r)
{
    const SsaCombatRuntime *c=r->combat;const SsaScriptSpawn *s=c->spawn;unsigned i;
    if(r->updating || c->updating || c->damage->updating || c->explosion.updating ||
        s->updating || s->world->query_active || s->level->updating || s->level->script.updating ||
        s->level->movers.updating || s->level->switches.updating)return 0;
    for(i=0;i<c->player_count;++i)if(c->players[i]->updating)return 0;
    return 1;
}
static void event(void *context,enum SsaPickupEvent kind,unsigned a,unsigned b)
{
    SsaPickupRuntime *r=context;SsaLevelRuntime *l=r->combat->spawn->level;
    if(r->failed)return;
    if(kind==SSA_PICKUP_TRIGGER){
        if(!ssa_level_script_queue(&l->script,&l->data,l->level,a))r->failed=1;
    }else{
        SsaPlayerSpawn *p=r->combat->players[r->player]->player;
        p->entity_health=p->inventory.health;
        r->ops.event(r->ops.context,r,r->player,kind,a,b);
    }
}
static void remove_pickup(void *context,SsaPickup *p)
{
    SsaPickupRuntime *r=context;SsaScriptSpawn *s=r->combat->spawn;SsaScriptActor *a=&s->actors[r->slot];
    if(r->failed)return;
    if(p!=&r->current || p->handle!=a || s->world->executing_special!=&a->entity ||
       !ssa_world_remove(s->world,&a->entity) || ssa_entity_pool_release(&s->pool,r->slot)!=1){r->failed=1;return;}
    memset(a,0,sizeof(*a));p->item=p->trigger=0;
}
int ssa_pickup_runtime_init(SsaPickupRuntime *r,SsaCombatRuntime *combat,
    const SsaInventoryRules *rules,const SsaPickupRuntimeOps *ops)
{
    SsaPickupRuntime n={0};
    if(!r || r->loaded || r->updating || r->combat || !rules || !ops || !ops->event)return -1;
    n.combat=combat;n.game.rules=rules;n.ops=*ops;n.loaded=1;
    if(!bound(&n) || !idle(&n) || combat->spawn->world->execute_special)return -1;
    n.game.player_count=(uint8_t)combat->player_count;n.game.local_player=combat->damage->local_player;
    *r=n;r->game.ops=(SsaPickupOps){event,remove_pickup,NULL,r};
    combat->spawn->world->execute_special=ssa_pickup_runtime_execute;combat->spawn->world->execute_context=r;
    return 0;
}
int ssa_pickup_runtime_free(SsaPickupRuntime *r)
{
    SsaWorld *w;
    if(!bound(r) || !idle(r))return -1;
    w=r->combat->spawn->world;
    if(w->execute_special!=ssa_pickup_runtime_execute || w->execute_context!=r)return -1;
    w->execute_special=NULL;w->execute_context=NULL;memset(r,0,sizeof(*r));return 0;
}
int ssa_pickup_runtime_execute(void *context,const SsaEntity *current,SsaEntity *special)
{
    SsaPickupRuntime *r=context;SsaCombatRuntime *c;SsaScriptSpawn *s;SsaScriptActor *actor;
    SsaPlayerSpawn *p;SsaPickupPlayer view;SsaEntityPool check;unsigned player,slot,i,locks[4],level_lock;int result;
    if(!bound(r) || !current || !special)return -1;
    if((uint16_t)(current->type-0xc9u)>3u)return 0;
    c=r->combat;s=c->spawn;
    if(r->updating || r->failed || c->updating || c->damage->updating || c->explosion.updating ||
       s->updating || !s->world->query_active || s->world->executing_special!=special ||
       s->world->execute_special!=ssa_pickup_runtime_execute || s->world->execute_context!=r ||
       special->owner!=s->world || special->type!=0xe1 || s->level->script.updating ||
       s->level->movers.updating || s->level->switches.updating)return -1;
    for(player=0;player<c->player_count && &c->players[player]->player->entity!=current;++player){}
    if(player==c->player_count)return -1;
    for(slot=0;slot<100 && &s->actors[slot].entity!=special;++slot){}
    if(slot==100 || !s->pool.occupied[slot] || !s->actors[slot].owned || s->actors[slot].activation.updating)return -1;
    check=s->pool;if(ssa_entity_pool_release(&check,slot)!=1)return -1;
    actor=&s->actors[slot];p=c->players[player]->player;
    view=(SsaPickupPlayer){&p->inventory,current->type,(uint8_t)player,current->player_index,p};
    r->slot=slot;r->player=player;r->current=(SsaPickup){actor->animation,actor->script.trigger,actor};
    r->game.player_count=(uint8_t)c->player_count;r->game.local_player=c->damage->local_player;
    r->updating=1;c->updating=1;s->updating=1;level_lock=s->level->updating;s->level->updating=1;
    for(i=0;i<c->player_count;++i){locks[i]=c->players[i]->updating;c->players[i]->updating=1;}
    result=r->pc_pickups?ssa_pickup_touch_pc(&r->game,&view,&r->current,0,r->repeat_weapon):
        ssa_pickup_touch_alternate(&r->game,&view,&r->current,0,r->repeat_weapon);
    if(actor->owned)actor->script.trigger=r->current.trigger;
    p->entity_health=p->inventory.health;
    for(i=0;i<c->player_count;++i)c->players[i]->updating=locks[i];
    s->level->updating=level_lock;s->updating=0;c->updating=0;r->updating=0;
    return result<0 || r->failed?-1:0;
}
