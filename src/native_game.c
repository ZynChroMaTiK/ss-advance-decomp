#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "ssa/pc_scene.h"
#include "ssa/settings.h"
#include <stdlib.h>
#include "ssa/weapon_progress.h"
#include <stdio.h>
#include <string.h>

#include "native_audio.inc"
static int select_variant(SsaNativeGame *,unsigned,unsigned);
static void hud_health_update(SsaNativeGame *g)
{
    int16_t health=g->players[0].inventory.health;
    if(health<g->hud_health_before)g->hud_health_flash=40;
    g->hud_health_before=health;
}
static void alternate_before(SsaNativeGame *);
static void alternate_after(SsaNativeGame *);
#include "native_alternates.inc"
static void music(SsaNativeGame *g,unsigned command)
{
    if(g->music_mode==command)return;
    g->music_mode=command;++g->music_serial;
    if(g->audio_ops.music)g->audio_ops.music(g->audio_ops.context,command,g->level);
}
int ssa_native_game_bind_audio(SsaNativeGame *g,const SsaNativeAudioOps *ops)
{
    if(!g || !g->loaded || g->game.updating || (ops && (!ops->play || !ops->stop)))return -1;
    audio_unbind(g);
    if(ops){g->audio_ops=*ops;audio_listener(g);
        if(g->music_mode!=2 && g->audio_ops.music)g->audio_ops.music(g->audio_ops.context,g->music_mode,g->level);
    }else memset(&g->audio_ops,0,sizeof(g->audio_ops));
    return 1;
}
static void notice(SsaNativeGame *g,const char *text)
{snprintf(g->notice,sizeof(g->notice),"%s",text);g->notice_ticks=180;++g->notice_serial;}
static int original_notice(SsaNativeGame *g,unsigned id,unsigned value,unsigned y,unsigned font)
{
    g->hud.game_flags=g->frame_game.flags;
    if(ssa_hud_notice_set(&g->hud_notice,&g->hud,id,value,y,font)<0){g->error_line=__LINE__;return -1;}
    g->frame_game.flags=g->hud.game_flags;
    g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=(uint16_t)g->hud.notice_timer;
    ++g->notice_serial;return 0;
}
static int particle(SsaNativeGame *g,unsigned id,int32_t x,int32_t y,int32_t z)
{
    uint8_t occupied[100];unsigned i;int result;
    if(!g->alternates.enabled)return ssa_projectile_runtime_particle_held(&g->projectiles,id,x,y,z);
    memcpy(occupied,g->spawn.pool.occupied,sizeof occupied);
    result=ssa_projectile_runtime_particle_held(&g->projectiles,id,x,y,z);
    for(i=0;i<100;++i)if(!occupied[i] && g->spawn.pool.occupied[i])
        g->alternates.flash_left[i]=(uint8_t)(id==8 && g->alternates.firing_left);
    return result;
}
static int particle_sound(void *p,const SsaProjectileRuntime *r,const SsaProjectile *e,unsigned id)
{(void)r;return world_sound(p,id,e->entity);}
static int combat_event(void *p,const SsaCombatRuntime *r,const SsaCombatDamageTarget *v,
 SsaCombatDamageEvent kind,uint32_t a,uint32_t b)
{
    (void)r;(void)b;
    if(kind==SSA_COMBAT_DAMAGE_SOUND){world_sound(p,a,v->entity);return 0;}
    return -1;
}
static int player_event(void *p,SsaPlayerGameplay *player,enum SsaPlayerLifecycleEvent kind,
 int32_t a,int32_t b,int32_t c,int32_t d)
{
    SsaNativeGame *g=p;
    if(kind==SSA_LIFECYCLE_WEAPON_ANIMATION)return ssa_weapon_runtime_respawn(&g->weapon,(uint16_t)a,(uint16_t)b);
    if(kind==SSA_LIFECYCLE_EFFECT)return particle(g,(unsigned)a,b,c,d);
    if(kind==SSA_LIFECYCLE_NOTICE)return original_notice(g,(unsigned)a,(unsigned)b,(unsigned)c,(unsigned)d);
    if(kind==SSA_LIFECYCLE_MODE){(void)player;music(g,(unsigned)a);return 0;}
    return -1;
}
static int player_animation(void *p,SsaPlayerRuntime *r,unsigned id,unsigned period)
{
    SsaNativeGame *g=p;
    if(id==0x68)g->player_death_facing=r->player->motion.facing;
    return ssa_actor_animation_set(&g->player_animation,(uint16_t)id,(uint16_t)period)<0?-1:0;
}
static int player_sound(void *p,SsaPlayerRuntime *r,unsigned id)
{
    SsaNativeGame *g=p;
    /* Player line interactions forward mover audio through this callback.
     * Keep it on the mover's source: a local PLAY would leave a second tied
     *67 sequence alive after the mover-owned loop is stopped. */
    if(id==0x67 || id==0x68)return mover_sound(g,id);
    if(id==13 || id==14){
        const SsaBody *b=&r->player->entity.body;
        int64_t dx=(int64_t)b->x-g->footstep_x,dy=(int64_t)b->y-g->footstep_y;
        /* Retain the recovered camera's left/right cadence, but only emit
         * walking sounds after actual grounded movement (at least1/16 unit).
         * In particular, jump bob and pushing into a wall are not footsteps. */
        if(!r->player->inventory.health || !(r->player->motion.movement.keys&0x3c0) ||
           b->z!=(int32_t)b->floor*65536 ||
           g->audio_player_before.z>(int32_t)g->audio_player_before.floor*65536 ||
           (dx> -4096 && dx<4096 && dy> -4096 && dy<4096))return 0;
    }
    sound(g,id);return 0;
}
static int cd_event(void *p,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,SsaEntity *other,
 int32_t a,int32_t b,int32_t d)
{
    (void)other;(void)b;(void)d;
    if(kind==SSA_ENEMY_CD_ANIMATION)return 0; /* metadata already committed */
    if((kind==SSA_ENEMY_CD_SOUND || kind==SSA_ENEMY_CD_STOP_SOUND) && a==0x3a && ((SsaNativeGame *)p)->audio_ops.source)return 0;
    if(kind==SSA_ENEMY_CD_SOUND){world_sound(p,(unsigned)a,cd->activation->entity);return 0;}
    if(kind==SSA_ENEMY_CD_STOP_SOUND){world_stop(p,(unsigned)a,cd->activation->entity);return 0;}
    return -1;
}
static int enemy_event(void *p,const SsaEnemyControl *c,const SsaEnemyControlEvent *e)
{
    if(e->kind==SSA_ENEMY_CONTROL_ANIMATION)return 0;
    if((e->kind==SSA_ENEMY_CONTROL_SOUND || e->kind==SSA_ENEMY_CONTROL_STOP_SOUND) &&
       e->args[0]==0x4e && c->activation->entity->type==0xce && ((SsaNativeGame *)p)->audio_ops.source)return 0;
    if(e->kind==SSA_ENEMY_CONTROL_SOUND){world_sound(p,(unsigned)e->args[0],c->activation->entity);return 0;}
    if(e->kind==SSA_ENEMY_CONTROL_STOP_SOUND){world_stop(p,(unsigned)e->args[0],c->activation->entity);return 0;}
    if(e->kind==SSA_ENEMY_CONTROL_MUSIC){music(p,(unsigned)e->args[0]);return 0;}
    if(e->kind==SSA_ENEMY_CONTROL_NOTICE)return original_notice(p,(unsigned)e->args[0],(unsigned)e->args[1],(unsigned)e->args[2],(unsigned)e->args[3]);
    return -1;
}
static int script_particle(void *p,const SsaScriptSpawn *s,unsigned id,int32_t x,int32_t y,int32_t z)
{(void)s;return particle(p,id,x,y,z);}
static int script_event(void *p,const SsaScriptSpawn *s,unsigned slot,SsaEnemyActivationEvent kind,
 int32_t a,int32_t b,int32_t c,int32_t d)
{
    if(kind==SSA_ENEMY_ACTIVATION_SOUND){world_sound(p,(unsigned)a,slot<100?&s->actors[slot].entity:NULL);return 0;}
    if(kind==SSA_ENEMY_ACTIVATION_MUSIC){music(p,(unsigned)a);return 0;}
    if(kind==SSA_ENEMY_ACTIVATION_ANIMATION)return 0;
    if(kind==SSA_ENEMY_ACTIVATION_NOTICE)return original_notice(p,(unsigned)a,(unsigned)b,(unsigned)c,(unsigned)d);
    return -1;
}
static int pickup_particle(void *p,const SsaPickupRespawnRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z)
{(void)r;return particle(p,id,x,y,z);}
static void pickup_event(void *p,const SsaPickupRuntime *r,unsigned player,
 enum SsaPickupEvent kind,unsigned a,unsigned b)
{
    (void)r;(void)player;(void)b;
    if(kind==SSA_PICKUP_SOUND)sound(p,a);
    else if(kind==SSA_PICKUP_MESSAGE){SsaNativeGame *g=p;unsigned i,colts=0;
        if(g->alternates.enabled && !g->alternates.dual_colts && r->current.item==0x20){
            for(i=0;i<g->players[0].inventory.pending_count;++i)
                colts+=g->players[0].inventory.pending_weapons[i]==2;
            if((g->players[0].inventory.weapons&2) || colts>=2){
                g->alternates.dual_colts=g->alternates.equip_dual=1;g->pickup.repeat_weapon=0;
            }
        }
        (void)original_notice(p,a,0,48,0);
    }
}
static int projectile_particle(void *p,const SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z)
{(void)r;return particle(p,id,x,y,z);}
static int weapon_event(void *p,const SsaWeaponRuntime *w,SsaWeaponEvent *e)
{
    SsaNativeGame *g=p;(void)w;
    if(e->kind==SSA_WEAPON_SOUND_PLAY){e->result=weapon_sound(g,(unsigned)e->args[0]);return 0;}
    if(e->kind==SSA_WEAPON_SOUND_STOP){stop_sound(g,(unsigned)e->args[0]);return 0;}
    if(e->kind==SSA_WEAPON_PARTICLE){
        if(e->args[0]==8){
            g->alternates.firing_left=g->alternates.enabled && g->alternates.variant?g->alternates.next_left:0;
            if(g->alternates.enabled && g->alternates.variant)g->alternates.next_left^=1;
            if(g->alternates.enabled){
                SsaWeaponObject *hand=&g->alternates.hands[g->alternates.firing_left];
                *hand=g->weapon.object;
                (void)ssa_weapon_animation_set(hand,11,g->weapon_rules.period[1]);
                (void)ssa_weapon_animation_resolve(hand,&g->weapon_animations);
                ++g->alternates.shot_serial;
                if(g->alternates.next_shot+20<g->alternates.clock)
                    g->alternates.next_shot=g->alternates.clock;
                g->alternates.next_shot+=g->alternates.variant?270:540;
                ++g->alternates.rounds[g->alternates.firing_left];
                if(g->alternates.rounds[g->alternates.variant?g->alternates.next_left:0]>=6){
                    g->alternates.reload_phase=1;
                    g->alternates.reload_dual=g->alternates.variant;
                }
                /* A second controller phase may finish this shot, but must
                 * not begin another before the next clock deadline. */
                g->weapon.fire_inhibited=1;
            }
        }
        return particle(g,(unsigned)e->args[0],e->args[1],e->args[2],e->args[3]);
    }
    if(e->kind==SSA_WEAPON_REQUEST && ((uint32_t)e->args[0]==0x80000009 || (uint32_t)e->args[0]==0x40000009)){
        g->damage.hurt_flash=(uint32_t)e->args[0];return 0;
    }
    return -1;
}
static int impact_particle(void *p,unsigned id,int32_t x,int32_t y,int32_t z)
{return particle(p,id,x,y,z);}
static int weapon_impact(void *p,const SsaWeaponRuntime *w,const SsaWeaponHit *h)
{
    SsaNativeGame *g=p;SsaWeaponImpactOps ops={impact_particle,p};
    return ssa_weapon_impact(&g->impact,w->controller.source,g->weapon_game.motion_index,
        g->gameplay.weapon.current,h->target!=NULL,h->x,h->y,h->z,&ops);
}
/* Presentation-only exact contact; never reuse the gameplay aim workspace. */
static int sight_target(SsaNativeGame *g,int32_t *x,int32_t *y,int32_t *height)
{
    SsaEntity *source=&g->players[0].entity;SsaWeaponHit hit;
    int32_t slope=g->weapon.controller.aim_slope,z=96*65536;
    uint16_t heading=(uint16_t)((unsigned)g->players[0].motion.facing<<8);
    if(g->player.free_aim){int32_t sine,cosine;
        ssa_sincos(&g->math,(uint16_t)g->player.pitch,&sine,&cosine);
        slope=(int32_t)((int64_t)sine*65536/cosine);
        heading=g->players[0].motion.movement.heading;z=g->player.camera.eye_z-source->body.z;
    }
    if(ssa_weapon_hit_query_unlimited(&g->camera_trace,source,heading,z,slope,&hit,1)<0)return -1;
    if(hit.impact){*x=hit.x;*y=hit.y;*height=hit.z;}
    else {int64_t h=(int64_t)g->camera_trace.shoot_z+(int64_t)g->camera_trace.range*slope/65536;
        *x=g->camera_trace.end_x;*y=g->camera_trace.end_y;
        *height=h>INT32_MAX?INT32_MAX:h<INT32_MIN?INT32_MIN:(int32_t)h;
    }
    return 1;
}
static int gba_cursor(SsaNativeGame *g,SsaWeaponObject *object)
{
    SsaBody still=g->players[0].entity.body;
    SsaPlayerVertical vertical=g->players[0].vertical;
    SsaCamera eye={0};SsaCameraFrame frame={0};
    int32_t x,y,z,sine,cosine;int64_t depth,scale,screen,eye_z;
    if(sight_target(g,&x,&y,&z)<0)return -1;
    /* Use a non-bobbing eye for the HUD, retaining the actual chase-camera
     * offset. Reuse original lowering, stair and clamp handling on copies;
     * eye recovery has already advanced this frame and must not advance twice. */
    still.velocity_x=still.velocity_y=0;vertical.flags&=(uint16_t)~4u;
    (void)ssa_camera_step(&still,&vertical,&eye,&frame,&g->math);
    eye_z=(int64_t)g->player.pose.z+eye.eye_z-g->player.camera.eye_z;
    ssa_sincos(&g->math,g->players[0].motion.movement.heading,&sine,&cosine);
    depth=(((int64_t)x-g->player.pose.x)*sine+((int64_t)y-g->player.pose.y)*cosine)/65536;
    if(depth<=0){object->y=0x5000;return 1;}
    /* D490's distance table and vertical projection, then the reference
     * display's120-to160 scaling. Keep8.8 precision until OAM placement. */
    if(g->gba_view){
        if(depth/65536>65535)depth=INT64_C(65535)*65536;
        scale=((int64_t)0x55b05a*(g->actor_assets.reciprocal[depth/65536]>>8)>>16)>>8;
        screen=80*256-((int64_t)z-eye_z)*(scale+scale/2)/(16*65536*12);
    }
    else screen=80*256-(int64_t)(((double)z-eye_z)/depth*(2.0*0x55b05a/65536.0)*ssa_pc_scene_fov_scale(g->fov_offset)*256);
    object->y=(int32_t)(screen<2*256?2*256:screen>157*256?157*256:screen);
    return 1;
}
static void hud_state(SsaNativeGame *g)
{
    SsaInventory *i=&g->players[0].inventory;unsigned slot;
    g->hud.boss_present=0;g->hud.boss_health=0;
    if(g->spawn.game.boss)for(slot=0;slot<100;++slot)
        if(g->spawn.actors[slot].owned && &g->spawn.actors[slot].entity==g->spawn.game.boss){
            g->hud.boss_present=1;g->hud.boss_health=g->spawn.actors[slot].activation.health;break;}
    g->hud.local_flags=g->players[0].motion.movement.flags;
    g->hud.current_weapon=g->gameplay.weapon.requested;g->hud.weapon_action=g->gameplay.weapon.current;
    g->hud.weapon_flags=g->gameplay.weapon.flags;g->hud.aim_marker=g->weapon.controller.aim.marker;
    g->hud.health=i->health;g->hud.armor=i->stat_c4;
    g->hud.score=g->score;g->hud.time=g->frame_game.elapsed;g->hud.game_flags=g->frame_game.flags;
    g->hud.game_value=g->game.life_game.continue_flag;
    g->hud.notice_timer=(int16_t)g->game.life_game.message_ticks;
    memcpy(g->hud.ammo,i->ammo,sizeof(g->hud.ammo));
}
static int sprite_handler(void *p,SsaSpriteDispatch *d,uint32_t entry,unsigned slot)
{
    SsaNativeGame *g=p;int result;(void)entry;
    if(d!=&g->sprites || slot>=84)return -1;
    hud_state(g);
    result=d->objects[slot]->type==13?
        ssa_hud_notice_step(d->objects[slot],&g->hud,&g->hud_notice,&g->weapon_animations,g->frame_game.ticks):
        ssa_hud_sprite_step(d->objects[slot],&g->hud);
    if(result==1 && (g->gba_view || !g->vertical_aim) && d->objects[slot]->type==18 && (d->objects[slot]->flags&1))
        result=gba_cursor(g,d->objects[slot]);
    g->frame_game.flags=g->hud.game_flags;
    g->game.life_game.message_ticks=g->gameplay.life_game.message_ticks=(uint16_t)g->hud.notice_timer;
    memcpy(g->players[0].inventory.ammo,g->hud.ammo,sizeof(g->hud.ammo));
    return result==1?0:-1;
}
static int external(void *p,SsaGameRuntime *r,SsaFrameStage stage)
{
    SsaNativeGame *g=p;
    if(stage==SSA_FRAME_POLL)return ssa_pc_input_runtime_poll(&g->input_runtime);
    if(stage==SSA_FRAME_DISTRIBUTE)return ssa_pc_input_runtime_distribute(&g->input_runtime);
    if(stage==SSA_FRAME_CONTROL){
        /* The frontend owns the original terminal fade and level/menu handoff.
         * Freeze here if a headless caller has not consumed that boundary. */
        if(r->life_game.input_state==3){
            if(!g->finished){music(g,9);notice(g,g->level+1<SSA_NATIVE_CAMPAIGN_LEVELS?
                "Level complete. Press Enter for the next level.":"Final level complete. Press Enter for the ending.");}
            g->finished=1;g->frame_game.flags|=1;
        }
        if(r->life_game.input_state==4)g->frame_game.flags|=1;
        return 0;
    }
    if(stage==SSA_FRAME_PREPARE_SPRITES){
        alternate_after(g);
        if(g->player.pose.third_person)g->weapon.object.flags&=~UINT32_C(1);
        return ssa_sprite_cache_prepare(&g->cache,&g->sprites,&g->sprite_assets,0)<0?-1:0;
    }
    if(stage==SSA_FRAME_BUILD_OAM){
        g->oam.counter=g->sprites.counter;
        if(ssa_native_game_build_oam(g,&g->oam,&g->sprites)<0)return -1;
        return ssa_sprite_cache_flush(&g->cache,&g->sprite_assets)<0?-1:0;
    }
    return -1;
}
static int stage_before(void *p,const SsaGameRuntime *r,SsaFrameStage stage)
{SsaNativeGame *g=p;(void)r;g->error_stage=(uint32_t)stage;hud_health_update(g);
    if(stage==SSA_FRAME_ENTITIES)audio_player_begin(g);
    else if(stage==SSA_FRAME_MOVERS)audio_player_motion(g);
    else if(stage==SSA_FRAME_SPRITE_CONTROLLERS){
        alternate_before(g);
        /* Pickups have already run. Compare after weapon controllers so a
         * same-frame pickup cannot mask actual ammo consumption. */
        memcpy(g->hud_ammo_before,g->players[0].inventory.ammo,sizeof g->hud_ammo_before);
        if(g->players[0].motion.movement.flags&8)g->weapon.fire_inhibited=1;
    }
    return 0;}
