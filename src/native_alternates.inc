/* PC alternate-weapon policy. The original Colt controller remains the only
 * source of ammunition use, damage and statistics. Dual firing runs two clock
 * phases while each hand's presentation retains the original animation rate. */
static void weapon_bob_update(SsaNativeGame *g)
{
    const SsaBody *body=&g->players[0].entity.body;int32_t sine,cosine,amplitude=g->player.camera.bob_amplitude;
    int64_t altitude=(int64_t)body->z-(int32_t)body->floor*65536;
    g->weapon_bob_x=g->weapon_bob_y=0;
    if(!g->weapon_bobbing || g->player.pose.third_person || g->players[0].inventory.health<=0 ||
       (g->players[0].motion.movement.flags&(8|SSA_PLAYER_CONTROL_LOCK)))return;
    if(amplitude<0)amplitude=0;
    /* The original squared-speed amplitude reaches only half strength at
     * straight walking speed8. Normalize there, so vertical motion survives
     * native pixel quantization without requiring faster diagonal movement. */
    if(amplitude>0x80000)amplitude=0x100000;
    else amplitude*=2;
    if(!body->velocity_x && !body->velocity_y)amplitude=0;
    /* Original art is the upper-left bound: only move right/down. Both
     * Colts and their flashes use the same offset, moving in parallel.
     * Sound13 is the left endpoint,14 the right, with the arch rising between. */
    ssa_sincos(&g->math,(uint16_t)(g->player.camera_frame.counter*1872u),&sine,&cosine);
    g->weapon_bob_x=(int32_t)((int64_t)(65536-cosine)*amplitude*3/(65536LL*4096));
    if(sine<0)sine=-sine;
    g->weapon_bob_y=(int32_t)((int64_t)(65536-sine)*amplitude*2/(65536LL*4096));
    /* Height-based jump dip also works from rest, eases around the apex and
     * returns continuously on landing. No extra animation/render clock. */
    if(altitude>0){if(altitude>64*65536)altitude=64*65536;
        g->weapon_bob_y+=(int32_t)(altitude/(16*256));}
}
static int alternate_hands_busy(const SsaNativeGame *g)
{
    return (g->alternates.hands[0].animation==11 && !g->alternates.hands[0].done) ||
           (g->alternates.hands[1].animation==11 && !g->alternates.hands[1].done);
}
int ssa_native_game_alternate_weapons(SsaNativeGame *g,unsigned enabled,unsigned start_mode)
{
    if(!g || !g->loaded || g->game.updating || enabled>1 || start_mode>SSA_ALTERNATE_LOADOUT)return -1;
    if(enabled!=g->alternates.enabled){
        g->alternates.variant=g->alternates.requested_variant=0;
        g->alternates.previous_variant=0;g->alternates.left_y=0xdc00;
        g->alternates.firing_left=g->alternates.next_left=0;
        memset(g->alternates.hands,0,sizeof g->alternates.hands);
        g->alternates.rounds[0]=g->alternates.rounds[1]=0;
        g->alternates.reload_phase=0;g->alternates.next_shot=g->alternates.clock;
        g->alternates.shotgun_inserting=0;
        g->weapon.object.previous_frame=0xffff;
    }
    g->alternates.enabled=enabled;
    g->weapon.alternate_shotgun=enabled;
    g->actor_scene.screen_mirrors=enabled;
    g->pickup.repeat_weapon=enabled && !g->alternates.dual_colts?2:0;
    g->pickup.collect_owned_weapons=1;
    g->weapon.fire_inhibited=g->weapon.discard_fire=g->weapon.double_colt=g->weapon.switch_blocked=0;
    g->weapon.colt_pair=0;if(!enabled)g->weapon.colt_pair_pending=0;
    if(enabled && start_mode==SSA_ALTERNATE_NEW_GAME){
        SsaInventory *inventory=&g->players[0].inventory;
        inventory->weapons|=2;
        if(ssa_inventory_add_ammo(inventory,&g->inventory_rules,2,g->inventory_rules.ammo_capacity[1]/2,1)<0)return -1;
        g->gameplay.weapon.current=g->gameplay.weapon.requested=2;
        g->gameplay.weapon.flags&=~0x3500u;
        g->players[0].motion.movement.weapon_flags=g->gameplay.weapon.flags;
        g->alternates.weapon=2;
        if(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)<0)return -1;
    }
    if(enabled && start_mode==SSA_ALTERNATE_LOADOUT && g->level && (g->players[0].inventory.weapons&2)){
        g->alternates.dual_colts=1;g->pickup.repeat_weapon=0;
        g->alternates.variant=g->gameplay.weapon.current==2;
        g->alternates.requested_variant=g->gameplay.weapon.requested==2;
        g->alternates.weapon=g->gameplay.weapon.current;
        g->alternates.left_y=g->alternates.variant?0x5000:0xdc00;
    }
    return 1;
}
static void alternate_before(SsaNativeGame *g)
{
    SsaWeaponControl *w=&g->gameplay.weapon;int32_t target;
    g->weapon.fire_inhibited=g->weapon.discard_fire=g->weapon.double_colt=g->weapon.switch_blocked=0;
    g->weapon.colt_pair=g->alternates.enabled && w->requested==2 && g->alternates.requested_variant;
    g->weapon.alternate_shotgun=g->alternates.enabled;
    if(g->alternates.dual_colts && !(g->players[0].inventory.weapons&2) && !g->players[0].inventory.pending_count){
        g->alternates.dual_colts=g->alternates.variant=g->alternates.requested_variant=0;
        g->alternates.previous_variant=0;g->alternates.left_y=0xdc00;
        g->pickup.repeat_weapon=g->alternates.enabled?2:0;
    }
    if(!g->alternates.enabled || g->frame_game.flags&1)return;
    g->alternates.clock+=(uint64_t)g->frame_game.ticks*20;
    if(g->alternates.equip_dual && (g->players[0].inventory.weapons&2)){
        (void)select_variant(g,2,1);g->alternates.equip_dual=0;
    }
    if(!g->players[0].inventory.health){
        g->alternates.rounds[0]=g->alternates.rounds[1]=0;
        g->alternates.reload_phase=0;g->alternates.next_shot=g->alternates.clock;
        g->alternates.left_y=0xdc00;return;
    }
    /* Keep the accelerated controller clock through a queued weapon change;
     * the runtime must suppress fresh shots even if selection updates there. */
    g->weapon.double_colt=w->current==2 && g->alternates.variant && g->weapon.object.y==0x5000 && !(w->flags&0x2100);
    if(w->current==2){
        unsigned manual=g->input_runtime.sample.enabled && g->input_runtime.sample.reload_pressed &&
            !(g->players[0].motion.movement.flags&8) && g->weapon.object.y==0x5000 && !(w->flags&0x2100) &&
            (g->alternates.rounds[0] || (g->alternates.variant && g->alternates.rounds[1]));
        /* Test the gun whose turn comes next, never the gun that just fired.
         * Also handle a partial magazine retained through a variant change. */
        if(!g->alternates.reload_phase && w->requested==2 &&
           g->alternates.variant==g->alternates.requested_variant &&
           (manual || g->alternates.rounds[g->alternates.variant?g->alternates.next_left:0]>=6)){
            g->alternates.reload_phase=1;g->alternates.reload_dual=g->alternates.variant;
        }
        if(g->alternates.reload_phase){
            g->weapon.discard_fire=1; /* Includes the tick which completes reload. */
            g->weapon.fire_inhibited=g->weapon.switch_blocked=1;
            if(g->alternates.reload_phase==1){
                if(!(w->flags&0x400) && !alternate_hands_busy(g)){
                    g->alternates.reload_phase=2;
                    g->alternates.reload_end=g->alternates.clock+1284;
                    (void)sound(g,0x28); /* Original weapon-mechanism click, local/centered. */
                }
            }else{
                /* Start the left0.33s after the right, preserving1.07s per
                 * hand while completing the pair in1.40s. reload_end always
                 * names the right hand's deadline, including during overlap. */
                uint64_t end=g->alternates.reload_end+(g->alternates.reload_dual?396:0);
                if(g->alternates.reload_phase==2 && g->alternates.reload_dual &&
                   g->alternates.clock>=g->alternates.reload_end-888){
                    g->alternates.reload_phase=3;
                    (void)sound(g,0x28);
                }
                if(g->alternates.clock>=g->alternates.reload_end)g->alternates.rounds[0]=0;
                if(g->alternates.clock>=end){
                    if(g->alternates.reload_dual)g->alternates.rounds[1]=0;
                    g->alternates.reload_phase=0;
                    g->alternates.next_left=0;g->alternates.next_shot=end;
                }
            }
            if(g->alternates.reload_phase)return;
            g->weapon.fire_inhibited=g->weapon.switch_blocked=0;
        }
        if(g->alternates.clock<g->alternates.next_shot)g->weapon.fire_inhibited=1;
    }
    if(w->current==2 && g->alternates.variant){
        if(alternate_hands_busy(g))g->weapon.switch_blocked=1;
        if(w->requested!=2)g->weapon.fire_inhibited=1;
    }
    if(w->requested!=2)g->alternates.requested_variant=0;
    if(w->current!=2 || w->requested!=2)return;
    if(g->alternates.variant==g->alternates.requested_variant &&
       g->alternates.left_y==(g->alternates.variant?0x5000:0xdc00)){
        return;
    }
    g->weapon.fire_inhibited=1;
    if((w->flags&0x400) || alternate_hands_busy(g))return;
    target=g->alternates.requested_variant?0x5000:0xdc00;
    if(g->alternates.left_y<target){g->alternates.left_y+=0x800;if(g->alternates.left_y>target)g->alternates.left_y=target;}
    else if(g->alternates.left_y>target){g->alternates.left_y-=0x800;if(g->alternates.left_y<target)g->alternates.left_y=target;}
    if(g->alternates.left_y==target){
        if(g->alternates.requested_variant)g->alternates.rounds[1]=0;
        g->alternates.variant=g->alternates.requested_variant;
        g->alternates.next_left=g->alternates.firing_left=0;
    }
}
static void alternate_after(SsaNativeGame *g)
{
    unsigned current=g->gameplay.weapon.current;
    if(!(g->frame_game.flags&1)){
        unsigned was_inserting=g->alternates.shotgun_inserting;
        g->alternates.shotgun_inserting=g->alternates.enabled && current==3 &&
            g->players[0].inventory.health>0 && g->weapon.shotgun_animating &&
            g->weapon.object.animation==12 && (g->weapon.object.frame&0x3fff)==23;
        /* Sprite23 holds across several timed slots. Cue only its first
         * visible tick, after animation advances, as a local weapon effect. */
        if(g->alternates.shotgun_inserting && !was_inserting)(void)sound(g,0x28);
    }
    if(g->alternates.weapon!=current){
        g->alternates.weapon=current;
        g->alternates.variant=current==2 && g->alternates.enabled?g->alternates.requested_variant:0;
        g->alternates.left_y=g->alternates.variant?0x5000:0xdc00;
        if(g->alternates.enabled && current==2){
            g->alternates.rounds[0]=0;
            if(g->alternates.variant)g->alternates.rounds[1]=0;
            g->alternates.reload_phase=0;
        }
        g->alternates.firing_left=g->alternates.next_left=0;
        memset(g->alternates.hands,0,sizeof g->alternates.hands);
    }
    if(g->alternates.enabled && current==2 && !(g->frame_game.flags&1)){
        unsigned i;for(i=0;i<2;++i)if(g->alternates.hands[i].animation==11 && !g->alternates.hands[i].done)
            (void)ssa_weapon_animation_step(&g->alternates.hands[i],&g->weapon_animations,0,g->frame_game.ticks);
    }
}
static int32_t alternate_reload_offset(const SsaNativeGame *g,unsigned hand)
{
    uint64_t end,remaining,elapsed,lower;
    if(g->alternates.reload_phase<2 || (hand && g->alternates.reload_phase!=3))return 0;
    end=g->alternates.reload_end+(hand?396:0);
    if(g->alternates.clock>=end || g->alternates.clock+1284<end)return 0;
    remaining=end-g->alternates.clock;
    elapsed=1284-remaining;lower=elapsed<360?elapsed:remaining<360?remaining:360;
    return (int32_t)(lower*0x8c00/360);
}
static int build_oam_weapons(SsaNativeGame *g,SsaSpriteOam *out,SsaSpriteDispatch *sprites,int32_t bob_x,int32_t bob_y)
{
    SsaSpriteDispatch copy;SsaWeaponObject objects[84];SsaSpriteCacheEntry cache[SSA_SPRITE_IMAGE_COUNT];
    unsigned i,slot=84,free_slot=84,id,used[84]={0},left_visible;uint16_t idle_frame;int result;
    if(!g || !out || !sprites)return -1;
    if(!g->alternates.enabled || g->gameplay.weapon.current!=2 || !g->players[0].inventory.health){
        if(!bob_x && !bob_y)
            return ssa_sprite_oam_build(out,sprites,&g->sprite_assets,g->cache.entries,&g->math);
        copy=*sprites;
        for(i=0;i<84;++i){objects[i]=*sprites->objects[i];copy.objects[i]=&objects[i];}
        slot=g->weapon.object.slot;if(slot>=84)return -1;
        objects[slot].x+=bob_x;objects[slot].y+=bob_y;
        result=ssa_sprite_oam_build(out,&copy,&g->sprite_assets,g->cache.entries,&g->math);
        if(result==1)for(i=0;i<84;++i){sprites->objects[i]->screen_x=objects[i].screen_x;
            sprites->objects[i]->screen_y=objects[i].screen_y;}
        return result;
    }
    left_visible=g->alternates.left_y<0xdc00;
    if(left_visible && sprites->active_count>=84)return -1;
    for(i=0;i<sprites->active_count;++i){unsigned index=sprites->active[i];
        if(index>=84)return -1;
        used[index]=1;
        if(index==g->weapon.object.slot)slot=index;
    }
    if(slot==84)return ssa_sprite_oam_build(out,sprites,&g->sprite_assets,g->cache.entries,&g->math);
    for(i=0;i<84;++i)if(!used[i]){free_slot=i;break;}
    if(left_visible && free_slot==84)return -1;
    idle_frame=g->weapon_animations.words[g->weapon_animations.offsets[1]];
    id=idle_frame&0x3fff;if(id>=SSA_SPRITE_IMAGE_COUNT)return -1;
    if(ssa_sprite_cache_flush(&g->cache,&g->sprite_assets)<0)return -1;
    memcpy(cache,g->cache.entries,sizeof cache);
    copy=*sprites;
    for(i=0;i<84;++i){objects[i]=*sprites->objects[i];copy.objects[i]=&objects[i];}
    if(left_visible){
        objects[free_slot]=objects[slot];objects[free_slot].slot=(uint16_t)free_slot;
        objects[free_slot].active_index=(uint16_t)copy.active_count;
        copy.active[copy.active_count++]=free_slot;
        /* Reflect the complete original art around the240px canvas center. */
        objects[free_slot].x=240*256-objects[slot].x;
        objects[free_slot].y=objects[slot].y+g->alternates.left_y-0x5000;
        objects[free_slot].frame=objects[free_slot].previous_frame=idle_frame;
    }
    if(g->alternates.variant){
        objects[slot].frame=objects[slot].previous_frame=g->alternates.hands[0].animation==11 && !g->alternates.hands[0].done?g->alternates.hands[0].frame:idle_frame;
        if(left_visible)objects[free_slot].frame=objects[free_slot].previous_frame=g->alternates.hands[1].animation==11 && !g->alternates.hands[1].done?g->alternates.hands[1].frame:idle_frame;
    }
    objects[slot].y+=alternate_reload_offset(g,0);
    if(left_visible)objects[free_slot].y+=alternate_reload_offset(g,1);
    /* Both independently animated hands fit inside the160 reserved weapon
     * tiles (Colt frames use at most45 each). Finish queued uploads first. */
    for(i=0;i<(left_visible?2u:1u);++i){SsaWeaponObject *hand=&objects[i?free_slot:slot];
        const SsaSpriteImage *im;unsigned bank=i?80:0;id=hand->previous_frame&0x3fff;
        if(id>=SSA_SPRITE_IMAGE_COUNT)return -1;
        if(i && id==(objects[slot].previous_frame&0x3fff))continue;
        im=&g->sprite_assets.images[id];if(!im->tiles || im->tile_count>80)return -1;
        memcpy(g->cache.tiles+bank*64,im->tiles,(size_t)im->tile_count*64);
        cache[id].tile=(uint16_t)bank;cache[id].resident=3;
    }
    if(left_visible){
        objects[free_slot].frame^=0x8000;objects[free_slot].previous_frame^=0x8000;
        objects[free_slot].x+=bob_x;objects[free_slot].y+=bob_y;
    }
    objects[slot].x+=bob_x;objects[slot].y+=bob_y;
    result=ssa_sprite_oam_build(out,&copy,&g->sprite_assets,cache,&g->math);
    if(result==1)for(i=0;i<sprites->active_count;++i){unsigned index=sprites->active[i];
        sprites->objects[index]->screen_x=objects[index].screen_x;
        sprites->objects[index]->screen_y=objects[index].screen_y;
    }
    return result;
}
static unsigned original_hud_group(const SsaWeaponObject *o)
{
    if(o->type==9 || o->type==12)return 1; /* Original top-center score/clock. */
    if(o->type==16 || o->type==17 || (o->type==4 && (o->animation==0x5b ||
       (o->animation>=0xdb && o->animation<=0xde))))return 2; /* Top-left lives. */
    if(o->type==4 || o->type==15 || o->type==5)return 3;
    if(o->type==7)return 4; /* Top-right weapon icon. */
    if(o->type==6 || o->type==8)return 5; /* Bottom-right ammo. */
    return 0; /* Weapon models, reticle and messages. */
}
static int build_original_hud(SsaNativeGame *g,SsaSpriteOam *out,SsaSpriteDispatch *sprites)
{
    SsaSpriteDispatch copy=*sprites;SsaWeaponObject objects[84];SsaSpriteOam layers[6];unsigned i,group;
    int result;
    for(i=0;i<84;++i){if(!sprites->objects[i])return -1;
        objects[i]=*sprites->objects[i];copy.objects[i]=&objects[i];}
    /* Build isolated snapshots so output-space anchoring never moves a piece
     * of a first-person weapon that happens to overlap a corner HUD panel.
     * All source coordinates and transparent sprite padding remain original;
     * native240x160 OAM is never normalized to a new margin or rearranged. */
    for(group=0;group<6;++group){SsaWeaponObject filtered[84];SsaSpriteDispatch d=copy;
        layers[group]=*out;
        for(i=0;i<84;++i){filtered[i]=objects[i];d.objects[i]=&filtered[i];
            if(original_hud_group(&filtered[i])!=group)filtered[i].flags&=~UINT32_C(1);}
        if(build_oam_weapons(g,&layers[group],&d,g->weapon_bob_x,g->weapon_bob_y)!=1)return -1;
    }
    result=build_oam_weapons(g,out,&copy,g->weapon_bob_x,g->weapon_bob_y);
    if(result==1 && out==&g->oam)memcpy(g->original_hud_oam,layers,sizeof layers);
    if(result==1)for(i=0;i<84;++i){sprites->objects[i]->screen_x=objects[i].screen_x;
        sprites->objects[i]->screen_y=objects[i].screen_y;}
    return result;
}
/* Keep the original crop/scale, but move its pixels in output space. The
 * source weapon canvas is unshifted so bobbing cannot clip at its3:2 edge. */
