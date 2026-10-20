#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"

static unsigned special_requests,fall_particles,blast_cases,fall_cases,full_cases,native_cases;
static unsigned timed_cases,denied_cases,timed_frames,smooth_frames,empty_animation_frames;
static unsigned prion_speed_cases,prion_speed_frames;
static unsigned frozen_render_cases;
static int request_event(void *context,const SsaWeaponRuntime *r,SsaWeaponEvent *e)
{
    WeaponCase *w=context;
    if(e->kind!=SSA_WEAPON_REQUEST)return weapon_effect(context,r,e);
    weapon_busy(w);CHECK((uint32_t)e->args[0]==(w->gameplay.weapon.current==8?0x80000009u:0x40000009u));
    w->damage.hurt_flash=(uint32_t)e->args[0];++special_requests;return 0;
}
static int fall_particle(void *context,const SsaProjectileRuntime *r,unsigned id,int32_t x,int32_t y,int32_t z)
{
    ProjectileCase *t=context;
    if(id!=7)return projectile_particle(context,r,id,x,y,z);
    CHECK(r==&t->runtime);projectile_busy(t,projectile_first(t));++fall_particles;
    return t->fail_particle?-1:0;
}
static void fire_special(ProjectileCase *t,unsigned weapon)
{
    unsigned before=t->weapon.game.shots[weapon-1],i;
    select_for_test(&t->weapon,weapon);
    for(i=0;i<128 && t->weapon.game.shots[weapon-1]==before;++i){
        CHECK(weapon_tick(&t->weapon,1,i==0)==0);weapon_animate(&t->weapon);
    }
    CHECK(t->weapon.game.shots[weapon-1]==before+1);
}
static void special_case(uint8_t *rom,size_t size,const SsaMathTables *math,
 const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
 const SsaWeaponAnimations *anim,unsigned thing,int32_t speed,unsigned level,unsigned weapon,int exhaust)
{
    Fixture f;ProjectileCase t;CdCase c;SsaScriptActor *a;unsigned i,slot,requests=special_requests;
    setup(&f,rom,size,level,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
    t.weapon.ops.event=request_event;t.ops.particle=fall_particle;
    script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
    a=&f.spawn.actors[c.runtime.slot];position_target(&t.weapon,&c);
    if(exhaust){
        /* Fill real shared slots with live projectiles, not fake occupancy. */
        f.players[0].inventory.ammo[5]=200;
        for(i=0;i<98;++i)projectile_fire(&t,6);
        CHECK(f.spawn.pool.active_count==100 && f.world.entity_count==100);
    }
    fire_special(&t,weapon);
    CHECK(special_requests==requests+1 && f.players[0].inventory.ammo[weapon-1]==99);
    CHECK(f.players[0].inventory.health==100 && !t.weapon.unsupported);
    if(weapon==8){
        CHECK(!a->activation.health && t.weapon.score==2000 && t.weapon.weapon.hits[7]==1);
        CHECK(t.weapon.weapon.controller.cooldown[7]==120 && !projectile_count(&t));
        /* The original input release gate still applies after its cooldown. */
        CHECK(weapon_tick(&t.weapon,0,0)==0);
        CHECK(weapon_tick(&t.weapon,1,1)==0);
        CHECK(t.weapon.game.shots[7]==1 && t.weapon.weapon.controller.cooldown[7]==118);
        ++blast_cases;
    }else if(exhaust){
        CHECK(projectile_count(&t)==98 && a->activation.health==30 && (a->entity.body.flags&0x800));
        CHECK(f.spawn.pool.active_count==100 && f.world.entity_count==100);++full_cases;
    }else{
        SsaProjectile *p;int32_t z;
        slot=projectile_first(&t);CHECK(slot<100 && projectile_count(&t)==1);p=&t.runtime.projectiles[slot];
        CHECK(p->entity->type==0xe3 && p->source==&a->entity && p->weapon==9 && p->owner==0);
        CHECK(p->entity->body.flags==0x20 && p->entity->body.height==0 && !p->entity->body.velocity_z);
        CHECK(p->entity->body.x==a->entity.body.x && p->entity->body.y==a->entity.body.y);
        CHECK(p->entity->body.z==a->entity.body.z+a->entity.body.height+400*65536);
        CHECK((a->entity.body.flags&0x800) && a->activation.health==30 && t.weapon.score==0);
        z=p->entity->body.z;
        for(i=0;i<128 && t.runtime.active[slot];++i){
            unsigned landed=p->state;
            CHECK(projectile_tick(&t,slot,1)>=0);
            if(t.runtime.active[slot]){
                CHECK(p->entity->body.z<=z);z=p->entity->body.z;
                CHECK(a->activation.health==30); /* Landing precedes explosion. */
                if(p->state==1)CHECK(p->entity->body.z==(int32_t)p->entity->body.floor*65536);
            }else CHECK(landed==1);
        }
        CHECK(i<128 && !t.runtime.active[slot] && !projectile_count(&t));
        CHECK(!a->activation.health && !(a->entity.body.flags&0x800));
        /* Original E3 has no player owner: do not invent kill credit. */
        CHECK(!t.weapon.score && !t.weapon.weapon.hits[8] && !a->group_released);++fall_cases;
    }
    projectile_cleanup(&t);cleanup(&f);
}
static void native_special(const uint8_t *rom,unsigned weapon,unsigned third)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer renderer={0};unsigned i,j,target=100,slot=100;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    g->damage.god_mode=1;CHECK(ssa_native_game_tick(g)==1);
    if(weapon==9){
        for(j=0;j<100;++j)if(g->spawn.actors[j].owned && g->spawn.actors[j].entity.type==0xcf){target=j;break;}
        CHECK(target<100);
        for(i=0;i<128 && !g->spawn.actors[target].activation.health;++i)CHECK(ssa_native_game_tick(g)==1);
        CHECK(g->spawn.actors[target].activation.health>0);
        for(j=0;j<4;++j){static const int dx[]={0,1,0,-1},dy[]={1,0,-1,0};int32_t sine,cosine,slope;SsaWeaponHit hit;
            SsaEntity *e=&g->spawn.actors[target].entity,*p=&g->players[0].entity;
            /* Move this fixture target off its original raised ledge before
             * querying. Keep its recovered hitbox and normal controller. */
            CHECK(ssa_world_remove(&g->world,e)==1);
            e->body.x=p->body.x+dx[j]*80*65536;e->body.y=p->body.y+dy[j]*80*65536;
            CHECK(ssa_world_insert(&g->world,e)==1);e->body.z=(int32_t)e->body.floor*65536;
            g->players[0].motion.movement.heading=(uint16_t)(j*16384);g->players[0].motion.facing=(uint8_t)(j*64);
            g->player.pitch=(int16_t)ssa_direction(&g->math,(int16_t)((e->body.z+e->body.height/2-g->player.camera.eye_z)/65536),80);
            ssa_sincos(&g->math,(uint16_t)g->player.pitch,&sine,&cosine);slope=(int32_t)((int64_t)sine*65536/cosine);
            CHECK(ssa_weapon_hit_query_free(&g->weapon.trace,p,(uint16_t)(j*16384),g->player.camera.eye_z-p->body.z,
                2048*65536,slope,&hit)==1);
            if(hit.target==e)break;
        }
        CHECK(j<4);
        /* Controlled native frontend fixture: nearby target, actual hitbox,
         * world, input, weapon runtime and entity scheduler remain active. */
        g->player.free_aim=1;
    }
    g->players[0].inventory.weapons|=(uint16_t)(1u<<(weapon-1));g->players[0].inventory.ammo[weapon-1]=2;
    g->gameplay.weapon.current=g->gameplay.weapon.requested=(uint16_t)weapon;
    g->gameplay.weapon.flags=0;g->players[0].motion.movement.weapon_flags=0;g->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
    if(third)CHECK(ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    for(i=0;i<120 && !g->weapon_game.shots[weapon-1];++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(i<120 && !g->error_line && g->players[0].inventory.ammo[weapon-1]==1);
    CHECK((g->damage.hurt_flash&0xe0000000)==(weapon==8?0x80000000u:0x40000000u));
    CHECK(g->player.view.third_person==third);
    if(weapon==9){
        for(j=0;j<100;++j)if(g->projectiles.active[j] && g->projectiles.projectiles[j].entity->type==0xe3){slot=j;break;}
        CHECK(slot<100);
    }
    {uint32_t request=g->damage.hurt_flash;uint8_t counter=g->palette_effect.counter;
        CHECK(ssa_pc_renderer_resize(&renderer,320,240)==1);
        for(j=0;j<3;++j){CHECK(ssa_native_game_draw(g)==1);CHECK(ssa_pc_renderer_draw(&renderer,g)==1);}
        CHECK(g->damage.hurt_flash==request && g->palette_effect.counter==counter);
    }
    for(i=0;i<120 && (g->damage.hurt_flash || (weapon==9 && g->projectiles.active[slot]));++i){
        CHECK(ssa_native_game_tick(g)==1);
        if(weapon==9 && g->projectiles.active[slot]){SsaProjectile *fall=&g->projectiles.projectiles[slot];
            SsaBody before=fall->entity->body;int32_t z;unsigned at;
            CHECK(ssa_projectile_runtime_render_z(&g->projectiles,slot,&z)==1);
            CHECK(ssa_native_game_prepare_view(g)==1);
            for(at=0;at<g->spawn.pool.active_count && g->sources[at].entity!=fall->entity;++at){}
            CHECK(at<g->spawn.pool.active_count && (int64_t)before.z+g->source_z_offsets[at]==z);
            if(i==14 || i==35){CHECK(ssa_native_game_draw(g)==1);CHECK(ssa_pc_renderer_draw(&renderer,g)==1);}
            CHECK(!memcmp(&before,&fall->entity->body,sizeof before));
        }
    }
    CHECK(i<120 && !g->damage.hurt_flash);
    /* The request1 restore copies raw palette entry0; the following request0
     * presentation clears that backdrop, exactly as0750C does. */
    CHECK(ssa_native_game_tick(g)==1);
    CHECK(!memcmp(g->palette_effect.colors,g->sprite_assets.background_palette[0],512));
    CHECK(g->weapon_game.shots[weapon-1]==1 && g->players[0].inventory.ammo[weapon-1]==1);
    if(weapon==9){CHECK(!g->projectiles.active[slot]);CHECK(!g->spawn.actors[target].activation.health);}
    if(weapon==8 && !third){
        /* A hurt flash must expire so a later hit can trigger its sound again. */
        unsigned sounds=g->sound_serial;
        for(j=0;j<2;++j){SsaEntity *p=&g->players[0].entity;int16_t health=g->players[0].inventory.health;
            g->damage.god_mode=0;p->body.flags&=UINT16_C(0xfffe);
            CHECK(ssa_combat_runtime_damage(&g->combat,p,p,NULL,3,&g->combat_ops)==0);
            CHECK(g->players[0].inventory.health==health-2 && g->damage.hurt_flash==9);
            CHECK(g->sound_serial==sounds+j+1);
            for(i=0;i<32 && g->damage.hurt_flash;++i)CHECK(ssa_native_game_tick(g)==1);
            CHECK(i<32 && !g->damage.hurt_flash);
        }
    }
    ++native_cases;ssa_pc_renderer_free(&renderer);ssa_native_game_free(g);free(g);
}
static void timed_strike(uint8_t *rom,size_t size,const SsaMathTables *math,
 const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
 const SsaWeaponAnimations *anim,unsigned thing,int32_t speed,unsigned difficulty,unsigned kind)
{
    static const unsigned types[]={0xcd,0xce,0xd6,0xd7},healths[]={30,100,150,3000},damage[]={150,100,66};
    Fixture f;ProjectileCase t;CdCase c;SsaScriptActor *a;SsaProjectile *p;unsigned i,slot;
    setup(&f,rom,size,0,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
    t.weapon.ops.event=request_event;t.ops.particle=fall_particle;
    script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
    a=&f.spawn.actors[c.runtime.slot];position_target(&t.weapon,&c);
    a->entity.type=(uint16_t)types[kind];a->activation.health=(int16_t)healths[kind];
    t.weapon.damage.difficulty=(uint8_t)difficulty;t.runtime.time_strike_gba_clock=1;
    t.weapon.weapon.time_strike_requires_target=1;fire_special(&t,9);
    slot=projectile_first(&t);CHECK(slot<100);p=&t.runtime.projectiles[slot];
    CHECK(!t.runtime.time_strike_phase[slot] && !t.runtime.time_strike_tick_phase[slot]);
    {int32_t initial=p->entity->body.z,previous=initial;unsigned animation=a->animation,smoothed=0;
        for(i=1;i<=67;++i){
            CHECK(projectile_tick(&t,slot,1)>=0);++timed_frames;
            if(i<=6)CHECK(p->entity->body.z==initial && !p->entity->body.velocity_z);
            if(i==7)CHECK(p->entity->body.z==initial-12*65536 && p->entity->body.velocity_z==-2*65536);
            if(i==14)CHECK(p->entity->body.z==initial-40*65536 && p->entity->body.velocity_z==-4*65536);
            if(i<67)CHECK(t.runtime.active[slot] && a->activation.health==(int16_t)healths[kind]);
            if(t.runtime.active[slot]){int32_t z,again;SsaBody before=p->entity->body;
                CHECK(ssa_projectile_runtime_render_z(&t.runtime,slot,&z)==1);
                CHECK(ssa_projectile_runtime_render_z(&t.runtime,slot,&again)==1 && z==again);
                CHECK(!memcmp(&before,&p->entity->body,sizeof before));
                CHECK(z<=p->entity->body.z && z>=(int32_t)p->entity->body.floor*65536 && z<=previous);
                CHECK((int64_t)previous-z<=32*65536);
                if(z<previous){++smooth_frames;++smoothed;}
                if(i==1)CHECK(z<initial && p->entity->body.z==initial);
                previous=z;
            }
            CHECK(a->animation==animation);
        }
        CHECK(smoothed>=50);
    }
    CHECK(!t.runtime.active[slot] && !t.runtime.time_strike_phase[slot] && !t.runtime.time_strike_tick_phase[slot]);
    CHECK(a->activation.health==(int16_t)(healths[kind]>damage[difficulty]?healths[kind]-damage[difficulty]:0));
    CHECK(!(a->entity.body.flags&0x800) && t.weapon.game.shots[8]==1 && f.players[0].inventory.ammo[8]==99);
    CHECK(!t.weapon.score && !t.weapon.weapon.hits[8]);
    /* Reuse the same stable slot; no old fractional clock may leak through. */
    a->activation.health=(int16_t)healths[kind];a->entity.body.flags&=UINT16_C(0xfffe);
    t.weapon.game.flags=0;fire_special(&t,9);CHECK(projectile_first(&t)==slot);
    CHECK(!t.runtime.time_strike_phase[slot]);
    CHECK(projectile_tick(&t,slot,6)==0 && !t.runtime.projectiles[slot].entity->body.velocity_z);
    CHECK(projectile_tick(&t,slot,1)==0 && t.runtime.projectiles[slot].entity->body.velocity_z==-2*65536);
    /* Batched catch-up must preserve landing-before-explosion and release. */
    CHECK(projectile_tick(&t,slot,60)==1 && !t.runtime.active[slot]);
    ++timed_cases;projectile_cleanup(&t);cleanup(&f);
}
static void denied_strike(uint8_t *rom,size_t size,const SsaMathTables *math,
 const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
 const SsaWeaponAnimations *anim,unsigned thing,int32_t speed,unsigned mode,unsigned direct)
{
    Fixture f;ProjectileCase t;CdCase c;SsaScriptActor *a;unsigned i,sounds=sound_effects,requests=special_requests,gestures=0,starts=0;
    setup(&f,rom,size,0,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
    t.weapon.ops.event=request_event;t.ops.particle=fall_particle;
    script(&f,rom,thing,1,1);step(&f,1);bind(&c,&f,math,speed);activate(&c);
    a=&f.spawn.actors[c.runtime.slot];position_target(&t.weapon,&c);select_for_test(&t.weapon,9);
    t.weapon.weapon.time_strike_requires_target=1;t.weapon.weapon.buffer_fire=1;
    f.player.free_aim=direct;f.player.pitch=0;
    if(mode==4){CHECK(weapon_tick(&t.weapon,1,1)==0);CHECK(t.weapon.gameplay.weapon.flags&0x400);}
    if(mode==0)a->entity.body.flags|=0x20; /* No ray target. */
    else if(mode==1 || mode>=4)a->activation.health=0; /* Corpse / target lost after start. */
    else if(mode==2)a->entity.type=0xe1; /* An item can intercept a free-look ray. */
    else a->entity.body.flags|=1; /* Original temporary damage protection. */
    if(mode==6)t.weapon.weapon.object.y=0x6000; /* Click before the weapon finishes raising. */
    for(i=0;i<(mode==7?96u:64u);++i){unsigned firing=t.weapon.gameplay.weapon.flags&0x400;
        if((mode==5 || mode==6) && i==2)a->activation.health=30; /* Target returns before this attempt completes. */
        CHECK(weapon_tick(&t.weapon,i==0 || (mode==7 && i==2),(i==0 && mode!=4) || (mode==7 && i==2))==0);
        if(!firing && (t.weapon.gameplay.weapon.flags&0x400))++starts;
        if(t.weapon.gameplay.weapon.flags&0x400){CHECK(t.weapon.weapon.object.animation==18);++gestures;++empty_animation_frames;}
        weapon_animate(&t.weapon);
    }
    CHECK(gestures>1);
    if(mode==6)CHECK(starts==1);
    if(mode==7)CHECK(starts==2);
    CHECK(!t.weapon.game.shots[8] && f.players[0].inventory.ammo[8]==100 && !projectile_count(&t));
    CHECK(sound_effects==sounds && special_requests==requests && !t.weapon.game.request);
    CHECK(!(t.weapon.gameplay.weapon.flags&0x400) && !t.weapon.weapon.fire_pending_weapon && !t.weapon.weapon.time_strike_empty_pending);
    /* A discarded click never fires later when a healthy target reappears. */
    a->activation.health=30;a->entity.type=0xcd;a->entity.body.flags&=UINT16_C(0xffde);
    for(i=0;i<8;++i){CHECK(weapon_tick(&t.weapon,0,0)==0);weapon_animate(&t.weapon);}
    CHECK(!t.weapon.game.shots[8] && sound_effects==sounds);
    fire_special(&t,9);CHECK(t.weapon.game.shots[8]==1 && f.players[0].inventory.ammo[8]==99);
    CHECK(sound_effects==sounds+1 && special_requests==requests+1 && projectile_count(&t)==1);
    ++denied_cases;projectile_cleanup(&t);cleanup(&f);
}
static void place_prion_fixture(SsaNativeGame *g,unsigned slot,int32_t x,int32_t y)
{
    SsaEntity *e=&g->spawn.actors[slot].entity;const uint8_t *sector;
    CHECK(ssa_world_remove(&g->world,e)==1);e->body.x=x;e->body.y=y;
    CHECK(ssa_world_insert(&g->world,e)==1);sector=g->map.data+g->map.sectors+(unsigned)e->sector*20;
    e->body.floor=(int16_t)(sector[0]|(unsigned)sector[1]<<8);e->body.z=(int32_t)e->body.floor*65536;
}
static void prion_speed_case(const uint8_t *rom,unsigned type)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaScriptActor *a,*other;SsaEnemyNavigation *nav;
    unsigned slot=100,second=100,i,mode;int16_t health;int32_t x,y;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
    CHECK(g->entities.fast_converted==1 && g->actor_runtime.fast_converted==1);
    g->entities.nearest_converted=0; /* Isolate the behavior-clock regression. */
    g->combat.prion_balance=0; /* Chase/health policy has its own Prion fixture. */
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && g->spawn.actors[i].entity.type>=0xcd && g->spawn.actors[i].entity.type<=0xd6){
        if(slot==100)slot=i;else{second=i;break;}
    }
    CHECK(second<100);a=&g->spawn.actors[slot];other=&g->spawn.actors[second];CHECK(!a->activation.state);
    a->entity.type=(uint16_t)type;other->entity.type=0xcd;
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && i!=slot && i!=second)CHECK(ssa_script_spawn_release(&g->spawn,i)==0);
    g->damage.god_mode=1;g->entities.fast_converted=g->actor_runtime.fast_converted=0;
    for(i=0;i<128 && (!a->activation.state || !other->activation.state);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(i<128 && a->activation.health>0 && other->activation.health>0);
    x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
    place_prion_fixture(g,slot,x+256*65536,y);place_prion_fixture(g,second,x+768*65536,y);
    other->entity.body.flags|=0x800;other->activation.health=3000;
    g->players[0].inventory.weapons|=64;g->players[0].inventory.ammo[6]=2;
    g->gameplay.weapon.current=g->gameplay.weapon.requested=7;
    g->gameplay.weapon.flags=g->players[0].motion.movement.weapon_flags=0;g->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
    g->players[0].motion.movement.heading=16384;g->players[0].motion.facing=64;g->player.free_aim=1;
    g->player.pitch=(int16_t)ssa_direction(&g->math,(int16_t)((a->entity.body.z+a->entity.body.height/2-g->player.camera.eye_z)/65536),256);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    for(i=0;i<120 && !(a->entity.body.flags&0x400);++i)CHECK(ssa_native_game_tick(g)==1);
    CHECK(i<120 && a->activation.health>0 && a->parameter34==30 && g->weapon_game.shots[6]==1);
    for(i=0;i<100;++i)if(g->projectiles.active[i])CHECK(ssa_projectile_runtime_release(&g->projectiles,i)==0);
    nav=type==0xcd?&g->entities.enemies[slot]->navigation:&g->enemies.navigation[slot];
    /* Controlled timer/animation starting points after an actual native Prion
     * hit. All subsequent updates use the full native game scheduler. */
    for(mode=0;mode<4;++mode){unsigned amount=mode==1?2u:1u;
        place_prion_fixture(g,slot,x+256*65536,y);
        a->activation.state=1;a->activation.timer=1000;a->parameter36=1000;
        a->entity.body.flags=(uint16_t)((a->entity.body.flags|0x400)&~(1u|0x20u|0x800u));
        if(mode==2)a->entity.body.flags&=UINT16_C(0xfbff);
        nav->target=&other->entity;nav->target_timer=1000;
        nav->phase=2;nav->waypoint_x=(uint16_t)(other->entity.body.x/65536);nav->waypoint_y=(uint16_t)(other->entity.body.y/65536);
        a->activation.facing=nav->desired_facing=192;
        a->animation_counter=0;a->animation_ticks=100;a->animation_flags=0;
        g->entities.fast_converted=g->actor_runtime.fast_converted=mode!=0;
        health=a->activation.health;
        if(mode==3)g->frame_game.flags|=1;
        CHECK(ssa_native_game_tick(g)==1);++prion_speed_frames;
        if(mode==3){CHECK(a->parameter36==1000 && !a->animation_counter);g->frame_game.flags&=UINT16_C(0xfffe);}
        else{CHECK(a->parameter36==1000-amount && a->activation.health==health);
            CHECK(a->animation_counter==amount);
            CHECK(g->entities.dispatch.walk_count==amount && a->entity.body.x>x+256*65536);
            if(mode<2)CHECK(nav->target_timer==1000-amount);
        }
    }
    /* Death and corpse expiry retain their normal clock and notify once. */
    CHECK(ssa_combat_runtime_damage(&g->combat,&a->entity,&g->players[0].entity,NULL,10000,&g->combat_ops)==0);
    CHECK(!a->activation.health);CHECK(ssa_native_game_tick(g)==1);++prion_speed_frames;
    {uint16_t timer=a->activation.timer;
        CHECK(a->owned && a->group_released);a->animation_counter=0;a->animation_ticks=100;
        CHECK(ssa_native_game_tick(g)==1);++prion_speed_frames;
        CHECK(a->owned && a->activation.timer==(uint16_t)(timer-1) && a->animation_counter==1);
    }
    if(type==0xcd){
        g->weapon.fire_pending_weapon=9;g->weapon.time_strike_empty_pending=1;
        ssa_pc_input_runtime_focus(&g->input_runtime,0);
        CHECK(!g->weapon.fire_pending_weapon && !g->weapon.time_strike_empty_pending);
        ssa_pc_input_runtime_focus(&g->input_runtime,1);
        g->weapon.fire_pending_weapon=9;g->weapon.time_strike_empty_pending=1;
        ssa_pc_input_runtime_enable(&g->input_runtime,0);
        CHECK(!g->weapon.fire_pending_weapon && !g->weapon.time_strike_empty_pending);
        ssa_pc_input_runtime_enable(&g->input_runtime,1);
        g->weapon.fire_pending_weapon=9;g->weapon.time_strike_empty_pending=1;
        g->players[0].motion.movement.flags&=UINT16_C(0xfffb);
        CHECK(ssa_weapon_runtime_step(&g->weapon,&g->weapon_game,0,0,&g->weapon_ops)==0);
        CHECK(!g->weapon.fire_pending_weapon && !g->weapon.time_strike_empty_pending);
        g->players[0].motion.movement.flags|=4;
    }
    ++prion_speed_cases;ssa_native_game_free(g);free(g);
}
static void frozen_animation_case(const uint8_t *rom,unsigned third)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer renderer={0};SsaScriptActor *a;
    unsigned slot=100,i,mode,found;uint16_t expected;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1);
    g->damage.god_mode=1;CHECK(ssa_native_game_tick(g)==1);
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && g->spawn.actors[i].entity.type>=0xcd && g->spawn.actors[i].entity.type<=0xd6){slot=i;break;}
    CHECK(slot<100);a=&g->spawn.actors[slot];
    place_prion_fixture(g,slot,g->players[0].entity.body.x+256*65536,g->players[0].entity.body.y);
    a->entity.body.height=120*65536;
    g->players[0].motion.movement.heading=16384;g->players[0].motion.facing=64;
    g->player.view.third_person=(uint8_t)third;g->player.free_aim=1;g->player.pitch=0;
    CHECK(ssa_view_pose_pitch(&g->player.view,&g->players[0].entity.body,&g->player.camera,16384,0,
        &g->math,&g->player.view_collision,&g->player.pose)==1);
    CHECK(ssa_pc_renderer_resize(&renderer,640,480)==1);
    CHECK(g->actor_assets.resources[0x48].frames==2 && g->actor_animations.words[g->actor_animations.offsets[0x48]]==0);
    for(mode=0;mode<6;++mode){SsaScriptActor before;
        a->animation=0x48;a->animation_frame=0;a->animation_display_frame=4;a->entity.body.flags|=0x800;
        expected=0;
        if(mode==1){a->animation_display_frame=1;expected=1;} /* Valid frozen frame. */
        if(mode==2){a->entity.body.flags&=UINT16_C(0xf7ff);expected=4;} /* No repair outside freeze. */
        if(mode==3){a->animation_frame=1;expected=4;} /* No guessed playback position. */
        if(mode==4){a->animation_display_frame=0xfffe;expected=0xfffe;}
        if(mode==5){a->animation_display_frame=0xffff;expected=0xffff;}
        before=*a;CHECK(ssa_native_game_prepare_view(g)==1);found=0;
        for(i=0;i<g->spawn.pool.active_count;++i)if(g->sources[i].entity==&a->entity){CHECK(g->sources[i].frame==expected);++found;}
        CHECK(found==1 && !memcmp(a,&before,sizeof before));
        if(mode!=2 && mode!=3){
            CHECK(ssa_native_game_draw(g)==1);
            if(mode<=1){found=0;
                for(i=0;i<g->actor_scene.queue.projection_count;++i)if(g->actor_scene.queue.projections[i].animation==0x48){
                    CHECK((g->actor_scene.queue.projections[i].frame_flags&0x3fffffffu)<=g->actor_assets.resources[0x48].count);++found;
                }
                CHECK(found==1);
            }
            CHECK(ssa_pc_renderer_draw(&renderer,g)==1);
            g->render_pitch=5440;CHECK(ssa_pc_renderer_draw(&renderer,g)==1);g->render_pitch=0;
            CHECK(!memcmp(a,&before,sizeof before));
        }
        ++frozen_render_cases;
    }
    ssa_pc_renderer_free(&renderer);ssa_native_game_free(g);free(g);
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);FILE *f;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inv;SsaWeaponRules rules;SsaWeaponAnimations anim;int32_t speeds[12];unsigned level,i,thing=100;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);CHECK(!fclose(f));
    CHECK(ssa_math_tables_load(&math,rom,0x800000));CHECK(ssa_spawn_definitions_load(&defs,rom,0x800000));
    CHECK(ssa_inventory_rules_load(&inv,rom,0x800000)==0 && ssa_weapon_rules_load(&rules,rom,0x800000)==1);
    CHECK(ssa_weapon_animations_load(&anim,rom,0x800000)==1 && ssa_enemy_navigation_speeds(speeds,rom,0x800000)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level){
        special_case(rom,0x800000,&math,&defs,&inv,&rules,&anim,thing,speeds[0],level,8,0);
        special_case(rom,0x800000,&math,&defs,&inv,&rules,&anim,thing,speeds[0],level,9,0);
    }
    special_case(rom,0x800000,&math,&defs,&inv,&rules,&anim,thing,speeds[0],0,9,1);
    for(level=0;level<3;++level)for(i=0;i<4;++i)
        timed_strike(rom,0x800000,&math,&defs,&inv,&rules,&anim,thing,speeds[0],level,i);
    for(level=0;level<2;++level)for(i=0;i<8;++i)
        denied_strike(rom,0x800000,&math,&defs,&inv,&rules,&anim,thing,speeds[0],i,level);
    for(i=0;i<2;++i){native_special(rom,8,i);native_special(rom,9,i);}
    for(i=0xcd;i<=0xd6;++i)prion_speed_case(rom,i);
    for(i=0;i<2;++i)frozen_animation_case(rom,i);
    printf("{\"result\":\"pass\",\"checks\":%u,\"maps\":16,\"blast_cases\":%u,\"fall_cases\":%u,\"pool_full_cases\":%u,\"native_input_cases\":%u,\"requests\":%u,\"fall_particles\":%u,\"timed_cases\":%u,\"denied_cases\":%u,\"timed_frames\":%u,\"smooth_fall_frames\":%u,\"empty_animation_frames\":%u,\"prion_speed_cases\":%u,\"prion_speed_frames\":%u,\"frozen_render_cases\":%u}\n",
        checks,blast_cases,fall_cases,full_cases,native_cases,special_requests,fall_particles,timed_cases,denied_cases,timed_frames,smooth_frames,empty_animation_frames,prion_speed_cases,prion_speed_frames,frozen_render_cases);
    free(rom);return 0;
}
