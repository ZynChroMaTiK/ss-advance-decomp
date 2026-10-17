#include "ssa/level_runtime.h"
#include <string.h>

static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
static int valid(const SsaLevelRuntime *l) {
    return l && !l->updating && !l->movers.updating && !l->switches.updating &&
        !l->script.updating && l->map && l->map->data && l->data.rom &&
        l->level<16 && l->movers.map==l->map && l->switches.map==l->map &&
        l->movers.hazard_count<=8;
}
int ssa_level_runtime_init(SsaLevelRuntime *l,SsaMapAssets *m,const uint8_t *rom,
                            size_t size,unsigned level) {
    SsaLevelRuntime next={0};
    if (!l || l->map || l->switches.entries || l->updating || !m || level>=16) return -1;
    if (!ssa_level_script_data_load(&next.data,rom,size) ||
        ssa_switches_init(&next.switches,m,rom,size)<0) return -1;
    if (!ssa_sector_movers_init(&next.movers,m,rom,size,level)) {
        ssa_switches_free(&next.switches); return -1;
    }
    if (!ssa_level_script_reset(&next.script)) {
        ssa_switches_free(&next.switches); return -1;
    }
    next.map=m; next.level=level; *l=next; return 0;
}
int ssa_level_runtime_free(SsaLevelRuntime *l) {
    if (!l || l->updating || l->movers.updating || l->switches.updating || l->script.updating) return -1;
    if (ssa_switches_free(&l->switches)<0) return -1;
    memset(l,0,sizeof(*l)); return 0;
}
typedef struct LineCall { SsaLevelRuntime *l; const SsaSectorMoverOps *sound; } LineCall;
static int enqueue(void *context,unsigned tag) {
    LineCall *c=context;
    return ssa_level_script_queue(&c->l->script,&c->l->data,c->l->level,tag) ? 0 : -1;
}
static int line_sound(void *context,unsigned id) {
    LineCall *c=context;
    return c->sound && c->sound->sound ? c->sound->sound(c->sound->context,id) : 0;
}
static int line(SsaLevelRuntime *l,unsigned kind,unsigned id,const SsaSectorMoverOps *sound) {
    LineCall c={l,sound};
    SsaSwitchOps switches={enqueue,line_sound,&c};
    if (kind==1) return ssa_sector_movers_activate_line(&l->movers,id,sound) ? 0 : -1;
    if (kind==2) return ssa_switch_activate(&l->switches,id,&switches);
    return -1;
}
static int sector(SsaLevelRuntime *l,unsigned id) {
    size_t offset;
    if (id>=l->map->sector_count || id>=256 || l->map->sectors>l->map->size ||
        (size_t)(id+1)>(l->map->size-l->map->sectors)/20) return -1;
    offset=l->map->sectors+(size_t)id*20;
    return ssa_level_script_enter(&l->script,&l->data,l->level,id,
                                   (uint16_t)u16(l->map->data+offset+16)) ? 0 : -1;
}
int ssa_level_runtime_line(SsaLevelRuntime *l,unsigned kind,unsigned id,
                            const SsaSectorMoverOps *sound) {
    int result;
    if (!valid(l)) return -1;
    l->updating=1; result=line(l,kind,id,sound); l->updating=0; return result;
}
int ssa_level_runtime_sector(SsaLevelRuntime *l,unsigned id) {
    int result;
    if (!valid(l)) return -1;
    l->updating=1; result=sector(l,id); l->updating=0; return result;
}

