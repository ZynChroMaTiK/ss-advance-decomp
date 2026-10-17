/* Host adapter example/integration test; no OS loop or runtime allocator. */
#include "ssa/world.h"
#include "ssa/pickup.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Fixture {
    SsaWorld world;
    SsaEntity entities[4];
    SsaPickup pickups[3];
    SsaInventory inventory;
    SsaPickupPlayer player;
    SsaPickupGame game;
    unsigned log[32], count;
} Fixture;

static void record(Fixture *f,unsigned value) {
    assert(f->count<32); f->log[f->count++]=value;
}
static void observe(void *context,const SsaEntity *current,const SsaEntity *other) {
    Fixture *f=context;
    assert(current==&f->entities[3]);
    record(f,100+(unsigned)(other-f->entities));
}
static void effect(void *context,enum SsaPickupEvent kind,unsigned a,unsigned b) {
    Fixture *f=context;
    (void)b;
    if (kind==SSA_PICKUP_SOUND) assert(a==0x32 || a==0x35);
    if (kind==SSA_PICKUP_MESSAGE) assert(a==0x9d || a==0x8d);
    record(f,300+(unsigned)kind);
}
static void remove_pickup(void *context,SsaPickup *pickup) {
    Fixture *f=context; SsaEntity *entity=pickup->handle;
    record(f,400+(unsigned)(entity-f->entities));
    assert(ssa_world_remove(&f->world,entity));
}
static int execute(void *context,const SsaEntity *current,SsaEntity *other) {
    Fixture *f=context;
    unsigned index=(unsigned)(other-f->entities);
    assert(current==&f->entities[3] && index<3);
    record(f,200+index);
    return ssa_pickup_touch(&f->game,&f->player,&f->pickups[index],0)<0 ? -1 : 0;
}
static void expect_log(Fixture *f,const unsigned *expected,unsigned count) {
    unsigned i;
    assert(f->count==count);
    for (i=0;i<count;++i) assert(f->log[i]==expected[i]);
    f->count=0;
}

int main(int argc,char **argv) {
    FILE *file;
    uint8_t *rom;
    long size;
    unsigned level,j;
    SsaInventoryRules rules;
    const unsigned first[]={102,202,101,201,302,303,401};
    const unsigned second[]={102,202,302,303,402};
    const unsigned third[]={100,200,302,303,400};
    assert(argc==2);
    file=fopen(argv[1],"rb"); assert(file);
    assert(!fseek(file,0,SEEK_END)); size=ftell(file); assert(size>0);
    rewind(file); rom=malloc((size_t)size); assert(rom);
    assert(fread(rom,1,(size_t)size,file)==(size_t)size); fclose(file);
    assert(!ssa_inventory_rules_load(&rules,rom,(size_t)size));
    for (level=0;level<16;++level) {
        SsaMapAssets map={0}; SsaPlayerStart start; Fixture f={0};
        assert(ssa_map_assets_load(&map,rom,(size_t)size,level));
        assert(ssa_map_player_start(&map,&start));
        assert(ssa_world_init(&f.world,&map,8));
        f.inventory.health=100;
        f.player.inventory=&f.inventory; f.player.entity_type=0xc9;
        f.player.handle=&f.entities[3];
        f.game.rules=&rules; f.game.player_count=1;
        f.game.ops.event=effect; f.game.ops.remove=remove_pickup; f.game.ops.context=&f;
        f.world.observe_special=observe; f.world.contact_context=&f;
        f.world.execute_special=execute; f.world.execute_context=&f;
        for (j=0;j<4;++j) {
            SsaEntity *e=&f.entities[j];
            e->body.x=start.x; e->body.y=start.y;
            e->body.height=100*65536; e->radius=32*65536;
            e->type=j==3 ? 0xc9 : 0xe1; e->body.flags=j==3 ? 16 : 0;
            assert(ssa_world_insert(&f.world,e));
            e->body.z=(int32_t)e->body.floor*65536;
            if (j<3) { f.pickups[j].item=j==1 ? 0x20 : 0x30; f.pickups[j].handle=e; }
        }
        assert(ssa_world_query(&f.world,&f.entities[3],start.x,start.y)>=0);
        expect_log(&f,first,sizeof(first)/sizeof(*first));
        assert(f.inventory.health==100 && f.inventory.pending_count==1 && f.inventory.pending_weapons[0]==2);
        assert(f.game.collected==1 && f.world.entity_count==3 && f.world.special_count==2);
        assert(!f.entities[1].owner && f.entities[0].owner && f.entities[2].owner);
        f.inventory.health=50;
        assert(ssa_world_query(&f.world,&f.entities[3],start.x,start.y)>=0);
        expect_log(&f,second,sizeof(second)/sizeof(*second));
        assert(f.inventory.health==75 && f.game.collected==2 && f.world.entity_count==2);
        assert(f.entities[0].owner && !f.entities[2].owner);
        assert(ssa_world_query(&f.world,&f.entities[3],start.x,start.y)>=0);
        expect_log(&f,third,sizeof(third)/sizeof(*third));
        assert(f.inventory.health==100 && f.game.collected==3 && f.world.entity_count==1);
        assert(ssa_world_query(&f.world,&f.entities[3],start.x,start.y)>=0);
        assert(!f.count && !f.world.special_count && f.game.collected==3);
        ssa_world_free(&f.world); ssa_map_assets_free(&map);
    }
    free(rom);
    puts("PASS 64 native pickup/world queries on all 16 real maps");
    return 0;
}
