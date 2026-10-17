#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"

static unsigned start_cases,start_guards,start_failures,start_shots[10],start_flight_frames;
static unsigned start_multiplayer,start_inactive,start_cooldowns,calloc_fail_at,calloc_calls;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t count,size_t size) {
    if(calloc_fail_at && ++calloc_calls==calloc_fail_at) return NULL;
    return __real_calloc(count,size);
}
static unsigned expected_weapon(const SsaWeaponLoadouts *loadouts,unsigned setting,unsigned god,
    const SsaInventoryRules *rules,uint16_t *owned_bits) {
    unsigned j,chosen=1;uint16_t bits=1;
    if(god) {*owned_bits=0x3ff;return 10;}
    if(setting) for(j=1;j<10;++j) if(loadouts->ammo[setting-1][j-1]) {
        bits|=(uint16_t)(1u<<j);
        if(rules->ammo_capacity[j]) chosen=j+1;
    }
    *owned_bits=bits;return chosen;
}
static void prepare_start(ProjectileCase *t,Fixture *f,const SsaInventoryRules *inv) {
    WeaponCase *w=&t->weapon;SsaEntity *reserved[]={&f->players[0].entity};SsaPlayerRuntime *players[]={&f->player};unsigned i;
    (void)inv;memset(t,0,sizeof(*t));w->f=f;w->damage.player_count=1;w->damage.difficulty=1;w->damage.god_mode=1;
    memset(&f->players[0].inventory,0,sizeof(f->players[0].inventory));
    f->players[0].inventory.health=100;f->players[0].entity_health=100;
    for(i=0;i<10;++i) {
        w->weapon.controller.cooldown[i]=(uint16_t)(10+i);
        f->players[0].inventory.ammo[i]=(int16_t)(71+i);
    }
    f->players[0].inventory.pending_count=3;
    f->players[0].motion.movement.flags=4;f->players[0].motion.movement.weapon_flags=0xdead;
    CHECK(ssa_player_gameplay_init(&w->gameplay,&f->player,&f->plan)==0);
    CHECK(!w->gameplay.weapon.current && !w->gameplay.weapon.requested && !f->players[0].inventory.weapons);
    CHECK(ssa_combat_runtime_init(&w->combat,&f->spawn,reserved,1,players,1,&w->damage,&w->score)==0);
    w->combat_ops=(SsaCombatRuntimeOps){weapon_combat_effect,w};
    w->ops=(SsaWeaponRuntimeOps){&w->combat_ops,weapon_effect,weapon_impact,w};
    w->gameplay_ops=(SsaPlayerGameplayOps){weapon_life_event,weapon_life_line,weapon_life_sector,
        weapon_life_animation,weapon_life_sound,w};
    w->game.players=1;w->game.ticks=1;
}
static int start_one(ProjectileCase *t,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,const SsaWeaponLoadouts *loadouts,unsigned setting,unsigned god) {
    SsaWeaponRuntime *r[]={&t->weapon.weapon};SsaPlayerGameplay *g[]={&t->weapon.gameplay};
    return ssa_weapon_runtimes_start(r,g,&t->weapon.combat,inv,rules,anim,loadouts,setting,(uint8_t)god);
}
static void rejected_starts(ProjectileCase *t,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,const SsaWeaponLoadouts *loadouts) {
    WeaponCase *w=&t->weapon;Fixture *f=w->f;
    SsaInventory before=f->players[0].inventory;SsaWeaponControl control=w->gameplay.weapon;
    SsaWeaponRuntime *r[]={&w->weapon};SsaPlayerGameplay *g[]={&w->gameplay};
    CHECK(ssa_weapon_runtimes_start(NULL,g,&w->combat,inv,rules,anim,loadouts,0,0)==-1);
    CHECK(ssa_weapon_runtimes_start(r,NULL,&w->combat,inv,rules,anim,loadouts,0,0)==-1);
    CHECK(ssa_weapon_runtimes_start(r,g,NULL,inv,rules,anim,loadouts,0,0)==-1);
    CHECK(start_one(t,inv,rules,anim,loadouts,37,0)==-1);start_guards+=4;
    w->combat.updating=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);w->combat.updating=0;++start_guards;
    f->spawn.updating=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);f->spawn.updating=0;++start_guards;
    f->world.query_active=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);f->world.query_active=0;++start_guards;
    f->player.updating=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);f->player.updating=0;++start_guards;
    w->gameplay.updating=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);w->gameplay.updating=0;++start_guards;
    w->weapon.controller.updating=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);w->weapon.controller.updating=0;++start_guards;
    f->players[0].entity.player_index=1;CHECK(start_one(t,inv,rules,anim,loadouts,0,0)==-1);f->players[0].entity.player_index=0;++start_guards;
    CHECK(!memcmp(&before,&f->players[0].inventory,sizeof(before)));
    CHECK(!memcmp(&control,&w->gameplay.weapon,sizeof(control)) && !w->weapon.player);
}
static void startup_fire(ProjectileCase *t,uint8_t *rom,unsigned thing,const SsaMathTables *math,int32_t speed) {
    WeaponCase *w=&t->weapon;unsigned weapon=w->gameplay.weapon.current,i;
    if(weapon<=5) {
        CdCase c;SsaScriptActor *actor;
        script(w->f,rom,thing,1,1);step(w->f,1);bind(&c,w->f,math,speed);activate(&c);
        actor=&w->f->spawn.actors[c.runtime.slot];position_target(w,&c);
        for(i=0;i<256 && actor->activation.health==30;++i) {
            CHECK(weapon_tick(w,1,i==0)==0);weapon_animate(w);
        }
        CHECK(w->game.shots[weapon-1] && actor->activation.health<30 && w->weapon.hits[weapon-1]);
        ++start_shots[weapon-1];CHECK(ssa_script_spawn_defeat(&w->f->spawn,c.runtime.slot)==0);
        CHECK(ssa_script_spawn_release(&w->f->spawn,c.runtime.slot)==0);
    } else if(weapon==6 || weapon==7 || weapon==10) {
        unsigned slot;
        for(i=0;i<256 && !projectile_count(t);++i) {
            CHECK(weapon_tick(w,1,i==0)==0);weapon_animate(w);
        }
        CHECK(projectile_count(t)==1 && w->game.shots[weapon-1]);slot=projectile_first(t);
        CHECK(t->runtime.projectiles[slot].source==&w->f->players[0].entity);
        CHECK(t->runtime.projectiles[slot].owner==1);++start_shots[weapon-1];
        for(i=0;i<32 && t->runtime.active[slot];++i) {
            CHECK(projectile_tick(t,slot,1)>=0);++start_flight_frames;
        }
    }
}
static void two_player_start(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,const SsaWeaponLoadouts *loadouts,unsigned mode) {
    Fixture f;Multiplayer m={0};SsaEntity *reserved[2];SsaPlayerRuntime *bodies[2];
    SsaWeaponRuntime *weapons[2]={&m.weapon[0],&m.weapon[1]};SsaPlayerGameplay *games[2]={&m.gp[0],&m.gp[1]};
    SsaPlayerGameplay inactive;SsaWeaponRuntime untouched;SsaInventory inventory;unsigned i,j;
    setup(&f,rom,size,0,math,defs,100);m.f=&f;CHECK(ssa_script_spawn_free(&f.spawn)==0);
    f.players[1]=f.players[0];memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=0xca;f.players[1].entity.player_index=1;
    CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);CHECK(ssa_slide_init(&m.slide,&f.map));
    CHECK(ssa_player_runtime_init(&m.second,&f.players[1],&f.world,&m.slide,math)==0);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;bodies[0]=&f.player;bodies[1]=&m.second;
    m.damage.player_count=2;m.damage.difficulty=1;
    CHECK(ssa_combat_runtime_init(&m.combat,&f.spawn,reserved,2,bodies,2,&m.damage,&m.score)==0);
    for(i=0;i<2;++i) {
        CHECK(ssa_player_gameplay_init(&m.gp[i],bodies[i],&f.plan)==0);
        f.players[i].inventory.weapons=0;f.players[i].motion.movement.flags=(uint16_t)(mode==1 && i==1?0:4);
        m.gp[i].weapon.keys=0xbeef;
        for(j=0;j<10;++j)m.weapon[i].controller.cooldown[j]=(uint16_t)(100+i*10+j);
    }
    inactive=m.gp[1];untouched=m.weapon[1];inventory=f.players[1].inventory;
    if(mode==2) {calloc_calls=0;calloc_fail_at=5;}
    CHECK(ssa_weapon_runtimes_start(weapons,games,&m.combat,inv,rules,anim,loadouts,2,0)==(mode==2?-1:0));
    calloc_fail_at=0;CHECK(m.weapon[0].player==&m.gp[0]);
    if(mode==1) {
        CHECK(!memcmp(&inactive,&m.gp[1],sizeof(inactive)));
        CHECK(!memcmp(&untouched,&m.weapon[1],sizeof(untouched)));
        CHECK(!memcmp(&inventory,&f.players[1].inventory,sizeof(inventory)));++start_inactive;
    } else for(i=0;i<2;++i) {
        CHECK(m.gp[i].weapon.current==3 && m.gp[i].weapon.requested==3);
        for(j=0;j<10;++j)CHECK(m.weapon[i].controller.cooldown[j]==(j==i?0:100+i*10+j));
        start_cooldowns+=10;
    }
    if(mode==2) {
        CHECK(!m.weapon[1].player && !m.weapon[1].trace.entities && !m.weapon[1].trace.walls.visited);
        CHECK(!m.weapon[1].trace.traversal.intercepts && !m.combat.updating && !f.spawn.updating);
        CHECK(m.gp[1].weapon_view.y==0xa000 && m.gp[0].weapon_view.y==0xa000);++start_failures;
        m.weapon[0].credited_kills=&m.kills[0];m.game.players=2;m.game.ticks=1;
        m.effects=(SsaCombatRuntimeOps){mp_combat,&m};m.ops=(SsaWeaponRuntimeOps){&m.effects,mp_event,mp_impact,&m};
        CHECK(ssa_weapon_runtime_step(&m.weapon[0],&m.game,0,0,&m.ops)==0);++weapon_frames;
        CHECK(ssa_weapon_runtime_animate(&m.weapon[0],0,1)==0);
    }
    CHECK(ssa_weapon_runtimes_start(weapons,games,&m.combat,inv,rules,anim,loadouts,2,0)==-1);++start_guards;
    for(i=0;i<2;++i) {
        if(m.weapon[i].player)CHECK(ssa_weapon_runtime_free(&m.weapon[i])==0);
        CHECK(ssa_player_gameplay_free(&m.gp[i])==0);
    }
    CHECK(ssa_combat_runtime_free(&m.combat)==0);CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&m.second)==0);ssa_slide_free(&m.slide);
    CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    cleanup(&f);++start_multiplayer;
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inv;SsaWeaponRules rules;SsaWeaponAnimations anim;SsaWeaponLoadouts loadouts;
    int32_t speeds[12];unsigned level,setting,i,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_inventory_rules_load(&inv,rom,(size_t)size)==0);CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);
    CHECK(ssa_weapon_animations_load(&anim,rom,(size_t)size)==1);CHECK(ssa_weapon_loadouts_load(&loadouts,rom,(size_t)size)==1);
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);
    for(level=0;level<16;++level) {
        Fixture f;setup(&f,rom,(size_t)size,level,&math,&defs,100);
        for(setting=0;setting<=37;++setting) {
            ProjectileCase t;WeaponCase *w=&t.weapon;unsigned selected,god=setting==37,actual_setting=god?0:setting;
            uint16_t owned_bits;unsigned view;int32_t jump;
            prepare_start(&t,&f,&inv);
            if(!setting) {
                CHECK(ssa_player_runtime_key(&f.player,'H',1,0,1));
                CHECK(!ssa_player_runtime_key(&f.player,'H',0,0,1));
                f.player.jump.launch_velocity=7*65536;f.player.jump.was_down=1;
            }
            view=f.player.view.third_person;jump=f.player.jump.launch_velocity;
            if(!level && !setting)rejected_starts(&t,&inv,&rules,&anim,&loadouts);
            CHECK(start_one(&t,&inv,&rules,&anim,&loadouts,actual_setting,god)==0);++start_cases;
            selected=expected_weapon(&loadouts,actual_setting,god,&inv,&owned_bits);
            CHECK(w->gameplay.weapon.current==selected && w->gameplay.weapon.requested==selected);
            CHECK(f.players[0].inventory.weapons==owned_bits && f.players[0].inventory.ammo[0]==999);
            CHECK(!f.players[0].inventory.pending_count && !w->gameplay.weapon.flags && !f.players[0].motion.movement.weapon_flags);
            CHECK(w->weapon.object.animation==selected-1 && w->weapon.object.period==3 && !w->weapon.object.done);
            CHECK(w->weapon.object.x==0x7800 && w->weapon.object.y==0xa000 && w->weapon.object.type==2);
            CHECK(w->weapon.object.layer==14 && w->weapon.object.owner==1 && w->weapon.object.flags==1);
            CHECK(w->gameplay.weapon_action==0xa000 && w->gameplay.weapon_view.y==0xa000);
            CHECK(f.player.view.third_person==view && f.player.jump.launch_velocity==jump && f.player.jump.was_down);
            for(i=0;i<10;++i)CHECK(w->weapon.controller.cooldown[i]==(i?10+i:0));
            start_cooldowns+=10;
            if(!actual_setting && !god)for(i=1;i<10;++i)CHECK(f.players[0].inventory.ammo[i]==71+(int)i);
            CHECK(start_one(&t,&inv,&rules,&anim,&loadouts,actual_setting,god)==-1);++start_guards;
            CHECK(ssa_projectile_runtime_init(&t.runtime,&w->combat,&math)==0);
            CHECK(ssa_weapon_runtime_bind_projectiles(&w->weapon,&t.runtime)==0);
            t.ops=(SsaProjectileRuntimeOps){&w->combat_ops,projectile_particle,&t};
            if(setting<=12)startup_fire(&t,rom,thing,&math,speeds[0]);
            if(setting==37) for(i=0;i<128;++i) {player_step(&f,i);CHECK(weapon_tick(w,0,0)==0);weapon_animate(w);}
            projectile_cleanup(&t);
        }
        cleanup(&f);
    }
    for(i=0;i<3;++i)two_player_start(rom,(size_t)size,&math,&defs,&inv,&rules,&anim,&loadouts,i);
    CHECK(start_shots[0] && start_shots[1] && start_shots[2] && start_shots[5] && start_shots[6] && start_shots[9]);
    CHECK(airborne && third_frames && start_failures==1 && start_inactive==1);free(rom);
    printf("{\"assertions\":%u,\"maps\":16,\"loadout_starts\":%u,\"guards\":%u,\"allocation_failure_paths\":%u,\"two_player_cases\":%u,\"inactive_players_preserved\":%u,\"cooldown_field_checks\":%u,\"weapon_frames\":%u,\"projectile_flight_frames\":%u,\"player_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"shots_by_initial_weapon\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]}\n",
        checks,start_cases,start_guards,start_failures,start_multiplayer,start_inactive,start_cooldowns,weapon_frames,start_flight_frames,player_frames,airborne,third_frames,
        start_shots[0],start_shots[1],start_shots[2],start_shots[3],start_shots[4],start_shots[5],start_shots[6],start_shots[7],start_shots[8],start_shots[9]);
    return 0;
}
