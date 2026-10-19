#define SSA_GAME_TEST_MAIN game_fixture_main
#include "test_game_runtime.c"
#include <limits.h>
#include "ssa/pc_input.h"
#include "ssa/pc_input_runtime.h"

static unsigned pc_samples,pc_frames,pc_moves,pc_airborne,pc_third,pc_shots,pc_guards;
static unsigned pc_multiplayer_frames,pc_network_errors,pc_gate_checks,pc_direction_checks;

/* The fixture keeps the same real map, player, world, combat, script, actor,
 * weapon and projectile components used by the combined-frame integration. */
typedef struct PcAssets {
    uint8_t *rom;size_t size;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations weapons;
    SsaWeaponLoadouts loadouts;SsaActorAnimations actors;SsaPickupRespawnData pickups;
    int32_t speeds[12];
} PcAssets;
static void pc_assets(PcAssets *a,const char *path) {
    FILE *file=fopen(path,"rb");long size;CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);a->size=(size_t)size;a->rom=malloc(a->size);CHECK(a->rom);
    CHECK(fread(a->rom,1,a->size,file)==a->size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&a->math,a->rom,a->size));CHECK(ssa_spawn_definitions_load(&a->defs,a->rom,a->size));
    CHECK(ssa_enemy_navigation_speeds(a->speeds,a->rom,a->size)==1);
    CHECK(ssa_inventory_rules_load(&a->inventory,a->rom,a->size)==0);
    CHECK(ssa_weapon_rules_load(&a->rules,a->rom,a->size)==1);
    CHECK(ssa_weapon_animations_load(&a->weapons,a->rom,a->size)==1);
    CHECK(ssa_weapon_loadouts_load(&a->loadouts,a->rom,a->size)==1);
    CHECK(ssa_actor_animations_load(&a->actors,a->rom,a->size)==1);
    CHECK(ssa_pickup_respawn_data_load(&a->pickups,a->rom,a->size)==1);
}

