/* Two native player bindings, explicit network boundary, and ownership guards. */
typedef struct ExtraCombat {
    SsaCombatRuntime *runtime;
    SsaCombatRuntimeOps ops;
    SsaEntity *target;
    SsaCombatDamageEvent expected;
    uint32_t expected_a, expected_b;
    unsigned calls, fail;
} ExtraCombat;
static int extra_effect(void *context,const SsaCombatRuntime *r,
    const SsaCombatDamageTarget *v,SsaCombatDamageEvent kind,uint32_t a,uint32_t b) {
    ExtraCombat *c=context;
    unsigned i;
    CHECK(r==c->runtime && v->entity==c->target);
    CHECK(kind==c->expected && a==c->expected_a && b==c->expected_b);
    CHECK(r->updating && r->spawn->updating && r->damage->updating);
    for(i=0;i<r->player_count;++i) {
        CHECK(r->players[i]->updating);
        CHECK(r->players[i]->player->entity_health==r->players[i]->player->inventory.health);
        CHECK(ssa_player_runtime_free(r->players[i])==-1);++combat_guards;
    }
    CHECK(ssa_combat_runtime_free(c->runtime)==-1);
    CHECK(ssa_combat_runtime_damage(c->runtime,NULL,NULL,NULL,1,&c->ops)==-1);
    CHECK(ssa_script_spawn_free(r->spawn)==-1);
    combat_guards+=3;++combat_callbacks;++c->calls;
    return c->fail ? -1 : 0;
}
static void extra_combat_cases(uint8_t *rom,size_t size,const SsaMathTables *math,
    const SsaSpawnDefinitions *defs,unsigned thing,int32_t speed) {
    Fixture f;
    SsaPlayerRuntime second={0};SsaSlide second_slide={0};
    SsaCombatRuntime r={0},invalid={0};SsaCombatDamage g={0};
    SsaEntity other={0},foreign={0};SsaEntity *reserved[3],*bad_reserved[3];
    SsaPlayerRuntime *players[2],*bad_players[2];
    uint32_t score=0;unsigned before;uint16_t movement_flags;
    ExtraCombat c={0};
    (void)thing;(void)speed;
    setup(&f,rom,size,0,math,defs,100);
    CHECK(ssa_script_spawn_free(&f.spawn)==0);
    f.players[1]=f.players[0];
    memset(&f.players[1].entity,0,sizeof(f.players[1].entity));
    f.players[1].entity.body=f.players[0].entity.body;
    f.players[1].entity.radius=f.players[0].entity.radius;
    f.players[1].entity.type=f.players[0].entity.type;
    f.players[1].entity.player_index=1;
    CHECK(ssa_world_insert(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_slide_init(&second_slide,&f.map));
    CHECK(ssa_player_runtime_init(&second,&f.players[1],&f.world,&second_slide,math)==0);
    other.body=f.players[0].entity.body;other.body.flags=0;other.type=0xe1;
    CHECK(ssa_world_insert(&f.world,&other)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    CHECK(f.spawn.reserved==3 && f.spawn.pool.active_count==3);
    reserved[0]=&f.players[0].entity;reserved[1]=&f.players[1].entity;reserved[2]=&other;
    players[0]=&f.player;players[1]=&second;
    g.player_count=2;g.difficulty=1;
    c.runtime=&r;c.target=reserved[1];c.ops=(SsaCombatRuntimeOps){extra_effect,&c};
    c.expected=SSA_COMBAT_DAMAGE_NETWORK;c.expected_a=1;c.expected_b=25;
#define EXTRA_INIT(rv, rr, rc, pp, pc) ssa_combat_runtime_init(rv,&f.spawn,rr,rc,pp,pc,&g,&score)
    CHECK(EXTRA_INIT(&invalid,reserved,2,players,2)==-1);++combat_guards;
    CHECK(EXTRA_INIT(&invalid,NULL,3,players,2)==-1);++combat_guards;
    CHECK(EXTRA_INIT(&invalid,reserved,3,NULL,2)==-1);++combat_guards;
    CHECK(EXTRA_INIT(&invalid,reserved,3,players,0)==-1);++combat_guards;
    CHECK(EXTRA_INIT(&invalid,reserved,3,players,5)==-1);++combat_guards;
    memcpy(bad_reserved,reserved,sizeof(reserved));bad_reserved[1]=bad_reserved[0];
    CHECK(EXTRA_INIT(&invalid,bad_reserved,3,players,2)==-1);++combat_guards;
    bad_reserved[1]=&foreign;
    CHECK(EXTRA_INIT(&invalid,bad_reserved,3,players,2)==-1);++combat_guards;
    bad_players[0]=players[0];bad_players[1]=players[0];
    CHECK(EXTRA_INIT(&invalid,reserved,3,bad_players,2)==-1);++combat_guards;
    bad_players[1]=NULL;
    CHECK(EXTRA_INIT(&invalid,reserved,3,bad_players,2)==-1);++combat_guards;
    g.local_player=2;
    CHECK(EXTRA_INIT(&invalid,reserved,3,players,2)==-1);g.local_player=0;++combat_guards;
    g.player_count=1;
    CHECK(EXTRA_INIT(&invalid,reserved,3,players,2)==-1);g.player_count=2;++combat_guards;
    CHECK(!invalid.spawn && !invalid.updating);
    CHECK(EXTRA_INIT(&r,reserved,3,players,2)==0);
#undef EXTRA_INIT
    /* The binding copies slot-order pointers rather than retaining this array. */
    reserved[2]=NULL;CHECK(r.reserved[2]==&other);reserved[2]=&other;
    g.source=&other;g.source_owner=reserved[1];g.local_flags=0x5555;
    movement_flags=f.players[1].motion.movement.flags;
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],reserved[0],NULL,0x10019,&c.ops)==0);
    CHECK(c.calls==1 && f.players[1].inventory.health==100 && f.players[1].entity_health==100);
    CHECK(!second.frame.timer && !(reserved[1]->body.flags&1));
    CHECK(g.source==&other && g.source_owner==reserved[1] && g.local_flags==0x5555);
    CHECK(!r.updating && !g.updating && !f.spawn.updating && !f.player.updating && !second.updating);
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],&other,reserved[0],25,&c.ops)==0);
    CHECK(c.calls==2 && f.players[1].inventory.health==100);
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],reserved[1],NULL,25,&c.ops)==0);
    CHECK(c.calls==2 && f.players[1].inventory.health==100);
    c.fail=1;
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],reserved[0],NULL,25,&c.ops)==-1);++combat_failures;
    CHECK(c.calls==3 && f.players[1].inventory.health==100 && f.players[1].entity_health==100);
    CHECK(!r.updating && !g.updating && !f.spawn.updating && !f.player.updating && !second.updating);
    CHECK(g.source==&other && g.source_owner==reserved[1] && g.local_flags==0x5555);c.fail=0;
    /* NULL source applies an already received multiplayer hit locally. */
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],NULL,NULL,25,&c.ops)==0);++damage_hits;
    CHECK(c.calls==3 && f.players[1].inventory.health==75 && f.players[1].entity_health==75);
    CHECK(!second.frame.timer && !(reserved[1]->body.flags&1));
    CHECK(f.players[1].motion.movement.flags==movement_flags);
    g.local_player=1;c.expected=SSA_COMBAT_DAMAGE_SOUND;c.expected_a=15;c.expected_b=0;
    CHECK(ssa_combat_runtime_damage(&r,reserved[1],NULL,NULL,25,&c.ops)==0);++damage_hits;
    CHECK(c.calls==4 && g.hurt_flash==9 && f.players[1].inventory.health==50 && f.players[1].entity_health==50);
    CHECK(f.players[1].motion.movement.flags==movement_flags && !score);
    before=c.calls;
    CHECK(ssa_combat_runtime_damage(&r,&other,NULL,NULL,1,&c.ops)==-1);++combat_guards;
    CHECK(ssa_combat_runtime_damage(&r,&foreign,NULL,NULL,1,&c.ops)==-1);++combat_guards;
    CHECK(ssa_combat_runtime_damage(&r,NULL,NULL,NULL,1,&c.ops)==0);
    CHECK(c.calls==before && !r.updating && !f.spawn.updating);
#define EXTRA_LOCK(field) do { field=1; \
    CHECK(ssa_combat_runtime_damage(&r,NULL,NULL,NULL,1,&c.ops)==-1); \
    field=0;++combat_guards; } while(0)
    EXTRA_LOCK(f.spawn.updating);EXTRA_LOCK(second.updating);EXTRA_LOCK(g.updating);
    EXTRA_LOCK(f.world.query_active);EXTRA_LOCK(f.level.updating);
    EXTRA_LOCK(f.level.script.updating);EXTRA_LOCK(f.level.movers.updating);
    EXTRA_LOCK(f.level.switches.updating);EXTRA_LOCK(r.explosion.updating);
#undef EXTRA_LOCK
    CHECK(ssa_combat_runtime_free(&r)==0);
    CHECK(ssa_script_spawn_free(&f.spawn)==0);
    CHECK(ssa_player_runtime_free(&second)==0);ssa_slide_free(&second_slide);
    CHECK(ssa_world_remove(&f.world,&f.players[1].entity)==1);
    CHECK(ssa_world_remove(&f.world,&other)==1);
    CHECK(ssa_script_spawn_init(&f.spawn,&f.level,&f.world)==0);
    cleanup(&f);
}