typedef struct PlayerCall { SsaLevelRuntime *l; SsaPlayerGameplay *g; const SsaLevelPlayerOps *ops; } PlayerCall;
static int player_sound_id(void *context,unsigned id) {
    PlayerCall *c=context;
    return c->ops->sound(c->ops->context,c->g->runtime,id);
}
static int player_line(void *context,SsaPlayerGameplay *g,unsigned kind,uint16_t id) {
    PlayerCall *c=context;
    SsaSectorMoverOps sound={player_sound_id,c};
    (void)g; return line(c->l,kind,id,&sound);
}
static int player_sector(void *context,SsaPlayerRuntime *r,int16_t id) {
    PlayerCall *c=context; (void)r;
    return id<0 ? -1 : sector(c->l,(unsigned)id);
}
static int player_event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent e,
                         int32_t a,int32_t b,int32_t d,int32_t f) {
    PlayerCall *c=context;
    return c->ops->event(c->ops->context,g,e,a,b,d,f);
}
static int player_animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    PlayerCall *c=context; return c->ops->animation(c->ops->context,r,id,ticks);
}
static int player_sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    PlayerCall *c=context; return c->ops->sound(c->ops->context,r,id);
}
int ssa_level_runtime_player_step(SsaLevelRuntime *l,SsaPlayerGameplay *g,
                                   const SsaPlayerRuntimeInput *input,const SsaLevelPlayerOps *ops) {
    PlayerCall c={l,g,ops};
    SsaPlayerGameplayOps adapters={player_event,player_line,player_sector,player_animation,player_sound,&c};
    const int32_t *saved;
    size_t saved_count;
    unsigned i;
    int result;
    if (!valid(l) || !g || g->updating || !g->runtime || !g->runtime->world ||
        g->runtime->world->collision.map!=l->map || !input || !ops ||
        !ops->event || !ops->animation || !ops->sound) return -1;
    saved=g->hazard_game.hazard_sectors; saved_count=g->hazard_game.hazard_count;
    for (i=0;i<l->movers.hazard_count;++i) l->hazard_sectors[i]=(int32_t)l->movers.hazard_sectors[i];
    g->hazard_game.hazard_sectors=l->hazard_sectors;
    g->hazard_game.hazard_count=l->movers.hazard_count;
    l->updating=1;
    result=ssa_player_gameplay_step(g,input,&adapters);
    l->updating=0;
    g->hazard_game.hazard_sectors=saved; g->hazard_game.hazard_count=saved_count;
    return result;
}

typedef struct WorldCall { SsaLevelRuntime *l; const SsaLevelScriptOps *ops; const SsaSectorMoverOps *sound; } WorldCall;
static SsaLevelScriptEntity *allocate(void *context) {
    WorldCall *c=context; return c->ops->allocate(c->ops->context);
}
static int animation(void *context,SsaLevelScriptEntity *e,unsigned id,unsigned ticks) {
    WorldCall *c=context; return c->ops->animation(c->ops->context,e,id,ticks);
}
static int insert(void *context,SsaLevelScriptEntity *e) {
    WorldCall *c=context; return c->ops->insert(c->ops->context,e);
}
static int particle(void *context,unsigned id,int32_t x,int32_t y,int32_t z) {
    WorldCall *c=context; return c->ops->particle(c->ops->context,id,x,y,z);
}
static int mover(void *context,unsigned opcode,uint32_t index) {
    WorldCall *c=context;
    SsaSectorMoverCommand command;
    switch (opcode) {
    case 4: command=SSA_SECTOR_MOVER_OPEN; break;
    case 5: command=SSA_SECTOR_MOVER_CLOSE; break;
    case 6: command=SSA_SECTOR_MOVER_UNLOCK; break;
    case 7: command=SSA_SECTOR_MOVER_LOCK; break;
    default: return -1;
    }
    return ssa_sector_movers_command(&c->l->movers,index,command,c->sound) ? 0 : -1;
}
int ssa_level_runtime_step(SsaLevelRuntime *l,unsigned ticks,int32_t fallback_z,
                            const SsaLevelScriptOps *ops,const SsaSectorMoverOps *sound) {
    WorldCall c={l,ops,sound};
    SsaLevelScriptOps adapters={allocate,animation,insert,particle,mover,&c};
    int result;
    if (!valid(l) || ticks>255 || !ops || !ops->allocate || !ops->animation ||
        !ops->insert || !ops->particle) return -1;
    l->updating=1;
    result=ssa_sector_movers_step(&l->movers,ticks,sound) ?
        ssa_level_script_step(&l->script,&l->data,ticks,fallback_z,&adapters) : -1;
    l->updating=0; return result;
}
int ssa_level_runtime_movers_step(SsaLevelRuntime *l,unsigned ticks,
                                  const SsaSectorMoverOps *sound) {
    int result;
    if (!valid(l) || ticks>255) return -1;
    l->updating=1;
    result=ssa_sector_movers_step(&l->movers,ticks,sound) ? 0 : -1;
    l->updating=0; return result;
}
int ssa_level_runtime_scripts_step(SsaLevelRuntime *l,unsigned ticks,int32_t fallback_z,
                                   const SsaLevelScriptOps *ops,const SsaSectorMoverOps *sound) {
    WorldCall c={l,ops,sound};
    SsaLevelScriptOps adapters={allocate,animation,insert,particle,mover,&c};
    int result;
    if (!valid(l) || ticks>255 || !ops || !ops->allocate || !ops->animation ||
        !ops->insert || !ops->particle) return -1;
    l->updating=1;
    result=ssa_level_script_step(&l->script,&l->data,ticks,fallback_z,&adapters);
    l->updating=0; return result;
}