static SsaPcInputSample sample(SsaPcInput *p) {
    SsaPcInputSample s;memset(&s,0xa5,sizeof(s));CHECK(ssa_pc_input_sample(p,&s)==1);++pc_samples;return s;
}
static void key(SsaPcInput *p,int k,int down,int repeat) {CHECK(ssa_pc_input_key(p,k,down,repeat)==1);}
static void button(SsaPcInput *p,int down) {CHECK(ssa_pc_input_button(p,SSA_PC_MOUSE_LEFT,down)==1);}
static void pc_core(void) {
    SsaPcInput p,before;SsaPcInputSample s,saved;unsigned mask,i,j;int32_t sensitivity[]={1,64,65536,INT32_MAX};
    int32_t counts[]={0,1,-1,INT32_MAX,INT32_MIN,65535,-65536};const int keys[]={'w','S','a','D'};
    for(mask=0;mask<16;++mask) {
        uint32_t expected=0;CHECK(ssa_pc_input_init(&p,64)==1);
        for(i=0;i<4;++i)if(mask&(1u<<i))key(&p,keys[i],1,0);
        if((mask&3)==1)expected|=0x40;
        if((mask&3)==2)expected|=0x80;
        if((mask&12)==4)expected|=0x200;
        if((mask&12)==8)expected|=0x100;
        s=sample(&p);CHECK(s.keys==expected && !s.held && !s.pressed && !s.jump_down && !s.view_toggle && s.enabled);
        s=sample(&p);CHECK(s.keys==expected && !s.yaw_delta);
    }
    for(i=0;i<sizeof(sensitivity)/sizeof(*sensitivity);++i)
    for(j=0;j<sizeof(counts)/sizeof(*counts);++j) {
        uint32_t expected=(uint32_t)counts[j]*(uint32_t)sensitivity[i];
        CHECK(ssa_pc_input_init(&p,sensitivity[i])==1);
        CHECK(ssa_pc_input_relative(&p,counts[j],INT32_MIN)==1);s=sample(&p);CHECK((uint32_t)s.yaw_delta==expected);
        CHECK(!sample(&p).yaw_delta);CHECK(ssa_pc_input_relative(&p,counts[j],INT32_MAX)==1);
        CHECK(ssa_pc_input_relative(&p,counts[j],0)==1);CHECK((uint32_t)sample(&p).yaw_delta==expected+expected);
    }
    CHECK(ssa_pc_input_init(&p,64)==1);button(&p,1);button(&p,0);s=sample(&p);
    CHECK(s.held==1 && s.pressed==1);s=sample(&p);CHECK(!s.held && !s.pressed);
    button(&p,1);s=sample(&p);CHECK(s.held==1 && s.pressed==1);
    button(&p,1);s=sample(&p);CHECK(s.held==1 && !s.pressed);button(&p,0);
    key(&p,' ',1,0);key(&p,' ',0,0);CHECK(sample(&p).jump_down);CHECK(!sample(&p).jump_down);
    key(&p,' ',1,0);CHECK(sample(&p).jump_down);key(&p,' ',1,1);CHECK(sample(&p).jump_down);key(&p,' ',0,0);
    key(&p,'H',1,0);CHECK(sample(&p).view_toggle);key(&p,'h',1,1);CHECK(!sample(&p).view_toggle);
    key(&p,'H',0,0);key(&p,'H',1,0);key(&p,'H',0,0);key(&p,'H',1,0);key(&p,'H',0,0);
    CHECK(!sample(&p).view_toggle);key(&p,'E',1,0);key(&p,'E',0,0);CHECK(sample(&p).keys&0x10000);CHECK(!(sample(&p).keys&0x10000));
    key(&p,'r',1,0);key(&p,'R',0,0);s=sample(&p);CHECK(s.reload_pressed && !s.packed);
    CHECK(!sample(&p).reload_pressed);
    key(&p,'R',1,1);CHECK(!sample(&p).reload_pressed);key(&p,'R',0,0);
    /* Disable consumes pending actions, and held actions remain blocked across
     * resume. Movement may resume, because it is a continuous control. */
    key(&p,'W',1,0);key(&p,' ',1,0);key(&p,'H',1,0);key(&p,'E',1,0);key(&p,'R',1,0);button(&p,1);
    CHECK(ssa_pc_input_relative(&p,12,0)==1);ssa_pc_input_enable(&p,0);s=sample(&p);
    CHECK(!s.enabled && !s.keys && !s.held && !s.pressed && !s.jump_down && !s.view_toggle && !s.yaw_delta);
    ssa_pc_input_enable(&p,1);s=sample(&p);CHECK(s.enabled && s.keys==0x40 && !s.held && !s.pressed && !s.jump_down && !s.view_toggle && !s.reload_pressed);
    key(&p,' ',0,0);key(&p,' ',1,0);button(&p,0);button(&p,1);s=sample(&p);CHECK(s.jump_down && s.held==1 && s.pressed==1);
    ssa_pc_input_focus(&p,0);key(&p,'H',1,0);button(&p,1);CHECK(ssa_pc_input_relative(&p,INT32_MAX,0)==1);
    s=sample(&p);CHECK(!s.enabled && !s.keys && !s.held && !s.pressed && !s.jump_down && !s.view_toggle && !s.yaw_delta);
    ssa_pc_input_focus(&p,1);s=sample(&p);CHECK(s.enabled && !s.keys && !s.held && !s.jump_down && !s.view_toggle && !s.yaw_delta);
    before=p;memset(&s,0xa5,sizeof(s));saved=s;CHECK(ssa_pc_input_sample(NULL,&s)==-1);CHECK(!memcmp(&s,&saved,sizeof(s)));
    CHECK(ssa_pc_input_sample(&p,NULL)==-1);CHECK(!memcmp(&p,&before,sizeof(p)));pc_guards+=2;
    CHECK(ssa_pc_input_sensitivity(&p,0)==-1);CHECK(!memcmp(&p,&before,sizeof(p)));++pc_guards;
    CHECK(ssa_pc_input_key(&p,'Z',1,0)==0);CHECK(ssa_pc_input_button(&p,2,1)==0);CHECK(!memcmp(&p,&before,sizeof(p)));
    CHECK(ssa_pc_input_init(NULL,64)==-1);CHECK(ssa_pc_input_init(&p,-1)==-1);pc_guards+=2;
}