static int entity_before(void *p,const SsaEntityRuntime *r,uint32_t entry,unsigned slot)
{SsaNativeGame *g=p;(void)r;g->error_entry=entry;g->error_slot=slot;hud_health_update(g);
    if(slot<100 && g->spawn.actors[slot].owned && g->spawn.actors[slot].entity.type>=0xcd &&
       g->spawn.actors[slot].entity.type<=0xd8)(void)audio_entity_source(g,slot);
    return 0;}
static int render_event(void *p,SsaRenderView *v,SsaRenderViewEvent event,uint32_t arg)
{
    SsaNativeGame *g=p;
    if(event==SSA_RENDER_VIEW_CLEAR)return ssa_world_scene_clear(&g->world_scene,(uint8_t)arg);
    if(event==SSA_RENDER_VIEW_WORLD)return ssa_world_scene_draw(&g->world_scene,&v->camera,
        (uint8_t)g->frame_game.counter,g->players[0].motion.movement.heading);
    return -1;
}
int ssa_native_game_prepare_view(SsaNativeGame *g)
{
    unsigned i,n;
    if(!g || !g->loaded || g->game.updating ||
       ssa_render_view_pose(&g->render_view,&g->player.pose,g->players[0].motion.movement.heading,&g->map)<0)return -1;
    g->pc_camera=g->render_view.camera;g->render_pitch=g->player.pitch;g->pc_pose=g->player.pose;
    weapon_bob_update(g);
    /* Reference world pixels expand2x horizontally and4/3 vertically. */
    g->actor_scene.screen_x=g->weapon_bob_x>=0?(g->weapon_bob_x+256)/512:-(-g->weapon_bob_x+256)/512;
    g->actor_scene.screen_y=g->weapon_bob_y>=0?(g->weapon_bob_y*3+512)/1024:-(-g->weapon_bob_y*3+512)/1024;
    if(g->player.pose.third_person && !g->gba_view && g->vertical_aim){
        /* Camera convergence uses a common unlimited sight ray. Short melee reach
         * must not tilt the chase view when switching weapons. Attack queries
         * retain their own ROM weapon ranges. */
        if(sight_target(g,&g->pc_pose.target_x,&g->pc_pose.target_y,&g->pc_pose.target_z)<0)return -1;
        if(!ssa_view_angles(&g->pc_pose,&g->math,&g->pc_camera.heading,&g->render_pitch))return -1;
    }
    n=g->spawn.pool.active_count;
    for(i=0;i<n;++i){unsigned slot=g->spawn.pool.active_slots[i];SsaScriptActor *a=&g->spawn.actors[slot];
        g->source_z_offsets[i]=0;
        if(a->owned){g->sources[i]=(SsaActorSource){&a->entity,a->animation,a->animation_display_frame,
            a->entity.body.flags,a->activation.facing,a->entity.player_index};
            if(g->alternates.enabled && g->alternates.flash_left[slot] && a->entity.type==0x96 &&
               a->animation==g->particle_rules.animation[8])g->sources[i].flags|=SSA_ACTOR_SCREEN_MIRROR;
            /* A controller can change animation while Time Strike freezes
             * 00908. Its previous displayed frame may belong to a longer
             * sequence. Repair only the render snapshot until playback resumes;
             * the canonical frozen animation, timers and AI stay untouched. */
            if((a->entity.body.flags&0x800) && !a->animation_frame && a->animation<SSA_ACTOR_RESOURCE_COUNT &&
               a->animation_display_frame<0xfffe && g->actor_assets.resources[a->animation].frames &&
               a->animation_display_frame>=g->actor_assets.resources[a->animation].frames){
                unsigned pos=g->actor_animations.offsets[a->animation];
                if(pos<SSA_ACTOR_ANIMATION_WORDS && g->actor_animations.words[pos]<g->actor_assets.resources[a->animation].frames)
                    g->sources[i].frame=g->actor_animations.words[pos];
            }
            if(g->projectiles.active[slot] && a->entity.type==0xe3){int32_t z;int64_t offset;
                if(ssa_projectile_runtime_render_z(&g->projectiles,slot,&z)<0)return -1;
                offset=(int64_t)z-a->entity.body.z;
                g->source_z_offsets[i]=offset<INT32_MIN?INT32_MIN:offset>INT32_MAX?INT32_MAX:(int32_t)offset;
            }
            if(g->player.pose.third_person && a->entity.type==0x96 && (g->sources[i].flags&0x2000))
                /* Particle IDs8..12 are fixed-screen weapon flashes. The
                 * reference fixed-screen projector ignores invisibility, so
                 * clear that mode as well as hiding this render snapshot. */
                g->sources[i].flags=(uint16_t)((g->sources[i].flags&UINT16_C(0xdfff))|0x80);
        }
        else {SsaPlayerSpawn *q=&g->players[0];
            g->sources[i]=(SsaActorSource){&q->entity,g->player_animation.animation,g->player_animation.frame,
                q->entity.body.flags,q->motion.facing,q->entity.player_index};
            if(q->motion.movement.flags&0x10)g->sources[i].facing=g->player_death_facing;
            /* Original body overlay family3 is the white shirt. Select only
             * the render appearance; player0 keeps its gameplay identity. */
            if(g->combat.player_count==1)g->sources[i].owner=3;
            if(g->player.pose.third_person)g->sources[i].flags&=UINT16_C(0xff7f);}
    }
    return 1;
}
int ssa_native_game_draw(SsaNativeGame *g)
{
    SsaRenderViewOps ops={render_event,g};const uint8_t *pixels;unsigned n;
    if(g && g->loaded && (g->players[0].motion.movement.flags&8)){
        SsaAutomapView map={0};SsaAutomapPoint points[100];uint8_t image[38400];unsigned i;
        const SsaEntity *body=&g->players[0].entity;int32_t sine,cosine,ex,ey;
        if(g->game.updating || g->error_line)return -1;
        map.x=map.player_x=body->body.x;map.y=map.player_y=body->body.y;
        map.heading=g->players[0].motion.movement.heading;map.facing=g->players[0].motion.facing;
        map.zoom=g->automap_zoom;map.scanline=g->automap_scanline;
        ssa_sincos(&g->math,(uint16_t)((unsigned)map.facing<<8),&sine,&cosine);
        ex=(int32_t)((int64_t)body->body.x+((int64_t)sine*64*map.zoom>>16));
        ey=(int32_t)((int64_t)body->body.y+((int64_t)cosine*64*map.zoom>>16));
        if(ssa_player_interaction_query_at(&g->automap_query,body,ex,ey)<0)return -1;
        map.contacts=g->automap_query.special_lines;map.contact_count=g->automap_query.special_count;
        map.entities=points;
        for(i=0;i<g->spawn.pool.active_count;++i){unsigned slot=g->spawn.pool.active_slots[i];
            SsaEntity *e=ssa_script_spawn_entity(&g->spawn,g->combat.reserved,slot);
            if(!e)return -1;
            points[map.entity_count++]=(SsaAutomapPoint){e->body.x,e->body.y};}
        if(ssa_automap_draw(image,sizeof image,&g->map,&g->math,&map)<0)return -1;
        for(i=0;i<38400;++i){unsigned at=0x46265c+2*image[i];g->rgb555[i]=(uint16_t)(g->rom[at]|(unsigned)g->rom[at+1]<<8);}
        return ssa_display_rgb888(g->rgb555,38400,g->rgb888,38400)<0?-1:1;
    }
    if(ssa_native_game_prepare_view(g)<0){fprintf(stderr,"Native view preparation failed.\n");return -1;}
    if(ssa_render_view_step(&g->render_view,&ops)<0){fprintf(stderr,"Native reference world rendering failed.\n");return -1;}
    pixels=ssa_world_scene_pixels(&g->world_scene);if(!pixels)return -1;
    if(g->surface_decals_enabled && ssa_surface_decals_reference(&g->surface_decals,&g->world_scene.planes,
        &g->render_view.camera,g->world_scene.framebuffer+SSA_SCENE_FRAME_PREFIX,SSA_SCENE_FRAME_BYTES)<0)return -1;
    if(g->surface_decals_enabled || g->water_reflections){
        if(ssa_water_begin(&g->water_render,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
            g->render_view.camera.x/65536.0,g->render_view.camera.y/65536.0,g->render_view.camera.z/65536.0,
            g->frame_game.counter,g->surface_decals_enabled,g->water_reflections,120,120)<0)return -1;
        if(ssa_water_reference(&g->water_render,&g->world_scene.planes,&g->render_view.camera,
            g->world_scene.framebuffer+SSA_SCENE_FRAME_PREFIX,SSA_SCENE_FRAME_BYTES)<0)return -1;
        if(ssa_pc_renderer_reflect(&g->water_renderer,g,&g->render_view.camera,0,&g->water_render,NULL,
            g->world_scene.framebuffer+SSA_SCENE_FRAME_PREFIX)<0)return -1;}
    n=g->spawn.pool.active_count;
    if(ssa_actor_scene_draw_offsets(&g->actor_scene,&g->render_view.camera,g->frame_game.flags,g->sources,n,g->source_z_offsets)<0){fprintf(stderr,"Native reference actor rendering failed (%u sources).\n",n);return -1;}
    if(ssa_display_compose_gameplay(&g->registers,pixels,14400,
       g->palette_effect.colors,g->sprite_assets.palette,
       &g->oam,g->cache.tiles,sizeof(g->cache.tiles),g->rgb555,SSA_DISPLAY_PIXELS,
       g->players[0].inventory.health,g->players[0].inventory.stat_c4)<0){fprintf(stderr,"Native display composition failed.\n");return -1;}
    if(g->hud_style==SSA_HUD_CLASSIC && !g->hud_suppressed){uint8_t canvas[38400],background[38400];unsigned i;
        if(ssa_native_game_classic_hud_layers(g,canvas,background,sizeof canvas)!=1)return -1;
        for(i=0;i<38400;++i){if(canvas[i])g->rgb555[i]=ssa_classic_hud_blend555(g->sprite_assets.palette[canvas[i]]&0x7fff,g->rgb555[i],
                ssa_classic_hud_alpha(i%240,i/240,g->hud_netricsa_ticks,g->hud.boss_present && g->hud.boss_health>0));
            else if(background[i])g->rgb555[i]=ssa_classic_hud_darken555(g->rgb555[i]);}}
    return ssa_display_rgb888(g->rgb555,SSA_DISPLAY_PIXELS,g->rgb888,SSA_DISPLAY_PIXELS)<0?-1:1;
}
int ssa_native_game_classic_hud(const SsaNativeGame *g,uint8_t *canvas,size_t capacity)
{return ssa_native_game_classic_hud_layers(g,canvas,NULL,capacity);}
static void netricsa_weapons(SsaNetricsaJournal *next,const SsaNativeGame *g)
{
    unsigned owned=g->players[0].inventory.weapons&0x3ffu;
    /* Entry loadouts are familiar. Preserve unread discoveries already in
     * the carried journal instead of acknowledging all carried equipment. */
    if(!g->frame_game.counter)next->read_weapons|=(uint16_t)(owned&~next->weapons);
    next->weapons|=(uint16_t)owned;
    /* The first-level Schofield remains familiar even when picked up there. */
    if(g->level==0)next->read_weapons|=next->weapons&3u;
}
static int netricsa_history(SsaNetricsaJournal *j)
{
    uint32_t known,seen=0;unsigned i;
    if(j->order_count>22 || (j->weapons&~0x3ffu) || (j->enemies&~0xfffu) ||
       (j->read_weapons&~j->weapons) || (j->read_enemies&~j->enemies))return -1;
    known=(uint32_t)j->weapons|((uint32_t)j->enemies<<10);
    for(i=0;i<j->order_count;++i){unsigned id=j->order[i];uint32_t bit;
        if(id<1 || id>22)return -1;
        bit=UINT32_C(1)<<(id-1);
        if(!(known&bit) || (seen&bit))return -1;
        seen|=bit;
    }
    /* Snapshot-only discoveries join the end without changing live state.
     * Simultaneous discoveries have a stable weapon-then-enemy ID tie order. */
    for(i=0;i<22;++i)if((known&~seen)&(UINT32_C(1)<<i))j->order[j->order_count++]=(uint8_t)(i+1);
    return 1;
}
int ssa_native_game_netricsa_entries(SsaNetricsaJournal *out,const SsaNativeGame *g)
{
    SsaNetricsaJournal next;unsigned i;
    if(!out || !g || !g->loaded || g->game.updating)return -1;
    next=g->journal;netricsa_weapons(&next,g);
    for(i=0;i<12;++i)if(g->damage.enemy_kills[i])next.enemies|=(uint16_t)(1u<<i);
    /* Drains/hazards can kill without the ordinary damage statistics call. */
    for(i=0;i<100;++i){const SsaScriptActor *a=&g->spawn.actors[i];
        if(a->owned && a->entity.type>=0xcd && a->entity.type<=0xd8 &&
           a->activation.health<=0 && (a->entity.body.flags&0x20))next.enemies|=(uint16_t)(1u<<(a->entity.type-0xcd));}
    if(netricsa_history(&next)!=1)return -1;
    *out=next;return 1;
}
int ssa_native_game_netricsa_unread(const SsaNativeGame *g)
{
    SsaNetricsaJournal next;unsigned bits,count=0;
    if(ssa_native_game_netricsa_entries(&next,g)!=1)return -1;
    bits=(unsigned)(next.weapons&~next.read_weapons)|((unsigned)(next.enemies&~next.read_enemies)<<10);
    while(bits){count+=bits&1u;bits>>=1;}
    return (int)count;
}
int ssa_native_game_netricsa_latest(const SsaNativeGame *g)
{
    SsaNetricsaJournal j;uint32_t unread;unsigned i;
    if(ssa_native_game_netricsa_entries(&j,g)!=1)return -1;
    unread=(uint32_t)(j.weapons&~j.read_weapons)|((uint32_t)(j.enemies&~j.read_enemies)<<10);
    for(i=j.order_count;i>0;--i)if(unread&(UINT32_C(1)<<(j.order[i-1]-1)))return j.order[i-1];
    return 0;
}
static void netricsa_store(SsaNativeGame *g,const SsaNetricsaJournal *next)
{
    unsigned weapons=next->weapons&~next->read_weapons;
    unsigned enemies=next->enemies&~next->read_enemies;
    if((weapons&~g->journal.weapons) || (enemies&~g->journal.enemies)){
        g->hud_netricsa_ticks=300;
        audio_netricsa_start(g);
    }
    if(!(weapons|enemies)){g->hud_netricsa_ticks=0;audio_netricsa_cancel(g);}
    g->journal=*next;
}
int ssa_native_game_netricsa_read(SsaNativeGame *g,unsigned weapons,unsigned enemies)
{
    SsaNetricsaJournal next;
    if(weapons>0x3ffu || enemies>0xfffu || ssa_native_game_netricsa_entries(&next,g)!=1)return -1;
    next.read_weapons|=(uint16_t)(weapons&next.weapons);next.read_enemies|=(uint16_t)(enemies&next.enemies);
    netricsa_store(g,&next);return 1;
}
int ssa_native_game_classic_hud_layers(const SsaNativeGame *g,uint8_t *canvas,uint8_t *background,size_t capacity)
{
    SsaHudSpriteState state;unsigned i,flashing=0;int unread;
    if(!g || !g->loaded || !canvas || capacity<38400)return -1;
    if(g->hud_style!=SSA_HUD_CLASSIC || g->hud_suppressed){memset(canvas,0,38400);
        if(background)memset(background,0,38400);
        return 1;}
    unread=ssa_native_game_netricsa_unread(g);if(unread<0)return -1;
    state=g->hud;state.local_flags=g->players[0].motion.movement.flags;
    state.health=g->players[0].inventory.health;state.armor=g->players[0].inventory.stat_c4;
    state.score=g->score;state.time=g->frame_game.elapsed;state.current_weapon=g->gameplay.weapon.requested;
    state.weapon_action=g->gameplay.weapon.current;
    memcpy(state.ammo,g->players[0].inventory.ammo,sizeof state.ammo);
    for(i=2;i<10;++i)if(g->hud_ammo_flash[i])flashing|=1u<<i;
    if(ssa_classic_hud_draw(&g->sprite_assets,&g->weapon_animations,&state,&g->inventory_rules,
        flashing,g->hud_health_flash,g->high_score,canvas,background,capacity)!=1)return -1;
    if(!(state.local_flags&8) &&
       ssa_classic_hud_netricsa(&g->sprite_assets,&g->weapon_animations,(unsigned)unread,
           state.health>0 && !g->finished?g->hud_netricsa_ticks:0,
           state.boss_present && state.boss_health>0,canvas,background,capacity)!=1)return -1;
    if(!(state.local_flags&8) && state.health>0 && !g->finished && g->hud_use_available &&
       ssa_classic_hud_use(&g->sprite_assets,canvas,capacity)!=1)return -1;
    if(!(state.local_flags&8) && g->game.life_game.message_ticks && g->hud_notice.length &&
       ssa_classic_hud_text(&g->hud_text,&g->sprite_assets,(const char *)g->hud_notice.text,
           g->hud_notice.id>=0x8c && g->hud_notice.id<=0xab?127:g->hud_notice.base_y,
           canvas,background,capacity)!=1)return -1;
    if(g->hud_weapon_ticks && !(state.local_flags&8) && state.health>0 && !g->finished)
        return ssa_classic_hud_weapon_wheel(&g->sprite_assets,&g->players[0].inventory,
            g->gameplay.weapon.requested,canvas,background,capacity);
    return 1;
}
int ssa_native_game_hud_style(SsaNativeGame *g,unsigned style)
{
    unsigned previous;
    if(!g || !g->loaded || g->game.updating || g->error_line || style>SSA_HUD_CLASSIC)return -1;
    if(g->hud_style==style)return 1;
    previous=g->hud_style;g->hud_style=style;
    /* Pre-level menus have no sprite-controller/animation/cache frame yet.
     * In particular the alternate starting Colt still has previous_frameFFFF.
     * Save the choice now; the first normal frame builds its complete OAM. */
    if(!g->frame_game.counter){if(style==SSA_HUD_ORIGINAL)audio_netricsa_cancel(g);return 1;}
    if(ssa_native_game_build_oam(g,&g->oam,&g->sprites)!=1){g->hud_style=previous;return -1;}
    if(style==SSA_HUD_ORIGINAL)audio_netricsa_cancel(g);
    return 1;
}
int ssa_native_game_automap(SsaNativeGame *g,unsigned enabled)
{
    uint8_t counter;
    if(!g || !g->loaded || g->error_line || g->game.updating || enabled>1 ||
       (enabled && (g->finished || g->players[0].inventory.health<=0 || (g->frame_game.flags&1))))return -1;
    if(enabled){if(!(g->players[0].motion.movement.flags&8))g->automap_zoom=8192;
        g->players[0].motion.movement.flags|=8;}
    else g->players[0].motion.movement.flags&=(uint16_t)~8u;
    g->players[0].vertical.flags=g->players[0].motion.movement.flags;
    g->weapon.fire_inhibited=enabled;g->weapon.fire_pending_weapon=g->weapon.colt_pair_pending=0;
    /* Mode changes can present before the next simulation tick. Switch its
     * palette now, retaining the simulation-owned flash clock. */
    counter=g->palette_effect.counter;
    if(ssa_palette_effect_step(&g->palette_effect,&g->damage.hurt_flash,g->players[0].motion.movement.flags)<0)return -1;
    g->palette_effect.counter=counter;
    return 1;
}
int ssa_native_game_automap_zoom(SsaNativeGame *g,int steps)
{
    int64_t count=steps<0?-(int64_t)steps:steps;
    if(!g || !g->loaded || g->game.updating || g->error_line || !(g->players[0].motion.movement.flags&8) ||
       (g->frame_game.flags&1) || g->players[0].inventory.health<=0)return -1;
    if(count>256)count=256;
    while(count--){if(steps>0){g->automap_zoom+=g->automap_zoom>>4;if(g->automap_zoom>65536)g->automap_zoom=65536;}
        else{g->automap_zoom-=g->automap_zoom>>4;if(g->automap_zoom<4096)g->automap_zoom=4096;}}
    return 1;
}
static int update_use_prompt(SsaNativeGame *g)
{
    size_t i,j;g->hud_use_available=0;
    if(g->players[0].inventory.health<=0 || g->finished || (g->players[0].motion.movement.flags&8))return 1;
    if(ssa_player_interaction_query(&g->use_query,&g->players[0].entity,g->players[0].motion.facing,&g->math)<0)return -1;
    for(i=0;i<g->use_query.special_count;++i)for(j=0;j<g->level_runtime.switches.count;++j){
        const SsaSwitchEntry *s=&g->level_runtime.switches.entries[j];
        if(s->line==g->use_query.special_lines[i] && !(s->flags&1)){g->hud_use_available=1;return 1;}}
    return 1;
}
int ssa_native_game_tick(SsaNativeGame *g)
{
    SsaGameRuntimeInput in={0};const SsaEntity *peers[1];unsigned old_weapon,old_variant,old_request,old_requested_variant,i;
    SsaNetricsaJournal journal;
    uint32_t shots_before[10];int16_t old_health;
    if(!g || !g->loaded || g->error_line)return -1;
    /* Commit the entry baseline before this frame can collect a new weapon.
     * Pre-level snapshots apply the same rule without mutating the game. */
    if(!g->frame_game.counter)netricsa_weapons(&g->journal,g);
    memcpy(g->hud_ammo_before,g->players[0].inventory.ammo,sizeof g->hud_ammo_before);
    memcpy(shots_before,g->weapon_game.shots,sizeof shots_before);
    if(!(g->frame_game.flags&1))g->hud_health_flash=g->hud_health_flash>g->frame_game.ticks?
        (uint8_t)(g->hud_health_flash-g->frame_game.ticks):0;
    if(!(g->frame_game.flags&1))g->hud_netricsa_ticks=g->hud_netricsa_ticks>g->frame_game.ticks?
        (uint16_t)(g->hud_netricsa_ticks-g->frame_game.ticks):0;
    hud_health_update(g);
    if(!(g->frame_game.flags&1))g->hud_weapon_ticks=g->hud_weapon_ticks>g->frame_game.ticks?
        (uint16_t)(g->hud_weapon_ticks-g->frame_game.ticks):0;
    if(!(g->frame_game.flags&1))for(i=2;i<10;++i)
        g->hud_ammo_flash[i]=g->hud_ammo_flash[i]>g->frame_game.ticks?
            (uint8_t)(g->hud_ammo_flash[i]-g->frame_game.ticks):0;
    if(g->players[0].inventory.health<=0 && (g->players[0].motion.movement.flags&8)){
        g->players[0].motion.movement.flags&=(uint16_t)~8u;
        g->players[0].vertical.flags=g->players[0].motion.movement.flags;g->weapon.fire_inhibited=0;
    }
    if(g->gba_view || !g->vertical_aim){g->player.pitch=0;g->player.free_aim=0;g->input.pending_pitch=0;}
    old_weapon=g->gameplay.weapon.current;old_variant=g->alternates.variant;old_health=g->players[0].inventory.health;
    old_request=g->gameplay.weapon.requested;old_requested_variant=g->alternates.requested_variant;
    peers[0]=&g->players[0].entity;
    g->footstep_x=peers[0]->body.x;g->footstep_y=peers[0]->body.y;
    in.entities.players[0].motion=(SsaMotionFrame){peers,1,1,0,0};
    in.entities.players[0].controls_enabled=1;in.entities.players[0].footsteps_muted=0;
    in.entities.players[0].camera_counter=(uint16_t)g->frame_game.counter;
    in.entities.enemies.ticks=1;in.entities.enemies.speed_factor=65536;
    in.entities.enemies.level_counter=(uint16_t)g->frame_game.counter;
    if(ssa_game_runtime_step(&g->game,&in,&g->game_ops)<0){g->error_line=__LINE__;return -1;}
    if(update_use_prompt(g)<0){g->error_line=__LINE__;return -1;}
    if(ssa_native_game_netricsa_entries(&journal,g)!=1){g->error_line=__LINE__;return -1;}
    netricsa_store(g,&journal);
    if(g->players[0].inventory.health<=0 || g->finished)g->hud_netricsa_ticks=0;
    if(!g->hud_netricsa_ticks || g->hud_style!=SSA_HUD_CLASSIC)audio_netricsa_cancel(g);
    /* Legacy sinks round .1075s of sound plus a half-duration gap to10
     * frames. Windows measures the sound and gap in output samples. */
    if(!(g->frame_game.flags&1) && g->hud_netricsa_repeat==1 && g->hud_netricsa_ticks<=290){
        g->hud_netricsa_repeat=0;(void)sound(g,0x0b);
    }
    hud_health_update(g);
    for(i=2;i<10;++i)if(g->players[0].inventory.ammo[i]<g->hud_ammo_before[i])g->hud_ammo_flash[i]=40;
    if(g->players[0].motion.movement.flags&8)g->automap_scanline=(uint8_t)(g->automap_scanline+g->frame_game.ticks);
    if(old_health>0 && g->players[0].inventory.health>0 && old_weapon>=1 && old_weapon<=10 &&
       (old_weapon!=g->gameplay.weapon.current || old_variant!=g->alternates.variant)){
        g->previous_weapon=old_weapon;g->alternates.previous_variant=g->alternates.dual_colts?old_variant:0;
        if(!g->hud_weapon_dismissed)g->hud_weapon_ticks=180;
    }
    if(old_request!=g->gameplay.weapon.requested || old_requested_variant!=g->alternates.requested_variant){
        g->hud_weapon_ticks=180;g->hud_weapon_dismissed=0;
    }
    /* Consume the sampled click even between shots or during a queued switch.
     * Shot counters also catch attacks delivered from the buffered-fire path. */
    if((g->input_runtime.sample.enabled && ((g->input_runtime.sample.held|g->input_runtime.sample.pressed)&1)) ||
       memcmp(shots_before,g->weapon_game.shots,sizeof shots_before) ||
       g->players[0].inventory.health<=0 || g->finished){g->hud_weapon_ticks=0;g->hud_weapon_dismissed=1;}
    if(g->players[0].inventory.health<=0){g->previous_weapon=0;g->alternates.previous_variant=0;}
    if(ssa_palette_effect_step(&g->palette_effect,&g->damage.hurt_flash,g->players[0].motion.movement.flags)<0){g->error_line=__LINE__;return -1;}
    audio_listener(g);
    audio_world_sync(g);
    if(g->notice_ticks)--g->notice_ticks;
    return 1;
}
int ssa_native_game_step(SsaNativeGame *g)
{return ssa_native_game_tick(g)<0?-1:ssa_native_game_draw(g);}
int ssa_native_game_select_weapon(SsaNativeGame *g,unsigned weapon)
{
    unsigned variant=0;
    if(!g || !g->loaded || g->game.updating || weapon<1 || weapon>10)return -1;
    if(weapon==2 && g->alternates.enabled && g->alternates.dual_colts)
        variant=g->gameplay.weapon.requested==2?!g->alternates.requested_variant:1;
    return select_variant(g,weapon,variant);
}
static int select_variant(SsaNativeGame *g,unsigned weapon,unsigned variant)
{
    if(!(g->players[0].inventory.weapons&(1u<<(weapon-1))) || !g->players[0].inventory.ammo[weapon-1])return 0;
    if(variant && (!g->alternates.enabled || !g->alternates.dual_colts || weapon!=2))return 0;
    if(g->gameplay.weapon.requested!=weapon || g->alternates.requested_variant!=variant){
        g->hud_weapon_ticks=180;g->hud_weapon_dismissed=0;
    }
    g->gameplay.weapon.requested=(uint16_t)weapon;
    g->alternates.requested_variant=variant;
    if(g->alternates.enabled && weapon==2 && g->gameplay.weapon.current==2){
        g->gameplay.weapon.flags&=~0x2100u;
        g->players[0].motion.movement.weapon_flags=g->gameplay.weapon.flags;return 1;
    }
    if(g->weapon.finish_fire_before_switch && weapon==g->gameplay.weapon.current && (g->gameplay.weapon.flags&0x400)){
        /* Selecting the firing weapon again cancels the queued handoff. */
        g->gameplay.weapon.flags&=~0x2100u;g->weapon.switch_wait=0;
    }else g->gameplay.weapon.flags|=0x2100;
    g->players[0].motion.movement.weapon_flags=g->gameplay.weapon.flags;return 1;
}
int ssa_native_game_cycle_weapon(SsaNativeGame *g,int steps)
{
    unsigned weapons[11],variants[11],current=0,next,count=0,i,hops,found=0;int direction;
    int64_t magnitude=steps;
    if(!g || !g->loaded || g->game.updating || g->gameplay.weapon.requested<1 ||
       g->gameplay.weapon.requested>10)return -1;
    if(!steps)return 0;
    for(i=0;i<10;++i)if((g->players[0].inventory.weapons&(1u<<i)) && g->players[0].inventory.ammo[i]){
        weapons[count]=i+1;variants[count++]=0;
        if(i==1 && g->alternates.enabled && g->alternates.dual_colts){weapons[count]=2;variants[count++]=1;}
    }
    if(!count)return 0;
    direction=steps<0?-1:1;if(magnitude<0)magnitude=-magnitude;
    for(i=0;i<count;++i)if(weapons[i]==g->gameplay.weapon.requested && variants[i]==g->alternates.requested_variant){current=i;found=1;break;}
    if(!found){unsigned requested=g->gameplay.weapon.requested*2+g->alternates.requested_variant;
        if(direction>0){current=count-1;for(i=0;i<count;++i)if(weapons[i]*2+variants[i]>requested){current=(i+count-1)%count;break;}}
        else{current=0;for(i=0;i<count;++i)if(weapons[i]*2+variants[i]<requested)current=(i+1)%count;}
    }
    next=current;
    hops=(unsigned)((magnitude-1)%count)+1;
    for(i=0;i<hops;++i){
        next=direction>0?(next+1)%count:(next+count-1)%count;
    }
    return found && next==current?0:select_variant(g,weapons[next],variants[next]);
}
int ssa_native_game_previous_weapon(SsaNativeGame *g)
{
    if(!g || !g->loaded || g->game.updating)return -1;
    if(!g->previous_weapon || g->previous_weapon>10 ||
       (g->previous_weapon==g->gameplay.weapon.requested && g->alternates.previous_variant==g->alternates.requested_variant))return 0;
    return select_variant(g,g->previous_weapon,g->alternates.previous_variant);
}
int ssa_native_game_init(SsaNativeGame *g,const uint8_t *rom,size_t size,unsigned level)
{return ssa_native_game_init_loadout(g,rom,size,level,0);}
int ssa_native_game_init_loadout(SsaNativeGame *g,const uint8_t *rom,size_t size,unsigned level,unsigned setting)
{
    SsaEntity *reserved[100]={0};SsaActorAnimation *states[100]={0};SsaPlayerRuntime *players[1];
    SsaPlayerGameplay *gameplays[1];SsaWeaponRuntime *weapons[1];SsaWeaponObject *object;unsigned i,n;
    if(!g || g->rom || !rom || size!=0x800000 || level>15 || setting>36 || (setting && (setting-1)%12!=level))return -1;
    g->rom=rom;g->rom_size=size;g->level=level;g->music_mode=2;
    g->fov_offset=SSA_FOV_DEFAULT_OFFSET;
#define INIT(x) do{if(!(x)){g->error_line=__LINE__;return -1;}}while(0)
    INIT(ssa_math_tables_load(&g->math,rom,size));INIT(ssa_map_assets_load(&g->map,rom,size,level));
    INIT(ssa_level_runtime_init(&g->level_runtime,&g->map,rom,size,level)==0);
    INIT(ssa_texture_assets_load(&g->textures,rom,size)==1);INIT(ssa_sprite_assets_load(&g->sprite_assets,rom,size)==1);
    INIT(ssa_sky_assets_load(&g->sky,rom,size)==1);INIT(ssa_visibility_tables_load(&g->visibility_tables,rom,size)==1);
    INIT(ssa_wall_render_tables_load(&g->wall_tables,rom,size)==1);INIT(ssa_spawn_definitions_load(&g->definitions,rom,size));
    INIT(ssa_actor_animations_load(&g->actor_animations,rom,size)==1);INIT(ssa_actor_assets_load(&g->actor_assets,rom,size)==1);
    INIT(ssa_inventory_rules_load(&g->inventory_rules,rom,size)==0);INIT(ssa_weapon_rules_load(&g->weapon_rules,rom,size)==1);
    INIT(ssa_weapon_animations_load(&g->weapon_animations,rom,size)==1);INIT(ssa_weapon_loadouts_load(&g->loadouts,rom,size)==1);
    INIT(ssa_pickup_respawn_data_load(&g->pickup_data,rom,size)==1);INIT(ssa_particle_rules_load(&g->particle_rules,rom,size)==1);
    INIT(ssa_world_init(&g->world,&g->map,100));INIT(ssa_slide_init(&g->slide,&g->map));
    INIT(ssa_map_collision_init(&g->automap_query,&g->map));g->automap_zoom=8192;
    INIT(ssa_map_collision_init(&g->use_query,&g->map));
    INIT(ssa_weapon_aim_init(&g->camera_trace,&g->world,&g->math)==1);
    INIT(ssa_spawn_plan_load(&g->plan,&g->map,&g->definitions,level,1,99));INIT(ssa_spawn_plan_insert(&g->plan,&g->world));
    INIT(ssa_players_spawn(g->players,&g->world,&g->plan,1,0));INIT(g->players[0].active);
    INIT(ssa_player_runtime_init(&g->player,&g->players[0],&g->world,&g->slide,&g->math)==0);
    INIT(ssa_player_gameplay_init(&g->gameplay,&g->player,&g->plan)==0);
    g->gameplay.immediate_use=1;
    g->gameplay.life_game.continue_flag=3;g->gameplay.life_game.input_state=1;
    INIT(ssa_script_spawn_init(&g->spawn,&g->level_runtime,&g->world)==0);
    INIT(ssa_script_spawn_adopt_plan(&g->spawn,&g->plan)==0);
    n=(unsigned)g->plan.object_count;
    for(i=0;i<n;++i)reserved[i]=&g->spawn.actors[i].entity;
    reserved[n]=&g->players[0].entity;states[n]=&g->player_animation;
    players[0]=&g->player;gameplays[0]=&g->gameplay;weapons[0]=&g->weapon;
    g->damage.player_count=1;g->damage.difficulty=(uint8_t)(setting?(setting-1)/12:1);
    if(setting)g->gameplay.life_game.continue_flag=(uint8_t)(g->damage.difficulty==0?5:g->damage.difficulty==1?3:2);
    INIT(ssa_combat_runtime_init(&g->combat,&g->spawn,reserved,n+1,players,1,&g->damage,&g->score)==0);
    g->combat.boss_projectile_immunity=1;
    g->combat.prion_balance=1;
    g->touch_ops=(SsaPickupRuntimeOps){pickup_event,g};
    INIT(ssa_pickup_runtime_init(&g->pickup,&g->combat,&g->inventory_rules,&g->touch_ops)==0);
    /* Always allow unowned ammo and duplicate guns for ammo, independent of
     * alternate weapons. That option changes only the second Colt policy. */
    g->pickup.pc_pickups=1;
    g->pickup.collect_owned_weapons=1;
    INIT(ssa_enemy_queries_init(&g->queries,&g->spawn,&g->math,reserved,n+1)==0);
    INIT(ssa_projectile_runtime_init(&g->projectiles,&g->combat,&g->math)==0);
    INIT(ssa_projectile_runtime_bind_particles(&g->projectiles,&g->particle_rules,particle_sound,g)==0);
    /* Reserve original weapon slot before1240C initializes its payload. */
    for(i=0;i<84;++i)g->sprites.objects[i]=i?&g->sprite_objects[i]:&g->weapon.object;
    INIT(ssa_sprite_pool_init(&g->sprite_pool,&g->sprites)==1);
    INIT(ssa_sprite_pool_allocate(&g->sprite_pool,&object)==1);INIT(object==&g->weapon.object);
    INIT(ssa_weapon_runtimes_start(weapons,gameplays,&g->combat,&g->inventory_rules,&g->weapon_rules,
        &g->weapon_animations,&g->loadouts,setting,0)==0);
    INIT(ssa_weapon_runtime_bind_projectiles(&g->weapon,&g->projectiles)==0);
    g->weapon.unlimited_ranged=1;g->projectiles.unlimited_player_range=1;
    g->projectiles.ballistic_player_rockets=1;
    g->projectiles.ballistic_player_prion=1;
    g->projectiles.ballistic_player_cannon=1;
    g->projectiles.rolling_cannonballs=1;
    g->projectiles.ceiling_impacts=1;
    g->weapon.time_strike_requires_target=1;
    g->projectiles.time_strike_gba_clock=1;
    /* Tommy/minigun: seven original GBA refreshes per shot. Chainsaw uses
     * its original measured mean (85 intervals/584 refreshes), with PC reach. */
    g->weapon.automatic_period=7;g->weapon.automatic_gba_clock=1;
    g->weapon.double_minigun=0;g->weapon.clocked_chainsaw=SSA_CHAINSAW_GBA_CADENCE;
    g->weapon.chainsaw_range=192;
    g->weapon.buffer_fire=1;
    g->weapon.finish_fire_before_switch=1;
    INIT(ssa_hud_sprites_create(&g->sprite_pool,1,0,level)==74);
    INIT(ssa_hud_glyphs_load(&g->hud,rom,size)==1);INIT(ssa_hud_notice_init(&g->hud_notice,rom,size,0)==1);
    INIT(ssa_classic_hud_text_load(&g->hud_text,rom,size)==1);
    INIT(ssa_sprite_cache_init(&g->cache)==1);
    INIT(ssa_palette_effect_init(&g->palette_effect,rom,size,level)==1);
    INIT(ssa_surface_decals_init(&g->surface_decals,g->palette_effect.colors,level)==1);
    INIT(ssa_sprite_oam_init(&g->oam,rom,size)==1);INIT(ssa_display_registers_default(&g->registers)==1);
    INIT(ssa_actor_animation_set(&g->player_animation,g->players[0].animation,g->players[0].animation_ticks)==1);
    INIT(ssa_actor_animation_runtime_init(&g->actor_runtime,&g->spawn,&g->actor_animations,reserved,states,n+1)==0);
    g->actor_runtime.fast_converted=1;
    INIT(ssa_pickup_respawn_runtime_init(&g->pickup_respawn,&g->combat,&g->plan,&g->pickup_data)==0);
    INIT(ssa_entity_runtime_init(&g->entities,&g->combat,&g->projectiles,&g->queries,gameplays,0)==0);
    g->entities.fast_converted=1;
    g->entities.nearest_converted=1;
    INIT(ssa_enemy_control_runtime_init(&g->enemies,&g->combat,&g->projectiles,&g->queries,&g->math,
        &g->entities.dispatch.walk_count,rom,size)==0);
    g->enemies.crowd_slide=1;
    INIT(ssa_enemy_navigation_speeds(g->enemy_speeds,rom,size)==1);
    g->entities.enemy_speeds=g->enemy_speeds;
    g->combat_ops=(SsaCombatRuntimeOps){combat_event,g};g->player_ops=(SsaLevelPlayerOps){player_event,player_animation,player_sound,g};
    g->cd_ops=(SsaCombatCdOps){&g->combat_ops,cd_event,g};g->enemy_ops=(SsaEnemyControlRuntimeOps){&g->combat_ops,enemy_event,g};
    g->projectile_ops=(SsaProjectileRuntimeOps){&g->combat_ops,projectile_particle,g};
    g->entity_ops.players[0]=&g->player_ops;g->entity_ops.enemies=&g->cd_ops;g->entity_ops.projectiles=&g->projectile_ops;
    g->entity_ops.before=entity_before;g->entity_ops.context=g;
    INIT(ssa_entity_runtime_bind_generic(&g->entities,&g->enemies,&g->enemy_ops)==0);
    g->script_ops=(SsaScriptSpawnOps){script_particle,script_event,g};g->mover_ops=(SsaSectorMoverOps){mover_sound,g};
    g->pickup_ops=(SsaPickupRespawnRuntimeOps){pickup_particle,g};g->weapon_ops=(SsaWeaponRuntimeOps){&g->combat_ops,weapon_event,weapon_impact,g};
    g->sprite_ops=(SsaSpriteDispatchOps){sprite_handler,NULL,g};
    g->frame_game.ticks=1;g->frame.game=&g->frame_game;
    INIT(ssa_game_runtime_init(&g->game,&g->frame,&g->entities,&g->actor_runtime,&g->pickup_respawn,
        &g->sprites,&g->weapon_animations,weapons,&g->weapon_game)==0);
    g->game.enemy_crowd_slide=1;
    g->game_ops.entities=&g->entity_ops;g->game_ops.scripts=&g->script_ops;g->game_ops.movers=&g->mover_ops;
    g->game_ops.pickups=&g->pickup_ops;g->game_ops.weapons[0]=&g->weapon_ops;g->game_ops.sprites=&g->sprite_ops;
    g->game_ops.external=external;g->game_ops.before=stage_before;g->game_ops.context=g;
    INIT(ssa_pc_input_init(&g->input,64)==1);INIT(ssa_pc_input_runtime_init(&g->input_runtime,&g->input,&g->game,0)==0);
    INIT(ssa_map_texture_bindings_init(&g->bindings,&g->map,&g->textures,(unsigned)ssa_texture_assets_family(level))==1);
    INIT(ssa_water_scene_init(&g->water_scene,&g->bindings,&g->math,&g->sky,g->palette_effect.colors)==1);
    INIT(ssa_world_scene_init(&g->world_scene,&g->bindings,&g->math,&g->visibility_tables,&g->wall_tables,&g->sky,level)==1);
    INIT(ssa_actor_scene_init(&g->actor_scene,&g->world_scene,&g->world,&g->actor_assets,&g->math)==1);
    g->render_view.math=&g->math;g->render_view.players[0].flags=4;g->render_view.ticks=1;
    g->loaded=1;
#undef INIT
    return 1;
}
static int carry_valid(const SsaNativeCarry *s)
{
    unsigned i;SsaNetricsaJournal journal;
    if(!(s && s->inventory.health>0 && s->inventory.pending_count<=SSA_PENDING_WEAPON_CAPACITY &&
        (s->inventory.weapons&1) && !(s->inventory.weapons&~0x3ffu) && s->weapon>=1 && s->weapon<=10 &&
        (s->inventory.weapons&(1u<<(s->weapon-1))) && s->inventory.ammo[s->weapon-1]>0 && s->third_person<=1 &&
        s->previous_weapon<=10 && s->alternate_weapons<=1 && s->dual_colts<=1 &&
        s->weapon_variant<=1 && s->previous_variant<=1 && s->colt_rounds[0]<=6 && s->colt_rounds[1]<=6 &&
        (!s->weapon_variant || (s->weapon==2 && s->dual_colts && s->alternate_weapons)) &&
        (!s->previous_variant || (s->previous_weapon==2 && s->dual_colts)) &&
        !(s->journal.weapons&~0x3ffu) && !(s->journal.enemies&~0xfffu) &&
        !(s->journal.read_weapons&~s->journal.weapons) && !(s->journal.read_enemies&~s->journal.enemies)))return 0;
    journal=s->journal;if(netricsa_history(&journal)!=1)return 0;
    for(i=0;i<s->inventory.pending_count;++i)if(s->inventory.pending_weapons[i]<1 || s->inventory.pending_weapons[i]>10)return 0;
    return 1;
}
int ssa_native_game_weapon_progress(SsaWeaponProgress *saved,SsaWeaponProgressStats stats[10],const SsaNativeGame *g)
{
    SsaWeaponProgress next={0};SsaWeaponProgressStats rows[10];unsigned i;
    if(!saved || !stats || !g || !g->loaded || g->error_line || g->game.updating || g->world.query_active ||
       g->combat.updating || g->spawn.updating || g->player.updating || g->weapon.updating)return -1;
    for(i=0;i<10;++i){rows[i].owned=rows[i].ammo=0;rows[i].shots=g->weapon_game.shots[i];rows[i].hits=g->weapon.hits[i];}
    if(ssa_weapon_progress_capture(&next,rows,&g->players[0].inventory,&g->gameplay.weapon,0)!=1)return -1;
    *saved=next;memcpy(stats,rows,sizeof rows);return 1;
}
int ssa_native_game_carry_capture(SsaNativeCarry *out,const SsaNativeGame *g)
{
    SsaNativeCarry next={0};SsaWeaponProgress saved;SsaWeaponProgressStats stats[10];unsigned i;
    if(!out || ssa_native_game_weapon_progress(&saved,stats,g)!=1)return -1;
    next.inventory=g->players[0].inventory;next.inventory.weapons=saved.weapons;
    memcpy(next.inventory.ammo,saved.ammo,sizeof saved.ammo);
    next.score=g->score;next.weapon=saved.requested;
    next.third_person=g->player.view.third_person!=0;
    next.previous_weapon=g->previous_weapon;
    next.alternate_weapons=g->alternates.enabled;next.dual_colts=g->alternates.dual_colts;
    next.weapon_variant=saved.requested==2?g->alternates.requested_variant:0;
    next.previous_variant=g->alternates.previous_variant;
    memcpy(next.colt_rounds,g->alternates.rounds,sizeof next.colt_rounds);
    if(ssa_native_game_netricsa_entries(&next.journal,g)!=1)return -1;
    if(next.weapon<1 || next.weapon>10 || !(next.inventory.weapons&(1u<<(next.weapon-1))) || next.inventory.ammo[next.weapon-1]<=0){
        next.weapon=1;next.weapon_variant=0;
        for(i=1;i<=10;++i)if((next.inventory.weapons&(1u<<(i-1))) && next.inventory.ammo[i-1]>0)next.weapon=i;
    }
    if(!carry_valid(&next))return -1;
    *out=next;return 1;
}
int ssa_native_game_carry_apply(SsaNativeGame *g,const SsaNativeCarry *s)
{
    SsaWeaponProgress saved;
    if(!g || !g->loaded || g->error_line || g->game.updating || g->world.query_active ||
       g->combat.updating || g->spawn.updating || g->player.updating || g->weapon.updating ||
       g->frame_game.counter || !carry_valid(s))return -1;
    g->players[0].inventory=s->inventory;g->score=s->score;
    g->journal=s->journal;
    saved.current=s->weapon;saved.requested=(uint16_t)s->weapon;saved.weapons=s->inventory.weapons;
    memcpy(saved.ammo,s->inventory.ammo,sizeof saved.ammo);
    g->player.view.third_person=(uint8_t)s->third_person;
    g->previous_weapon=s->previous_weapon;
    g->alternates.enabled=s->alternate_weapons;g->alternates.dual_colts=s->dual_colts;
    g->actor_scene.screen_mirrors=s->alternate_weapons;
    g->alternates.variant=g->alternates.requested_variant=s->weapon_variant;
    g->alternates.previous_variant=s->previous_variant;g->alternates.weapon=s->weapon;
    g->alternates.left_y=s->weapon_variant?0x5000:0xdc00;
    memcpy(g->alternates.rounds,s->colt_rounds,sizeof g->alternates.rounds);
    if(g->alternates.enabled && s->colt_rounds[0]==6){
        g->alternates.reload_phase=1;g->alternates.reload_dual=s->dual_colts;
    }
    g->pickup.repeat_weapon=s->alternate_weapons && !s->dual_colts?2:0;
    g->pickup.collect_owned_weapons=1;
    g->gameplay.weapon.flags&=~UINT32_C(0x1400);
    return ssa_weapon_progress_restore(&g->players[0].inventory,&g->gameplay.weapon,&g->weapon.object,&saved);
}
int ssa_native_game_carry_next(SsaNativeCarry *out,const SsaNativeGame *g)
{
    SsaNativeCarry next;unsigned current;
    if(!out || !g || !g->finished || ssa_native_game_carry_capture(&next,g)!=1)return -1;
    current=g->gameplay.weapon.current;
    if(current>=1 && current<=10 && (next.inventory.weapons&(1u<<(current-1))) && next.inventory.ammo[current-1]>0)
        {next.weapon=current;next.weapon_variant=current==2?g->alternates.variant:0;}
    next.inventory.health=100;next.inventory.stat_c4=next.inventory.stat_c6=0;
    next.inventory.pending_count=0;
    memset(next.inventory.pending_weapons,0,sizeof next.inventory.pending_weapons);
    *out=next;return 1;
}
int ssa_native_game_init_campaign(SsaNativeGame *g,const uint8_t *rom,size_t size,
    unsigned level,unsigned difficulty,const SsaNativeCarry *progress)
{
    static const uint8_t continues[]={5,3,2};
    if(level>=12 || difficulty>=3 || (progress && !carry_valid(progress)))return -1;
    if(ssa_native_game_init_loadout(g,rom,size,level,!progress && level?difficulty*12+level+1:0)!=1)return -1;
    g->damage.difficulty=(uint8_t)difficulty;
    g->gameplay.life_game.continue_flag=g->game.life_game.continue_flag=continues[difficulty];
    return progress?ssa_native_game_carry_apply(g,progress):1;
}
void ssa_native_game_free(SsaNativeGame *g)
{
    if(!g || g->game.updating)return;
    audio_unbind(g);
    if(g->input_runtime.input)(void)ssa_pc_input_runtime_free(&g->input_runtime);
    if(g->game.frame)(void)ssa_game_runtime_free(&g->game);
    if(g->enemies.combat)(void)ssa_enemy_control_runtime_free(&g->enemies);
    if(g->entities.combat)(void)ssa_entity_runtime_free(&g->entities);
    if(g->pickup_respawn.combat)(void)ssa_pickup_respawn_runtime_free(&g->pickup_respawn);
    if(g->actor_runtime.spawn)(void)ssa_actor_animation_runtime_free(&g->actor_runtime);
    if(g->camera_trace.world)(void)ssa_weapon_aim_free(&g->camera_trace);
    if(g->projectiles.combat)(void)ssa_projectile_runtime_free(&g->projectiles);
    if(g->weapon.player)(void)ssa_weapon_runtime_free(&g->weapon);
    if(g->pickup.loaded)(void)ssa_pickup_runtime_free(&g->pickup);
    if(g->combat.spawn)(void)ssa_combat_runtime_free(&g->combat);
    if(g->queries.spawn)(void)ssa_enemy_queries_free(&g->queries);
    if(g->actor_scene.loaded)(void)ssa_actor_scene_free(&g->actor_scene);
    if(g->world_scene.loaded)(void)ssa_world_scene_free(&g->world_scene);
    if(g->gameplay.runtime)(void)ssa_player_gameplay_free(&g->gameplay);
    if(g->player.player)(void)ssa_player_runtime_free(&g->player);
    if(g->spawn.world)(void)ssa_script_spawn_free(&g->spawn);
    ssa_world_free(&g->world);ssa_slide_free(&g->slide);
    ssa_map_collision_free(&g->automap_query);
    ssa_map_collision_free(&g->use_query);
    (void)ssa_spawn_plan_free(&g->plan);
    if(g->level_runtime.map)(void)ssa_level_runtime_free(&g->level_runtime);
    if(g->water_renderer){ssa_pc_renderer_free(g->water_renderer);free(g->water_renderer);}
    ssa_water_free(&g->water_render);ssa_water_scene_free(&g->water_scene);
    ssa_map_texture_bindings_free(&g->bindings);ssa_texture_assets_free(&g->textures);
    ssa_sprite_assets_free(&g->sprite_assets);ssa_actor_assets_free(&g->actor_assets);ssa_map_assets_free(&g->map);
    memset(g,0,sizeof(*g));
}
