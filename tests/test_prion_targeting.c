#include "ssa/native_game.h"
#include "../src/combat_runtime_internal.h"
#include "../src/projectile_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks,cases,frames,selections,melee_hits,shots,unconverted,idle_cases,boss_targets,current_type,current_case;
static unsigned damage_cases,range_holds,chase_cases,health_cases;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Prion type%x case%u line%d: %s\n",current_type,current_case,__LINE__,#x);exit(1);}}while(0)
static void place(SsaNativeGame *g,unsigned slot,int32_t x,int32_t y)
{
    SsaEntity *e=&g->spawn.actors[slot].entity;
    CHECK(ssa_world_remove(&g->world,e)==1);e->body.x=x;e->body.y=y;
    CHECK(ssa_world_insert(&g->world,e)==1);e->body.z=(int32_t)e->body.floor*65536;
}
static SsaEnemyNavigation *nav(SsaNativeGame *g,unsigned slot)
{return g->spawn.actors[slot].entity.type==0xcd?&g->entities.enemies[slot]->navigation:&g->enemies.navigation[slot];}
static void tick(SsaNativeGame *g)
{CHECK(ssa_native_game_tick(g)==1);CHECK(!g->error_line);++frames;}
/* Step only the attacker with real movement/combat. The recipient's own
 * controller (notably a kamikaze's detonation) must not hide contact damage. */