typedef struct PcCase {
    GameCase game;SsaPcInput input;SsaPcInputRuntime runtime;int key_before,key_after;
    unsigned after_sample,skip_poll,expect_distribute_error,bad_weapon_game;
} PcCase;
static int pc_before(void *ctx,const SsaGameRuntime *r,SsaFrameStage stage) {
    PcCase *p=ctx;return game_before(&p->game,r,stage);
}
static int pc_external(void *ctx,SsaGameRuntime *r,SsaFrameStage stage) {
    PcCase *p=ctx;
    if(stage==SSA_FRAME_POLL) {
        int result;
        if(p->skip_poll)return 0;
        if(p->key_before) {
            CHECK(ssa_pc_input_runtime_key(&p->runtime,p->key_before,1,0)==1);
            CHECK(ssa_pc_input_runtime_key(&p->runtime,p->key_before,0,0)==1);p->key_before=0;
        }
        result=ssa_pc_input_runtime_poll(&p->runtime);CHECK(result==0);
        if(p->after_sample) {
            CHECK(p->runtime.sample.view_toggle && p->runtime.sample.held && p->runtime.sample.yaw_delta);
            if(p->after_sample<=2) {
                ssa_pc_input_runtime_focus(&p->runtime,0);
                if(p->after_sample==2)ssa_pc_input_runtime_focus(&p->runtime,1);
            } else if(p->after_sample<=4) {
                ssa_pc_input_runtime_enable(&p->runtime,0);
                if(p->after_sample==4)ssa_pc_input_runtime_enable(&p->runtime,1);
            } else p->game.f->players[0].motion.movement.flags&=(uint16_t)~4u;
            if(p->after_sample<=4)CHECK(!r->frame->processed_input && !p->runtime.sample.enabled);
        }
        if(p->key_after) {
            CHECK(ssa_pc_input_runtime_key(&p->runtime,p->key_after,1,0)==1);
            CHECK(ssa_pc_input_runtime_key(&p->runtime,p->key_after,0,0)==1);p->key_after=0;
        }
        CHECK(ssa_pc_input_runtime_poll(&p->runtime)==-1);++pc_guards;
        return p->game.fail_external==stage?-1:0;
    }
    if(stage==SSA_FRAME_DISTRIBUTE) {
        int result;
        if(p->bad_weapon_game) {
            SsaWeaponGame *saved=r->weapon_game;r->weapon_game=NULL;
            CHECK(ssa_pc_input_runtime_distribute(&p->runtime)==-1);r->weapon_game=saved;++pc_guards;
        }
        result=ssa_pc_input_runtime_distribute(&p->runtime);
        if(p->expect_distribute_error) {CHECK(result==-1);++pc_guards;return -1;}
        CHECK(result==0);
        if(p->after_sample==5) {
            CHECK(!memcmp(&r->input.entities.players[0],&p->game.input.entities.players[0],sizeof(SsaPlayerRuntimeInput)));
            CHECK(r->input.held[0]==p->game.input.held[0] && r->input.pressed[0]==p->game.input.pressed[0]);
        }
        CHECK(ssa_pc_input_runtime_distribute(&p->runtime)==-1);++pc_guards;return 0;
    }
    CHECK(ssa_pc_input_runtime_key(&p->runtime,'W',1,0)==-1);
    CHECK(ssa_pc_input_runtime_button(&p->runtime,SSA_PC_MOUSE_LEFT,1)==-1);
    CHECK(ssa_pc_input_runtime_relative(&p->runtime,1,0)==-1);
    CHECK(ssa_pc_input_runtime_free(&p->runtime)==-1);pc_guards+=4;
    return game_external(&p->game,r,stage);
}
static void pc_tick(PcCase *p) {CHECK(game_tick(&p->game)==0);++pc_frames;}
static void pc_key(PcCase *p,int k,int down,int repeat) {CHECK(ssa_pc_input_runtime_key(&p->runtime,k,down,repeat)==1);}
static void pc_button(PcCase *p,int down) {CHECK(ssa_pc_input_runtime_button(&p->runtime,SSA_PC_MOUSE_LEFT,down)==1);}
static void pc_start(PcCase *p,Fixture *f,ProjectileCase *projectile,PcAssets *a,unsigned map) {
    memset(p,0,sizeof(*p));setup(f,a->rom,a->size,map,&a->math,&a->defs,100);
    game_start(&p->game,f,projectile,&a->math,&a->inventory,&a->rules,&a->weapons,&a->loadouts,&a->actors,&a->pickups,0,1,a->speeds[0]);
    CHECK(ssa_pc_input_init(&p->input,64)==1);CHECK(ssa_pc_input_runtime_init(&p->runtime,&p->input,&p->game.runtime,0)==0);
    p->game.ops.external=pc_external;p->game.ops.before=pc_before;p->game.ops.context=p;
}
static void pc_finish(PcCase *p) {CHECK(ssa_pc_input_runtime_free(&p->runtime)==0);game_finish(&p->game);}
static void pc_map(PcAssets *a,unsigned map) {
    Fixture f;ProjectileCase projectile;PcCase p;unsigned frame,shot=0;const int keys[]={'W','A','S','D'};
    pc_start(&p,&f,&projectile,a,map);CHECK(!f.player.view.third_person);
    pc_button(&p,1);
    for(frame=0;frame<128;++frame) {
        int32_t x=f.players[0].motion.x,y=f.players[0].motion.y;uint16_t heading=f.players[0].motion.movement.heading;
        if(!(frame%32)) {if(frame)pc_key(&p,keys[frame/32-1],0,0);pc_key(&p,keys[frame/32],1,0);}
        if(frame%48==0)pc_key(&p,' ',1,0);
        if(frame%48==3)pc_key(&p,' ',0,0);
        if(frame==16 || frame==112) {pc_key(&p,'H',1,0);pc_key(&p,'H',0,0);}
        CHECK(ssa_pc_input_runtime_relative(&p.runtime,(frame&1)?1:-1,frame&1?INT32_MAX:INT32_MIN)==1);
        pc_tick(&p);CHECK(f.players[0].motion.movement.heading==(uint16_t)(heading+((frame&1)?64:-64)));
        CHECK(f.player.view.third_person==(frame>=16 && frame<112));
        if(x!=f.players[0].motion.x || y!=f.players[0].motion.y)++pc_moves;
        if(f.players[0].motion.z>(int32_t)f.players[0].entity.body.floor*65536)++pc_airborne;
        if(f.player.view.third_person)++pc_third;
        if(!shot && projectile.weapon.game.shots[9]) {shot=1;pc_button(&p,0);++pc_shots;}
    }
    CHECK(shot);pc_finish(&p);
}
static void pc_direction(PcAssets *a) {
    const int keys[]={'W','S','A','D'};unsigned i;
    for(i=0;i<4;++i) {
        Fixture f;ProjectileCase projectile;PcCase p;SsaMovement *m;
        pc_start(&p,&f,&projectile,a,0);m=&f.players[0].motion.movement;m->heading=0;
        pc_key(&p,keys[i],1,0);pc_tick(&p);CHECK(!m->heading && !m->turn_velocity);
        if(i==0)CHECK(m->forward_velocity>0 && m->velocity_y>0 && !m->strafe_velocity && !m->velocity_x);
        if(i==1)CHECK(m->forward_velocity<0 && m->velocity_y<0 && !m->strafe_velocity && !m->velocity_x);
        if(i==2)CHECK(m->strafe_velocity>0 && m->velocity_x<0 && !m->forward_velocity && !m->velocity_y);
        if(i==3)CHECK(m->strafe_velocity<0 && m->velocity_x>0 && !m->forward_velocity && !m->velocity_y);
        ++pc_direction_checks;pc_key(&p,keys[i^1],1,0);pc_tick(&p);
        CHECK(!m->forward_velocity && !m->strafe_velocity && !m->turn_velocity);++pc_direction_checks;
        pc_finish(&p);
    }
}
static void pc_edges(PcAssets *a) {
    Fixture f;ProjectileCase projectile;PcCase p;unsigned before,frame,mode;uint16_t heading;
    SsaPcInputRuntime copy;
    pc_start(&p,&f,&projectile,a,0);copy=p.runtime;
    CHECK(ssa_pc_input_runtime_poll(&p.runtime)==-1);CHECK(ssa_pc_input_runtime_distribute(&p.runtime)==-1);pc_guards+=2;
    CHECK(p.runtime.input==copy.input && p.runtime.game==copy.game);
    CHECK(ssa_pc_input_runtime_init(NULL,&p.input,&p.game.runtime,0)==-1);
    CHECK(ssa_pc_input_runtime_init(&copy,&p.input,&p.game.runtime,4)==-1);pc_guards+=2;
    p.game.fail_external=SSA_FRAME_POLL;
    CHECK(game_tick(&p.game)==-1);p.game.fail_external=0;
    CHECK(ssa_pc_input_runtime_relative(&p.runtime,3,0)==1);heading=f.players[0].motion.movement.heading;
    pc_tick(&p);CHECK(f.players[0].motion.movement.heading==(uint16_t)(heading+192));++pc_gate_checks;
    p.key_before='H';pc_tick(&p);CHECK(f.player.view.third_person);
    p.key_after='H';pc_tick(&p);CHECK(f.player.view.third_person);
    pc_tick(&p);CHECK(!f.player.view.third_person);pc_gate_checks+=3;
    /* Setup selects a ready grenade, then only actual mouse events initiate
     * its native weapon controller and allocation; the short click is retained. */
    select_for_test(&projectile.weapon,10);before=projectile.weapon.game.shots[9];pc_button(&p,1);pc_button(&p,0);pc_tick(&p);
    CHECK(projectile.weapon.game.shots[9]==before && (projectile.weapon.gameplay.weapon.flags&0x400));
    pc_tick(&p);CHECK(!p.game.runtime.input.held[0] && !p.game.runtime.input.pressed[0]);
    CHECK(projectile.weapon.game.shots[9]==before+1);CHECK(projectile_count(&projectile)==1);++pc_shots;
    for(frame=0;frame<80;++frame)pc_tick(&p);
    CHECK(projectile.weapon.game.shots[9]==before+1);
    pc_key(&p,' ',1,0);pc_key(&p,' ',0,0);pc_tick(&p);CHECK(f.player.jump.was_down);
    CHECK(f.players[0].motion.z>(int32_t)f.players[0].entity.body.floor*65536);++pc_gate_checks;
    for(frame=0;frame<80;++frame)pc_tick(&p);
    pc_key(&p,'H',1,0);pc_tick(&p);CHECK(f.player.view.third_person);
    for(frame=0;frame<8;++frame) {pc_key(&p,'H',1,1);pc_tick(&p);CHECK(f.player.view.third_person);}
    pc_key(&p,'H',0,0);pc_key(&p,'H',1,0);pc_key(&p,'H',0,0);
    pc_key(&p,'H',1,0);pc_key(&p,'H',0,0);pc_tick(&p);CHECK(f.player.view.third_person);++pc_gate_checks;
    /* Pausing, global input mode3, and an explicit host disable each consume
     * held jump/fire/H/use and mouse motion. Release is required to reactivate. */
    for(mode=0;mode<3;++mode) {
        unsigned view=f.player.view.third_person;int32_t z=f.players[0].motion.z;
        select_for_test(&projectile.weapon,10);before=projectile.weapon.game.shots[9];heading=f.players[0].motion.movement.heading;
        if(mode==0) {p.game.game.flags|=1;p.game.control_pause=1;}
        if(mode==1)p.game.runtime.life_game.input_state=3;
        if(mode==2)ssa_pc_input_runtime_enable(&p.runtime,0);
        pc_key(&p,' ',1,0);pc_key(&p,'H',1,0);pc_key(&p,'E',1,0);pc_button(&p,1);
        CHECK(ssa_pc_input_runtime_relative(&p.runtime,300,INT32_MIN)==1);pc_tick(&p);
        CHECK(f.player.view.third_person==view && f.players[0].motion.movement.heading==heading);
        CHECK(projectile.weapon.game.shots[9]==before && f.players[0].motion.z==z);
        if(mode==0) {p.game.game.flags&=(uint16_t)~1;p.game.control_pause=0;}
        if(mode==1)p.game.runtime.life_game.input_state=0;
        if(mode==2)ssa_pc_input_runtime_enable(&p.runtime,1);
        for(frame=0;frame<3;++frame) {
            pc_key(&p,'H',1,1);pc_key(&p,' ',1,1);pc_tick(&p);
            CHECK(f.player.view.third_person==view && projectile.weapon.game.shots[9]==before);
            CHECK(f.players[0].motion.z==z && f.players[0].motion.movement.heading==heading);
        }
        pc_key(&p,' ',0,0);pc_key(&p,'H',0,0);pc_key(&p,'E',0,0);pc_button(&p,0);pc_tick(&p);
        pc_key(&p,' ',1,0);pc_key(&p,'H',1,0);pc_key(&p,'H',0,0);pc_tick(&p);
        CHECK(f.player.view.third_person!=view && f.players[0].motion.z>z);pc_key(&p,' ',0,0);pc_gate_checks+=5;
        for(frame=0;frame<80;++frame)pc_tick(&p);
    }
    before=f.player.view.third_person;heading=f.players[0].motion.movement.heading;
    pc_key(&p,'W',1,0);pc_key(&p,'H',1,0);pc_key(&p,' ',1,0);pc_button(&p,1);
    CHECK(ssa_pc_input_runtime_relative(&p.runtime,200,0)==1);ssa_pc_input_runtime_focus(&p.runtime,0);pc_tick(&p);
    CHECK(f.player.view.third_person==before && f.players[0].motion.movement.heading==heading);
    CHECK(!p.game.runtime.input.entities.players[0].keys && !p.game.runtime.input.held[0]);
    ssa_pc_input_runtime_focus(&p.runtime,1);pc_tick(&p);
    CHECK(!p.game.runtime.input.entities.players[0].keys && !p.game.runtime.input.held[0]);CHECK(f.player.view.third_person==before);
    pc_key(&p,'H',1,0);pc_key(&p,'H',0,0);pc_tick(&p);CHECK(f.player.view.third_person!=before);pc_gate_checks+=3;
    CHECK(ssa_pc_input_sensitivity(&p.input,INT32_MAX)==1);heading=f.players[0].motion.movement.heading;
    CHECK(ssa_pc_input_runtime_relative(&p.runtime,INT32_MIN,INT32_MAX)==1);pc_tick(&p);
    CHECK(f.players[0].motion.movement.heading==heading);
    CHECK(ssa_pc_input_runtime_relative(&p.runtime,INT32_MAX,INT32_MIN)==1);pc_tick(&p);
    CHECK(f.players[0].motion.movement.heading==(uint16_t)(heading+1));++pc_gate_checks;
    pc_finish(&p);
}
static void pc_sample_gates(PcAssets *a) {
    unsigned mode;
    for(mode=1;mode<=5;++mode) {
        Fixture f;ProjectileCase projectile;PcCase p;uint16_t heading;unsigned view;
        pc_start(&p,&f,&projectile,a,0);p.after_sample=mode;
        heading=f.players[0].motion.movement.heading;view=f.player.view.third_person;
        if(mode==5) {
            p.game.input.entities.players[0].keys=0x40;p.game.input.entities.players[0].yaw_delta=113;
            p.game.input.entities.players[0].jump_down=1;p.game.input.held[0]=2;p.game.input.pressed[0]=2;
            p.game.control_pause=1;
        }
        pc_key(&p,'H',1,0);pc_key(&p,' ',1,0);pc_button(&p,1);
        CHECK(ssa_pc_input_runtime_relative(&p.runtime,400,0)==1);pc_tick(&p);
        CHECK(f.player.view.third_person==view && f.players[0].motion.movement.heading==heading);
        CHECK(!projectile.weapon.game.shots[9] && !(projectile.weapon.gameplay.weapon.flags&0x400));
        CHECK(f.players[0].motion.z==(int32_t)f.players[0].entity.body.floor*65536);
        if(mode<5) {
            CHECK(!p.game.runtime.input.entities.players[0].keys && !p.game.runtime.input.entities.players[0].yaw_delta);
            CHECK(!p.game.runtime.input.entities.players[0].jump_down && !p.game.runtime.input.held[0] && !p.game.runtime.input.pressed[0]);
        }
        ++pc_gate_checks;p.after_sample=0;
        if(mode==5) {
            /* Starting the next poll inactive also preserves every caller
             * input rather than replacing it with a zero PC sample. */
            pc_tick(&p);
            CHECK(!memcmp(&p.game.runtime.input.entities.players[0],&p.game.input.entities.players[0],sizeof(SsaPlayerRuntimeInput)));
            CHECK(p.game.runtime.input.held[0]==2 && p.game.runtime.input.pressed[0]==2);++pc_gate_checks;
        }
        pc_finish(&p);
    }
    {
        Fixture f;ProjectileCase projectile;PcCase p;SsaPlayerGameplay *saved;unsigned count;
        pc_start(&p,&f,&projectile,a,0);
        count=p.game.entity.runtime.combat->player_count;p.game.entity.runtime.combat->player_count=5;
        CHECK(ssa_pc_input_runtime_key(&p.runtime,'W',1,0)==-1);p.game.entity.runtime.combat->player_count=count;++pc_guards;
        saved=p.game.entity.runtime.players[0];p.game.entity.runtime.players[0]=NULL;
        CHECK(ssa_pc_input_runtime_relative(&p.runtime,1,0)==-1);p.game.entity.runtime.players[0]=saved;++pc_guards;
        p.bad_weapon_game=1;pc_tick(&p);p.bad_weapon_game=0;
        p.game.fail_external=SSA_FRAME_POLL;CHECK(game_tick(&p.game)==-1);
        p.skip_poll=p.expect_distribute_error=1;p.game.fail_external=SSA_FRAME_DISTRIBUTE;
        CHECK(game_tick(&p.game)==-1);CHECK(p.game.order_count==2);++pc_gate_checks;
        p.skip_poll=p.expect_distribute_error=0;p.game.fail_external=0;pc_tick(&p);++pc_gate_checks;
        pc_finish(&p);
    }
}

