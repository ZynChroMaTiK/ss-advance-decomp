#define SSA_PROJECTILE_TEST_MAIN projectile_fixture_main
#include "test_projectile_runtime.c"
#include "ssa/enemy_queries.h"

static unsigned conversion_frames,conversion_moves,conversion_drains,conversion_failures;
static unsigned conversion_level,conversion_retargets,conversion_effect_frames;
static int conversion_presentation(void *context,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,
    SsaEntity *other,int32_t a,int32_t b,int32_t d) {
    CdCase *c=context;SsaScriptActor *actor=&c->f->spawn.actors[c->runtime.slot];
    CHECK(actor->parameter34==cd->drain_period && actor->parameter36==cd->drain_countdown);
    return cd_event(context,cd,kind,other,a,b,d);
}
static int conversion_cd_tick(ProjectileCase *t,CdCase *c,SsaEnemyQueries *q,unsigned ticks) {
    SsaCombatCdOps ops={&t->weapon.combat_ops,conversion_presentation,c};
    SsaScriptActor *a=&c->f->spawn.actors[c->runtime.slot];int result;
    int32_t x=a->entity.body.x,y=a->entity.body.y;int16_t health=a->activation.health;
    c->frame.ticks=ticks;++c->frame.frame_counter;
    result=ssa_combat_cd_step(&t->weapon.combat,&c->runtime,&c->frame,&q->ops,&ops);
    CHECK(!c->runtime.updating && !c->f->spawn.updating && !t->weapon.combat.updating);
    CHECK(c->runtime.controller.drain_period==a->parameter34);
    CHECK(c->runtime.controller.drain_countdown==a->parameter36);
    if(a->entity.body.x!=x || a->entity.body.y!=y)++conversion_moves;
    if(a->activation.health<health)++conversion_drains;
    ++conversion_frames;return result;
}
static int conversion_find_target(Fixture *f,SsaScriptActor *converted,SsaScriptActor *other) {
    static const int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    unsigned distance,direction;
    for(distance=768;distance>=384;distance-=64) for(direction=0;direction<8;++direction) {
        int moved=ssa_world_try_move(&f->world,&other->entity,
            converted->entity.body.x+dirs[direction][0]*(int32_t)distance*65536,
            converted->entity.body.y+dirs[direction][1]*(int32_t)distance*65536);
        CHECK(moved>=0);if(moved)return 1;
    }
    return 0;
}
static void conversion_case(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *anim,unsigned thing,int32_t speed) {
    unsigned level,found=0;
    for(level=0;level<16 && !found;++level) {
        Fixture f;ProjectileCase t;CdCase c;SsaEnemyQueries q={0};SsaEntity *reserved[1];
        SsaScriptActor *a,*other;unsigned second,dir;
        setup(&f,rom,size,level,math,defs,100);projectile_init(&t,&f,inv,rules,anim,math);
        script(&f,rom,thing,1,2);step(&f,1);step(&f,1);CHECK(owned(&f)==2);
        bind(&c,&f,math,speed);activate(&c);a=&f.spawn.actors[c.runtime.slot];
        second=f.spawn.pool.active_slots[2];CHECK(second!=c.runtime.slot);other=&f.spawn.actors[second];
        CHECK(ssa_script_spawn_activate(&f.spawn,second,59,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,second,1,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,second,29,&f.ops)==0);
        CHECK(ssa_script_spawn_activate(&f.spawn,second,1,&f.ops)==1);
        reserved[0]=&f.players[0].entity;
        CHECK(ssa_enemy_queries_init(&q,&f.spawn,math,reserved,1)==0);
        for(dir=0;dir<4 && !found;++dir) {
            static const int dx[4]={0,1,0,-1},dy[4]={1,0,-1,0};
            SsaPlayerSpawn *player=&f.players[0];unsigned slot,effect=100,i;int rejected;
            int32_t x=player->entity.body.x+(dx[dir]*208+dy[dir]*40)*65536;
            int32_t y=player->entity.body.y+(dy[dir]*208-dx[dir]*40)*65536;
            int moved=ssa_world_try_move(&f.world,&a->entity,x,y);
            CHECK(moved>=0);if(!moved)continue;
            CHECK(q.ops.visibility(&q,player->entity.sector,a->entity.sector,&rejected)==0);
            if(rejected || !conversion_find_target(&f,a,other))continue;
            player->motion.facing=(uint8_t)(dir*64);player->motion.movement.heading=(uint16_t)(dir*16384);
            a->parameter36=17;
            slot=projectile_fire(&t,7);CHECK(projectile_tick(&t,slot,1)>=0);
            if(!(a->entity.body.flags&0x400)) {
                if(t.runtime.active[slot])CHECK(ssa_projectile_runtime_release(&t.runtime,slot)==0);
                continue;
            }
            CHECK(!t.runtime.active[slot] && a->parameter34==30 && a->parameter36==17);
            CHECK(a->activation.health==30 && a->activation.state==1);
            for(i=0;i<100;++i)if(t.runtime.active[i] && t.runtime.projectiles[i].entity->type==0xe2)effect=i;
            CHECK(effect<100);found=1;conversion_level=level;
            CHECK(!c.runtime.navigation.target);
            for(i=0;i<17;++i) {
                CHECK(conversion_cd_tick(&t,&c,&q,1)==0);
                CHECK(a->activation.state==1 && a->activation.health==30);
                CHECK(a->parameter34==30 && a->parameter36==16-i);
                CHECK(c.runtime.navigation.target==&other->entity);
                CHECK(projectile_tick(&t,effect,1)==0);++conversion_effect_frames;
                CHECK(t.runtime.projectiles[effect].entity->body.x==a->entity.body.x);
                CHECK(t.runtime.projectiles[effect].entity->body.y==a->entity.body.y);
            }
            ++conversion_retargets;
            CHECK(conversion_cd_tick(&t,&c,&q,1)==0);
            CHECK(a->activation.health==29 && a->parameter36==30 && a->parameter34==30);
            CHECK(a->entity.body.flags&0x400);
            /* Retarget timer +56 is distinct from +34/+36 and refreshes via
             * native nearest-enemy query. It is not a conversion expiry. */
            c.runtime.navigation.target_timer=1;
            CHECK(conversion_cd_tick(&t,&c,&q,1)==0);
            CHECK(c.runtime.navigation.target_timer==600 && c.runtime.navigation.target==&other->entity);
            CHECK(a->parameter36==29 && a->activation.health==29);++conversion_retargets;
            CHECK(ssa_combat_runtime_damage(&t.weapon.combat,&a->entity,&player->entity,NULL,28,&t.weapon.combat_ops)==0);
            CHECK(a->activation.health==1 && a->entity.body.flags&1);
            c.fail_event=SSA_ENEMY_CD_ANIMATION+1;
            CHECK(conversion_cd_tick(&t,&c,&q,1)==-1);++conversion_failures;
            CHECK(a->parameter36==28 && a->parameter34==30 && a->activation.health==1);
            c.fail_event=0;
            for(i=0;i<28;++i) {
                CHECK(conversion_cd_tick(&t,&c,&q,1)==0);
                CHECK(a->activation.state==1 && a->activation.health==1);
                CHECK(a->parameter36==27-i);
                CHECK(projectile_tick(&t,effect,1)==0);++conversion_effect_frames;
            }
            CHECK(conversion_cd_tick(&t,&c,&q,1)==0);
            CHECK(a->activation.state==2 && !a->activation.health && a->group_released);
            CHECK(a->parameter34==30 && !a->parameter36 && (a->entity.body.flags&0x420)==0x420);
            CHECK(projectile_tick(&t,effect,1)==1 && !t.runtime.active[effect]);
            CHECK(!projectile_count(&t) && f.world.entity_count==3);
            CHECK(ssa_enemy_queries_free(&q)==0);
            CHECK(ssa_script_spawn_defeat(&f.spawn,second)==0);
            CHECK(ssa_script_spawn_release(&f.spawn,second)==0);
            expire(&c);
        }
        if(!found)CHECK(ssa_enemy_queries_free(&q)==0);
        projectile_cleanup(&t);cleanup(&f);
    }
    CHECK(found && conversion_moves && conversion_drains==2);
}
int main(int argc,char **argv) {
    FILE *file;long size;uint8_t *rom;SsaMathTables math;SsaSpawnDefinitions defs;
    SsaInventoryRules inventory;SsaWeaponRules rules;SsaWeaponAnimations animations;
    int32_t speeds[12];unsigned i,thing=100;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));size=ftell(file);rewind(file);
    CHECK(size>0);rom=malloc((size_t)size);CHECK(rom);CHECK(fread(rom,1,(size_t)size,file)==(size_t)size);CHECK(!fclose(file));
    CHECK(ssa_math_tables_load(&math,rom,(size_t)size));CHECK(ssa_spawn_definitions_load(&defs,rom,(size_t)size));
    CHECK(ssa_enemy_navigation_speeds(speeds,rom,(size_t)size)==1);
    CHECK(ssa_inventory_rules_load(&inventory,rom,(size_t)size)==0);
    CHECK(ssa_weapon_rules_load(&rules,rom,(size_t)size)==1);
    CHECK(ssa_weapon_animations_load(&animations,rom,(size_t)size)==1);
    for(i=0;i<64;++i)if(defs.thing[i].type==0xcd && !defs.thing[i].variant){thing=i;break;}
    CHECK(thing<64);conversion_case(rom,(size_t)size,&math,&defs,&inventory,&rules,&animations,thing,speeds[0]);
    free(rom);
    printf("{\"assertions\":%u,\"level\":%u,\"conversion_frames\":%u,\"native_enemy_moves\":%u,\"drains\":%u,\"retargets\":%u,\"effect_frames\":%u,\"callback_failures\":%u,\"corpse_releases\":%u}\n",
        checks,conversion_level,conversion_frames,conversion_moves,conversion_drains,conversion_retargets,
        conversion_effect_frames,conversion_failures,released);
    return 0;
}