static void melee_step(SsaNativeGame *g,unsigned slot,unsigned policy,unsigned slide)
{
    SsaEntity *source=&g->spawn.actors[slot].entity,*player=&g->players[0].entity;
    SsaEnemyCdFrame frame=g->game.input.entities.enemies;
    frame.ticks=1;frame.speed_factor=g->enemy_speeds[source->type-0xcd];
    if(source->type==0xcd){SsaEnemyCdRuntime *cd=g->entities.enemies[slot];
        cd->converted_combat=policy;cd->crowd_slide=slide;
        cd->game->player=player;cd->game->player_x=player->body.x;cd->game->player_y=player->body.y;
        cd->navigation.target=player; /* Contact need not be the selected foe. */
        CHECK(ssa_combat_cd_step(&g->combat,cd,&frame,&g->queries.ops,&g->cd_ops)==0);
    }else{
        g->enemies.converted_combat=policy;g->enemies.crowd_slide=slide;
        g->enemies.game.player=player;g->enemies.game.player_x=player->body.x;g->enemies.game.player_y=player->body.y;
        CHECK(ssa_enemy_control_runtime_step(&g->enemies,slot,&frame,&g->enemy_ops)==0);
    }
    ++frames;
}
static SsaNativeGame *fixture(const uint8_t *rom,unsigned type,unsigned slots[3],unsigned boss)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));unsigned i,n=0;int32_t x,y;SsaScriptActor *a;
    CHECK(g && ssa_native_game_init(g,rom,0x800000,0)==1 && g->entities.nearest_converted==1);
    for(i=0;i<100 && n<3;++i)if(g->spawn.actors[i].owned && g->spawn.actors[i].entity.type>=0xcd && g->spawn.actors[i].entity.type<=0xd6)slots[n++]=i;
    CHECK(n==3);
    for(i=0;i<100;++i)if(g->spawn.actors[i].owned && i!=slots[0] && i!=slots[1] && i!=slots[2])CHECK(ssa_script_spawn_release(&g->spawn,i)==0);
    g->spawn.actors[slots[0]].entity.type=(uint16_t)type;
    g->spawn.actors[slots[1]].entity.type=g->spawn.actors[slots[2]].entity.type=0xcd;
    if(boss)g->spawn.actors[slots[2]].entity.type=0xd7;
    g->damage.god_mode=1;g->damage.difficulty=1;g->entities.fast_converted=0;
    for(i=0;i<128 && (!g->spawn.actors[slots[0]].activation.state || !g->spawn.actors[slots[1]].activation.state || !g->spawn.actors[slots[2]].activation.state);++i)tick(g);
    CHECK(i<128);x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
    for(i=0;i<3;++i){place(g,slots[i],x+(int32_t)(256+512*i)*65536,y);
        g->spawn.actors[slots[i]].activation.health=5000;g->spawn.actors[slots[i]].entity.body.flags|=0x800;}
    a=&g->spawn.actors[slots[0]];a->entity.body.flags&=UINT16_C(0xf7ff);
    g->players[0].inventory.weapons|=64;g->players[0].inventory.ammo[6]=2;
    g->gameplay.weapon.current=g->gameplay.weapon.requested=7;g->gameplay.weapon.flags=0;
    g->players[0].motion.movement.weapon_flags=0;g->weapon.object.y=0x5000;
    CHECK(ssa_weapon_animation_idle(&g->weapon.object,&g->gameplay.weapon)==1);
    g->vertical_aim=1;g->players[0].motion.movement.heading=16384;g->players[0].motion.facing=64;g->player.free_aim=1;
    g->player.pitch=(int16_t)ssa_direction(&g->math,(int16_t)((a->entity.body.z+a->entity.body.height/2-g->player.camera.eye_z)/65536),256);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,1)==1);CHECK(ssa_pc_input_runtime_button(&g->input_runtime,1,0)==1);
    for(i=0;i<120 && !(a->entity.body.flags&0x400);++i)tick(g);
    CHECK(i<120 && a->activation.health==10000 && a->parameter34==30 && g->weapon_game.shots[6]==1);
    for(i=0;i<100;++i)if(g->projectiles.active[i])CHECK(ssa_projectile_runtime_release(&g->projectiles,i)==0);
    return g;
}
static void reset(SsaNativeGame *g,unsigned slots[3])
{
    unsigned i;int32_t x=g->players[0].entity.body.x,y=g->players[0].entity.body.y;
    for(i=0;i<3;++i){SsaScriptActor *a=&g->spawn.actors[slots[i]];
        place(g,slots[i],x+(int32_t)(256+192*i)*65536,y);
        a->activation.health=5000;a->activation.state=1;a->activation.timer=1000;a->parameter36=1000;
        a->entity.body.flags=(uint16_t)((a->entity.body.flags|0x800)&~(1u|0x20u|0x400u));
        a->animation_flags=0;
    }
    g->spawn.actors[slots[0]].entity.body.flags=(uint16_t)((g->spawn.actors[slots[0]].entity.body.flags|0x400)&~0x800u);
}
typedef struct Melee {SsaEntity *target,*source;unsigned amount;} Melee;
static int melee_damage(void *ctx,SsaCombatScope *scope)
{Melee *m=ctx;return ssa_combat_scope_melee(scope,m->target,m->source,m->amount);}
static void damage_balance(SsaNativeGame *g,unsigned slots[3])
{
    SsaScriptActor *a=&g->spawn.actors[slots[0]],*b=&g->spawn.actors[slots[1]];
    SsaEntity projectile={0},*player=&g->players[0].entity;
    unsigned policy,mask,kind;projectile.type=0x66;
    for(policy=0;policy<2;++policy)for(mask=0;mask<4;++mask)for(kind=0;kind<3;++kind){
        unsigned expected=30;Melee m={&b->entity,&a->entity,30};
        reset(g,slots);a->entity.body.flags&=UINT16_C(0xfbff);
        if(mask&1)a->entity.body.flags|=0x400;
        if(mask&2)b->entity.body.flags|=0x400;
        g->combat.prion_balance=policy;
        if(policy && mask)expected=kind==0?15:60;
        if(kind==0)CHECK(ssa_combat_scope_run(&g->combat,&g->combat_ops,melee_damage,&m)==0);
        else if(kind==1)CHECK(ssa_combat_runtime_damage(&g->combat,&b->entity,&projectile,&a->entity,30,&g->combat_ops)==0);
        else {
            /* A direct enemy-source blast is not melee. Zero radius applies
             * full damage to visible ordinary enemies in this shared sector. */
            CHECK(ssa_combat_runtime_explode(&g->combat,&a->entity,&a->entity,NULL,0,30,0,&g->combat_ops)==0);
        }
        CHECK(b->activation.health==5000-(int)expected);++damage_cases;
    }
    g->combat.prion_balance=1;
    /* Both directions of player combat retain their original damage. */
    for(kind=0;kind<3;++kind){
        Melee m={player,&a->entity,30};reset(g,slots);
        g->damage.god_mode=0;g->players[0].inventory.health=1000;
        g->players[0].inventory.stat_c4=g->players[0].inventory.stat_c6=0;
        player->body.flags&=UINT16_C(0xfffe);
        if(!kind)CHECK(ssa_combat_scope_run(&g->combat,&g->combat_ops,melee_damage,&m)==0);
        else if(kind==1)CHECK(ssa_combat_runtime_damage(&g->combat,player,&projectile,&a->entity,30,&g->combat_ops)==0);
        else CHECK(ssa_combat_runtime_explode(&g->combat,&a->entity,&a->entity,NULL,0,30,1,&g->combat_ops)==0);
        CHECK(g->players[0].inventory.health==980); /* Original difficulty1: 2/3. */
        a->entity.body.flags&=UINT16_C(0xfffe);
        CHECK(ssa_combat_runtime_damage(&g->combat,&a->entity,&projectile,player,30,&g->combat_ops)==0);
        CHECK(a->activation.health==4970);++damage_cases;
    }
    g->damage.god_mode=1;g->players[0].inventory.health=1000;player->body.flags&=UINT16_C(0xfffe);
}
static void tactics(SsaNativeGame *g,unsigned slots[3],unsigned type)
{
    SsaScriptActor *a=&g->spawn.actors[slots[0]],*b=&g->spawn.actors[slots[1]];
    SsaEnemyNavigation *n=nav(g,slots[0]);int32_t dx[2];unsigned policy;
    unsigned ranged=type==0xcf || type==0xd1 || type==0xd2 || type==0xd3 || type==0xd4 || type==0xd6;
    for(policy=0;policy<2;++policy){int32_t before;
        reset(g,slots);g->combat.prion_balance=policy;
        n->phase=2;n->target=&b->entity;n->target_timer=600;
        n->waypoint_x=(uint16_t)((uint32_t)b->entity.body.x>>16);n->waypoint_y=(uint16_t)((uint32_t)b->entity.body.y>>16);
        a->activation.facing=n->desired_facing=192;before=a->entity.body.x;
        /* Full dispatch supplies the selected enemy as controller target. */
        tick(g);dx[policy]=a->entity.body.x-before;
    }
    CHECK(dx[0]>0);
    if(ranged){CHECK(dx[1]==0);++range_holds;}
    else {CHECK(dx[1]>0 && dx[1]<=dx[0]/2+1);++chase_cases;}
    /* Out of firing range, a ranged actor still closes on its target. */
    if(ranged){
        int32_t before;reset(g,slots);
        place(g,slots[1],a->entity.body.x+900*65536,a->entity.body.y);
        g->spawn.actors[slots[2]].activation.health=0;
        n->phase=2;n->target=&b->entity;n->target_timer=600;
        n->waypoint_x=(uint16_t)((uint32_t)b->entity.body.x>>16);n->waypoint_y=(uint16_t)((uint32_t)b->entity.body.y>>16);
        a->activation.facing=n->desired_facing=192;before=a->entity.body.x;
        melee_step(g,slots[0],1,1);CHECK(a->entity.body.x>before);++chase_cases;
    }
}
typedef struct PrionShot {SsaNativeGame *g;SsaEntity *target;SsaProjectile *out;} PrionShot;
static int create_prion(void *ctx,SsaCombatScope *scope)
{
    PrionShot *p=ctx;SsaProjectileCreateRequest q={0x69,&p->g->players[0].entity,80*65536,0,32*65536,16384,0};
    return ssa_projectile_runtime_enemy_create_scoped(&p->g->projectiles,scope,&q,p->target,1,&p->out);
}
static void conversion_health(SsaNativeGame *g,unsigned slots[3])
{
    SsaScriptActor *a=&g->spawn.actors[slots[0]];
    static const int health[]={17,20000,71,0,17};
    static const int expected[]={34,32767,71,0,17};unsigned mode,i,slot;
    for(mode=0;mode<5;++mode){PrionShot shot={g,&a->entity,NULL};
        reset(g,slots);a->activation.health=(int16_t)health[mode];a->parameter36=17;
        if(mode!=2)a->entity.body.flags&=UINT16_C(0xfbff); /* Repeat hit must not boost twice. */
        g->combat.prion_balance=mode!=4;
        CHECK(ssa_combat_scope_run(&g->combat,&g->combat_ops,create_prion,&shot)==0 && shot.out);
        shot.out->entity->body.flags&=UINT16_C(0xbfff); /* PC detached flight. */
        for(slot=0;slot<100 && shot.out!=&g->projectiles.projectiles[slot];++slot){}
        CHECK(slot<100);
        /* Only advance this real projectile, avoiding AI drain between impact
         * and the canonical health observation. */
        for(i=0;i<120 && g->projectiles.active[slot];++i)
            CHECK(ssa_projectile_runtime_step(&g->projectiles,slot,1,&g->projectile_ops)>=0);
        CHECK(i<120 && (a->entity.body.flags&0x400));
        CHECK(a->activation.health==expected[mode] && a->parameter36==17);++health_cases;
        for(i=0;i<100;++i)if(g->projectiles.active[i])CHECK(ssa_projectile_runtime_release(&g->projectiles,i)==0);
    }
    g->combat.prion_balance=1;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned type;
    CHECK(argc==2 && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    for(type=0xcd;type<=0xd6;++type){
        unsigned slots[3],i;SsaNativeGame *g;SsaEnemyNavigation *n;SsaEntity *expected;
        int32_t x,y;current_type=type;g=fixture(rom,type,slots,0);n=nav(g,slots[0]);
        x=g->players[0].entity.body.x;y=g->players[0].entity.body.y;
        conversion_health(g,slots);damage_balance(g,slots);tactics(g,slots,type);
        for(current_case=0;current_case<7;++current_case){
            reset(g,slots);expected=&g->spawn.actors[slots[1]].entity;
            /* A stale player preference and long original timer must not win. */
            n->target=&g->players[0].entity;n->target_timer=1000;n->phase=0;
            if(current_case==1 || current_case==3 || current_case==4){
                place(g,slots[2],x+416*65536,y);expected=&g->spawn.actors[slots[2]].entity;
            }
            if(current_case==2){place(g,slots[1],x+768*65536,y);expected=&g->players[0].entity;}
            if(current_case==3)g->spawn.actors[slots[1]].activation.health=0;
            if(current_case==4){g->spawn.actors[slots[1]].entity.body.flags|=0x20;g->spawn.actors[slots[2]].entity.body.flags|=0x400;}
            if(current_case==5){g->spawn.actors[slots[1]].activation.health=0;g->spawn.actors[slots[2]].entity.body.flags|=0x20;expected=&g->players[0].entity;}
            if(current_case==6){g->spawn.actors[slots[1]].entity.body.z+=1024*65536;expected=&g->players[0].entity;}
            tick(g);CHECK(n->target==expected && n->target_timer<=600);
            CHECK(g->enemies.game.player==&g->players[0].entity);
            CHECK(g->game.cd_game.player==&g->players[0].entity);++selections;
            /* Change the winner with the old retarget timer still live. */
            if(current_case==0){place(g,slots[2],x+416*65536,y);tick(g);
                CHECK(n->target==&g->spawn.actors[slots[2]].entity);++selections;}
        }
        reset(g,slots);n->target=&g->spawn.actors[slots[1]].entity;n->target_timer=1000;n->phase=2;
        n->waypoint_x=(uint16_t)((x+448*65536)/65536);n->waypoint_y=(uint16_t)(y/65536);
        g->spawn.actors[slots[0]].activation.facing=n->desired_facing=192;
        {
            unsigned slide,mode;
            for(slide=0;slide<2;++slide)for(mode=0;mode<6;++mode){
            SsaScriptActor *a=&g->spawn.actors[slots[0]],*target=&g->spawn.actors[slots[1]];int16_t old=target->activation.health;
            reset(g,slots);n->target=&target->entity;n->phase=2;
            a->activation.facing=n->desired_facing=192;
            if(mode==1 || mode==2)a->entity.body.flags&=UINT16_C(0xfbff);
            if(mode==1 || mode==3 || mode==5)target->entity.body.flags|=0x400;
            if(mode==4)target->activation.health=0;
            old=target->activation.health;
            place(g,slots[1],a->entity.body.x+a->entity.radius+target->entity.radius-65536,y);
            n->waypoint_x=(uint16_t)(target->entity.body.x/65536);n->waypoint_y=(uint16_t)(y/65536);g->enemies.crowd_slide=slide;
            melee_step(g,slots[0],mode!=5,slide);
            if(mode==2 || mode==4 || mode==5)CHECK(target->activation.health==old);
            else{if(target->activation.health!=old-(type==0xd0 || type==0xd6?20:10))fprintf(stderr,"slide%u mode%u health%d->%d moved%u flags%x/%x\n",slide,mode,old,target->activation.health,n->move_result,a->entity.body.flags,target->entity.body.flags);
                CHECK(target->activation.health==old-(type==0xd0 || type==0xd6?20:10));++melee_hits;}
            /* Previous contact must never damage a distant enemy on a later
             * update, including a navigation pass which makes no move. */
            old=target->activation.health;place(g,slots[1],x+1024*65536,y);
            target->entity.body.flags&=UINT16_C(0xfffe); /* Do not let the hit latch hide a stale hit. */
            a->entity.body.flags|=0x800;melee_step(g,slots[0],mode!=5,slide);CHECK(target->activation.health==old);
            a->entity.body.flags&=UINT16_C(0xf7ff);n->phase=2;
            melee_step(g,slots[0],mode!=5,slide);CHECK(target->activation.health==old);
            }
            g->entities.nearest_converted=1;g->enemies.crowd_slide=1;
        }
        if(type==0xcf || type==0xd1 || type==0xd2 || type==0xd3 || type==0xd4 || type==0xd6){
            reset(g,slots);n->target=&g->spawn.actors[slots[1]].entity;n->phase=2;n->navigation_flags=1;
            n->waypoint_x=(uint16_t)((x+448*65536)/65536);n->waypoint_y=(uint16_t)(y/65536);
            g->spawn.actors[slots[0]].activation.facing=n->desired_facing=192;
            g->spawn.actors[slots[0]].activation.timer=0;
            if(type==0xd4){g->spawn.actors[slots[0]].activation.state=2;g->spawn.actors[slots[0]].animation_flags=0xfa;}
            tick(g);
            for(i=0;i<100;++i)if(g->projectiles.active[i] && g->projectiles.projectiles[i].source==&g->spawn.actors[slots[0]].entity)break;
            CHECK(i<100 && g->projectiles.projectiles[i].entity->body.velocity_x>0);++shots;
        }
        reset(g,slots);g->spawn.actors[slots[0]].entity.body.flags&=UINT16_C(0xfbff);
        n->phase=0;n->target=&g->spawn.actors[slots[1]].entity;
        {SsaEnemyMotion facing={&g->spawn.actors[slots[0]].entity,0,0,0,0};
         CHECK(ssa_enemy_face(&facing,&g->players[0].entity,&g->math)==0);tick(g);
         CHECK(n->desired_facing==facing.desired_facing);++unconverted;}
        reset(g,slots);g->spawn.actors[slots[1]].activation.health=g->spawn.actors[slots[2]].activation.health=0;
        g->players[0].inventory.health=0;n->target=&g->spawn.actors[slots[1]].entity;
        tick(g);CHECK(!n->target);++idle_cases;
        ++cases;ssa_native_game_free(g);free(g);
    }
    {unsigned slots[3];SsaNativeGame *g=fixture(rom,0xce,slots,1);SsaEntity *a=&g->spawn.actors[slots[0]].entity;
     current_type=0xce;current_case=8;
     place(g,slots[2],a->body.x+128*65536,a->body.y);
     g->spawn.actors[slots[2]].entity.body.z=a->body.z+a->body.height/2-g->spawn.actors[slots[2]].entity.body.height/2;
     tick(g);CHECK(nav(g,slots[0])->target==&g->spawn.actors[slots[2]].entity);++boss_targets;
     ssa_native_game_free(g);free(g);}
    free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"converted_types\":%u,\"frames\":%u,\"target_selections\":%u,\"melee_hits\":%u,\"ranged_shots\":%u,\"unconverted_cases\":%u,\"no_target_cases\":%u,\"boss_targets\":%u,\"damage_cases\":%u,\"range_holds\":%u,\"chase_cases\":%u,\"health_cases\":%u}\n",checks,cases,frames,selections,melee_hits,shots,unconverted,idle_cases,boss_targets,damage_cases,range_holds,chase_cases,health_cases);return 0;
}