static int build_pc_weapon_layers(SsaNativeGame *g)
{
    SsaSpriteDispatch copy=g->sprites;SsaWeaponObject objects[84];unsigned i,layer;
    g->pc_weapon_separate=0;
    if(!g->weapon_bob_x && !g->weapon_bob_y)return 1;
    for(layer=0;layer<(g->hud_style==SSA_HUD_ORIGINAL?3u:2u);++layer){
        SsaSpriteOam *o=layer==0?&g->pc_weapon_oam:layer==1?&g->pc_hud_oam:&g->pc_center_hud_oam;
        *o=g->oam;
        for(i=0;i<84;++i){objects[i]=*g->sprites.objects[i];copy.objects[i]=&objects[i];
            if(layer==0?i!=g->weapon.object.slot:i==g->weapon.object.slot ||
               (g->hud_style==SSA_HUD_CLASSIC && ssa_classic_hud_replaces(objects[i].type)) ||
               (layer==2 && original_hud_group(&objects[i])))objects[i].flags&=~UINT32_C(1);}
        if((layer==0?build_oam_weapons(g,o,&copy,0,0):
            ssa_sprite_oam_build(o,&copy,&g->sprite_assets,g->cache.entries,&g->math))!=1)return -1;
    }
    g->pc_weapon_separate=1;return 1;
}
int ssa_native_game_build_oam(SsaNativeGame *g,SsaSpriteOam *out,SsaSpriteDispatch *sprites)
{
    SsaSpriteDispatch copy;SsaWeaponObject objects[84];unsigned i;int result;
    if(!g || !sprites || sprites->active_count>84)return -1;
    weapon_bob_update(g);
    if(g->hud_style!=SSA_HUD_CLASSIC){result=build_original_hud(g,out,sprites);
        if(result==1 && out==&g->oam && sprites==&g->sprites)result=build_pc_weapon_layers(g);
        return result;}
    copy=*sprites;
    for(i=0;i<84;++i){if(!sprites->objects[i])return -1;
        objects[i]=*sprites->objects[i];copy.objects[i]=&objects[i];
        if(ssa_classic_hud_replaces(objects[i].type))objects[i].flags&=~UINT32_C(1);}
    result=build_oam_weapons(g,out,&copy,g->weapon_bob_x,g->weapon_bob_y);
    if(result==1)for(i=0;i<84;++i){sprites->objects[i]->screen_x=objects[i].screen_x;
        sprites->objects[i]->screen_y=objects[i].screen_y;}
    if(result==1 && out==&g->oam && sprites==&g->sprites)result=build_pc_weapon_layers(g);
    return result;
}
