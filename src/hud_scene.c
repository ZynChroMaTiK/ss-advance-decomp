#include "ssa/hud_scene.h"
#include <string.h>
static int ready(const SsaHudScene *s)
{
    unsigned i,weapon_bound=0;uint8_t seen[84]={0};
    if(!(s && s->loaded==1 && s->assets && s->assets->loaded==1 && s->math &&
        s->pool.loaded==1 && s->pool.dispatch==&s->dispatch && s->cache.loaded==1 &&
        s->oam.loaded==1 && s->weapon.inventory && s->weapon.control==&s->control &&
        s->weapon.object==s->weapon_object && s->weapon.source && s->dispatch.active_count<=84))return 0;
    for(i=0;i<84;++i){
        if(s->dispatch.objects[i]!=&s->objects[i])return 0;
        if(s->weapon_object==&s->objects[i])weapon_bound=1;
    }
    if(!weapon_bound)return 0;
    for(i=0;i<s->dispatch.active_count;++i){unsigned slot=s->dispatch.active[i];
        if(slot>=84 || seen[slot])return 0;
        seen[slot]=1;
    }
    return 1;
}
static int busy(const SsaHudScene *s)
{return s->updating || s->dispatch.updating || s->pool.updating ||
 s->cache.updating || s->oam.updating || s->weapon.updating || s->loadout.updating;}
static int allocate(void *context,unsigned player,SsaWeaponObject **out)
{return player?-1:ssa_sprite_pool_allocate(&((SsaHudScene *)context)->pool,out)==1?1:-1;}
int ssa_hud_scene_init(SsaHudScene *s,SsaInventory *inventory,SsaEntity *body,
    const SsaSpriteAssets *assets,const SsaMathTables *math,const void *rom,size_t size,
    unsigned level,unsigned setting)
{
    SsaWeaponLoadoutPlayer player;SsaWeaponLoadoutOps ops={allocate,s};unsigned i;
    if(!s || s->loaded || s->updating || s->assets || !inventory || !body ||
       !assets || assets->loaded!=1 || !math || !rom || level>15 || setting>36)return -1;
    memset(s,0,sizeof(*s));s->assets=assets;s->math=math;
    for(i=0;i<84;++i)s->dispatch.objects[i]=&s->objects[i];
    if(ssa_weapon_animations_load(&s->animations,rom,size)!=1 ||
       ssa_weapon_loadouts_load(&s->loadouts,rom,size)!=1 ||
       ssa_inventory_rules_load(&s->inventory_rules,rom,size)<0 ||
       ssa_hud_glyphs_load(&s->state,rom,size)!=1 ||
       ssa_sprite_pool_init(&s->pool,&s->dispatch)!=1 ||
       ssa_sprite_cache_init(&s->cache)!=1 ||
       ssa_sprite_oam_init(&s->oam,rom,size)!=1 ||
       ssa_display_registers_default(&s->registers)!=1)goto fail;
    s->control.keys=4;
    player=(SsaWeaponLoadoutPlayer){inventory,&s->control,NULL,s->weapon.cooldown};
    if(ssa_weapon_loadout_initialize(&s->loadout,&player,1,&s->loadouts,
        &s->inventory_rules,setting,0,&ops)!=1)goto fail;
    s->weapon_object=player.object;s->weapon.inventory=inventory;
    s->weapon.control=&s->control;s->weapon.object=player.object;s->weapon.source=body;
    if(ssa_hud_sprites_create(&s->pool,1,0,level)!=74)goto fail;
    s->loaded=1;return 1;
fail:
    memset(s,0,sizeof(*s));return -1;
}
int ssa_hud_scene_free(SsaHudScene *s)
{if(!ready(s) || busy(s))return -1;memset(s,0,sizeof(*s));return 1;}
int ssa_hud_scene_step_idle(SsaHudScene *s,unsigned ticks,unsigned input_state,unsigned third)
{
    unsigned i;int result=-1;
    if(!ready(s) || busy(s) || ticks>255)return -1;
    s->updating=1;s->dispatch.counter++;
    memcpy(s->state.ammo,s->weapon.inventory->ammo,sizeof(s->state.ammo));
    if(!(s->state.game_flags&1)) {
        if((s->state.local_flags&8) || third)s->weapon_object->flags&=~UINT32_C(1);
        else s->weapon_object->flags|=1;
        if(input_state!=3 && ssa_weapon_controller_switch(&s->weapon,(uint8_t)ticks)<0)goto done;
    }
    /* Weapon slot runs before the HUD in original allocation order. */
    s->state.current_weapon=s->control.requested;
    s->state.weapon_action=s->control.current;
    s->state.weapon_flags=s->control.flags;
    for(i=0;i<s->dispatch.active_count;++i) {
        SsaWeaponObject *o=s->dispatch.objects[s->dispatch.active[i]];
        if(o==s->weapon_object)continue;
        if(ssa_hud_sprite_step(o,&s->state)!=1)goto done;
    }
    memcpy(s->weapon.inventory->ammo,s->state.ammo,sizeof(s->state.ammo));
    for(i=0;i<s->dispatch.active_count;++i)
        if(ssa_weapon_animation_step(s->dispatch.objects[s->dispatch.active[i]],
            &s->animations,s->state.game_flags,ticks)<0)goto done;
    if(ssa_sprite_cache_prepare(&s->cache,&s->dispatch,s->assets,0)<0)goto done;
    s->oam.counter=s->dispatch.counter;
    if(ssa_sprite_oam_build(&s->oam,&s->dispatch,s->assets,s->cache.entries,s->math)<0 ||
       ssa_sprite_cache_flush(&s->cache,s->assets)<0)goto done;
    result=1;
done:s->updating=0;return result;
}
int ssa_hud_scene_draw(SsaHudScene *s,const uint8_t *world,size_t size,unsigned family,
    uint16_t *out,size_t pixels)
{
    int result;
    if(!ready(s) || busy(s) || family>1)return -1;
    s->updating=1;
    result=ssa_display_compose(&s->registers,world,size,s->assets->background_palette[family],
        s->assets->palette,&s->oam,s->cache.tiles,sizeof(s->cache.tiles),out,pixels);
    s->updating=0;return result;
}