typedef struct PcMulti {GameMulti *game;SsaPcInput input;SsaPcInputRuntime runtime;} PcMulti;
static int pc_multi_before(void *ctx,const SsaGameRuntime *r,SsaFrameStage stage) {
    PcMulti *p=ctx;return multi_game_before(p->game,r,stage);
}
static int pc_multi_external(void *ctx,SsaGameRuntime *r,SsaFrameStage stage) {
    PcMulti *p=ctx;
    if(stage==SSA_FRAME_POLL)return ssa_pc_input_runtime_poll(&p->runtime);
    if(stage==SSA_FRAME_DISTRIBUTE)return ssa_pc_input_runtime_distribute(&p->runtime);
    return multi_game_external(p->game,r,stage);
}
static void pc_multi(PcAssets *a) {
    Fixture f;GameMulti t;PcMulti p;SsaEntity *reserved[2];SsaPlayerRuntime *players[2];SsaPlayerGameplay *games[2];
    SsaWeaponRuntime *runtimes[2];SsaActorAnimation *states[2];const SsaEntity *peers[2];unsigned i,frame;
    memset(&t,0,sizeof(t));memset(&p,0,sizeof(p));setup(&f,a->rom,a->size,0,&a->math,&a->defs,100);t.m.f=&f;p.game=&t;
    CHECK(ssa_script_spawn_free(&f.spawn)==0);f.players[1]=f.players[0];memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=0xca;f.players[1].entity.player_index=1;CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_slide_init(&t.m.slide,&f.map));CHECK(ssa_player_runtime_init(&t.m.second,&f.players[1],&f.world,&t.m.slide,&a->math)==0);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;players[0]=&f.player;players[1]=&t.m.second;
    t.m.damage.player_count=2;t.m.damage.difficulty=1;t.m.damage.god_mode=1;t.m.damage.kill_limit=99;
    CHECK(ssa_combat_runtime_init(&t.m.combat,&f.spawn,reserved,2,players,2,&t.m.damage,&t.m.score)==0);
    for(i=0;i<2;++i) {
        games[i]=&t.m.gp[i];runtimes[i]=&t.m.weapon[i];states[i]=&t.states[i];peers[i]=reserved[i];
        CHECK(ssa_player_gameplay_init(games[i],players[i],&f.plan)==0);f.players[i].motion.movement.flags=4;
        CHECK(ssa_actor_animation_set(states[i],f.players[i].animation,f.players[i].animation_ticks)==1);
    }
    CHECK(ssa_weapon_runtimes_start(runtimes,games,&t.m.combat,&a->inventory,&a->rules,&a->weapons,&a->loadouts,0,1)==0);
    CHECK(ssa_projectile_runtime_init(&t.projectiles,&t.m.combat,&a->math)==0);
    for(i=0;i<2;++i) {t.m.weapon[i].credited_kills=&t.m.kills[i];CHECK(ssa_weapon_runtime_bind_projectiles(&t.m.weapon[i],&t.projectiles)==0);}
    CHECK(ssa_enemy_queries_init(&t.queries,&f.spawn,&a->math,reserved,2)==0);
    CHECK(ssa_entity_runtime_init(&t.entities,&t.m.combat,&t.projectiles,&t.queries,games,0)==0);
    CHECK(ssa_actor_animation_runtime_init(&t.actors,&f.spawn,&a->actors,reserved,states,2)==0);
    CHECK(ssa_pickup_respawn_runtime_init(&t.pickups,&t.m.combat,&f.plan,&a->pickups)==0);
    for(i=0;i<84;++i) {CHECK(ssa_weapon_object_init(&t.objects[i])==1);t.sprites.objects[i]=&t.objects[i];}
    for(i=0;i<2;++i) {t.sprites.objects[i]=&t.m.weapon[i].object;t.sprites.active[i]=i;t.m.weapon[i].object.active_index=(uint16_t)i;}
    t.sprites.active_count=2;t.game.ticks=1;t.game.flags=0x100;t.frame.game=&t.game;
    CHECK(ssa_game_runtime_init(&t.runtime,&t.frame,&t.entities,&t.actors,&t.pickups,&t.sprites,&a->weapons,runtimes,&t.m.game)==0);
    t.m.effects=(SsaCombatRuntimeOps){mp_combat,&t.m};t.m.ops=(SsaWeaponRuntimeOps){&t.m.effects,multi_game_weapon,multi_game_impact,&t};
    t.player_ops=(SsaLevelPlayerOps){multi_game_life,multi_game_animation,multi_game_sound,&t};
    t.projectile_ops=(SsaProjectileRuntimeOps){&t.m.effects,multi_game_particle,&t};t.entity_ops.projectiles=&t.projectile_ops;
    t.entity_ops.before=multi_game_entity;t.entity_ops.context=&t;
    t.cd_ops=(SsaCombatCdOps){&t.m.effects,multi_game_cd,&t};t.entity_ops.enemies=&t.cd_ops;
    t.pickup_ops=(SsaPickupRespawnRuntimeOps){multi_game_pickup,&t};
    for(i=0;i<2;++i) {
        t.entity_ops.players[i]=&t.player_ops;t.ops.weapons[i]=&t.m.ops;t.input.entities.players[i]=f.input;
        t.input.entities.players[i].motion.players=peers;t.input.entities.players[i].motion.slot=(uint8_t)i;
    }
    t.input.entities.enemies=(SsaEnemyCdFrame){1,a->speeds[0],0,0};
    t.ops.entities=&t.entity_ops;t.ops.scripts=&f.ops;t.ops.movers=&f.movers;t.ops.pickups=&t.pickup_ops;
    t.ops.external=pc_multi_external;t.ops.before=pc_multi_before;t.ops.context=&p;
    CHECK(ssa_pc_input_init(&p.input,64)==1);CHECK(ssa_pc_input_runtime_init(&p.runtime,&p.input,&t.runtime,1)==0);
    CHECK(ssa_pc_input_runtime_key(&p.runtime,'W',1,0)==1);CHECK(ssa_pc_input_runtime_key(&p.runtime,'H',1,0)==1);
    CHECK(ssa_pc_input_runtime_key(&p.runtime,'H',0,0)==1);CHECK(ssa_pc_input_runtime_button(&p.runtime,SSA_PC_MOUSE_LEFT,1)==1);
    /* Player0 is display-local, player1 is the explicit PC recipient. Existing
     * remote input remains a caller-owned fixture, never a network substitute. */
    for(frame=0;frame<64;++frame) {
        uint16_t h0=f.players[0].motion.movement.heading,h1=f.players[1].motion.movement.heading;
        t.input.entities.players[0].keys=0x100;t.input.entities.players[0].yaw_delta=7;
        t.input.entities.players[0].jump_down=(uint8_t)(frame%48<3);t.input.held[0]=t.input.pressed[0]=0;
        CHECK(ssa_pc_input_runtime_relative(&p.runtime,1,0)==1);multi_game_tick(&t);++pc_multiplayer_frames;
        CHECK(t.runtime.input.entities.players[0].keys==0x100 && t.runtime.input.entities.players[0].yaw_delta==7);
        CHECK(t.runtime.input.entities.players[0].jump_down==(frame%48<3) && !t.runtime.input.held[0] && !t.runtime.input.pressed[0]);
        CHECK(t.runtime.input.entities.players[1].keys==0x40 && t.runtime.input.entities.players[1].yaw_delta==64);
        CHECK(f.players[0].motion.movement.heading==(uint16_t)(h0+7));CHECK(f.players[1].motion.movement.heading==(uint16_t)(h1+64));
        CHECK(!f.player.view.third_person && t.m.second.view.third_person && t.frame.local_index==0 && t.m.game.motion_index==1);
    }
    CHECK(t.m.game.shots[9]);
    /* Without the original bypass branch, the adapter reports missing network
     * transport before modifying either player's copied inputs or game bodies. */
    t.game.flags=0;t.count=0;t.frame.presentation_latch=0;
    {
        SsaPlayerRuntimeInput remote=t.input.entities.players[0],local=t.input.entities.players[1];
        int32_t x=f.players[1].motion.x,y=f.players[1].motion.y;uint16_t heading=f.players[1].motion.movement.heading;
        CHECK(ssa_game_runtime_step(&t.runtime,&t.input,&t.ops)==-1);CHECK(p.runtime.last_error==SSA_INPUT_DISPATCH_NETWORK_REQUIRED);
        CHECK(t.count==2 && !t.frame.presentation_latch && !t.runtime.updating && !t.frame.updating);
        CHECK(!memcmp(&t.runtime.input.entities.players[0],&remote,sizeof(remote)) && !memcmp(&t.runtime.input.entities.players[1],&local,sizeof(local)));
        CHECK(f.players[1].motion.x==x && f.players[1].motion.y==y && f.players[1].motion.movement.heading==heading);++pc_network_errors;
    }
    t.runtime.life_game.exit_flag=1;multi_game_tick(&t);++pc_multiplayer_frames;
    CHECK(!p.runtime.last_error && t.runtime.input.entities.players[1].keys==0x40);
    CHECK(ssa_pc_input_runtime_free(&p.runtime)==0);CHECK(ssa_game_runtime_free(&t.runtime)==0);
    CHECK(ssa_pickup_respawn_runtime_free(&t.pickups)==0);CHECK(ssa_actor_animation_runtime_free(&t.actors)==0);
    CHECK(ssa_entity_runtime_free(&t.entities)==0);CHECK(ssa_enemy_queries_free(&t.queries)==0);CHECK(ssa_projectile_runtime_free(&t.projectiles)==0);
    for(i=0;i<2;++i) {CHECK(ssa_weapon_runtime_free(&t.m.weapon[i])==0);CHECK(ssa_player_gameplay_free(&t.m.gp[i])==0);}
    CHECK(ssa_combat_runtime_free(&t.m.combat)==0);CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&t.m.second)==0);ssa_slide_free(&t.m.slide);CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);cleanup(&f);
}
#ifndef SSA_PC_TEST_MAIN
#define SSA_PC_TEST_MAIN main
#endif
int SSA_PC_TEST_MAIN(int argc,char **argv) {
    PcAssets a;unsigned map;CHECK(argc==2);memset(&a,0,sizeof(a));pc_assets(&a,argv[1]);pc_core();
    for(map=0;map<16;++map)pc_map(&a,map);
    pc_direction(&a);pc_edges(&a);pc_sample_gates(&a);pc_multi(&a);CHECK(pc_moves && pc_airborne && pc_third && pc_shots==17);
    free(a.rom);
    printf("{\"assertions\":%u,\"maps\":16,\"core_samples\":%u,\"native_frames\":%u,\"moves\":%u,\"airborne\":%u,\"third_person\":%u,\"startup_or_click_shots\":%u,\"direction_checks\":%u,\"gate_checks\":%u,\"guards\":%u,\"multiplayer_frames\":%u,\"network_required_errors\":%u,\"reentry_checks\":%u}\n",
        checks,pc_samples,pc_frames,pc_moves,pc_airborne,pc_third,pc_shots,pc_direction_checks,pc_gate_checks,pc_guards,pc_multiplayer_frames,pc_network_errors,game_reentry);
    return 0;
}

