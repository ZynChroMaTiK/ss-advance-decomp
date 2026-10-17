#include "ssa/player_runtime.h"
#include "ssa/pickup.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, frames, airborne_frames, third_person_frames;
static unsigned pickup_cases, error_cases, sounds, animations, reentry_cases, metadata_cases;
#define CHECK(x) do { assert(x); ++checks; } while (0)

typedef struct TestContext {
    const SsaPlayerRuntimeInput *input;
    const SsaPlayerRuntimeOps *ops;
    int phase, fail;
    SsaEntity pickup_entity;
    SsaPickup pickup;
    SsaPickupGame pickup_game;
    SsaPickupPlayer pickup_player;
    SsaPlayerRuntime *runtime;
    unsigned removed, pickup_events;
} TestContext;

static int gameplay(void *context, SsaPlayerRuntime *r, enum SsaPlayerFrameStage stage)
{
    TestContext *c = context;
    CHECK(r->player->entity_health == r->player->inventory.health);
    CHECK(r->player->vertical.flags == r->player->motion.movement.flags);
    CHECK(r->frame.x == r->player->motion.x && r->frame.y == r->player->motion.y);
    if (stage == SSA_PLAYER_FRAME_DEATH) {
        CHECK(c->phase == 0); c->phase = 1;
        CHECK(ssa_player_runtime_step(r, c->input, c->ops) == -1);
        CHECK(ssa_player_runtime_free(r) == -1);
        CHECK(!ssa_player_runtime_key(r, 'H', 1, 0, 1));
        ++reentry_cases;
        if (c->fail == 1) { r->player->inventory.health = 17; return -1; }
    } else {
        CHECK(stage == SSA_PLAYER_FRAME_HAZARDS && c->phase == 1); c->phase = 2;
        if (r->game.local_player == r->player->entity.player_index) {
            CHECK(r->game.view.x == r->player->motion.x);
            CHECK(r->game.view.y == r->player->motion.y);
            CHECK(r->game.view.z == r->camera.eye_z);
            CHECK(r->game.view.heading == r->player->motion.movement.heading);
        }
        if (c->fail == 2) { r->player->inventory.health = 23; return -1; }
    }
    return 0;
}

static int sector(void *context, SsaPlayerRuntime *r, int16_t id)
{
    TestContext *c = context;
    CHECK(c->phase == 2); c->phase = 3;
    CHECK(id == r->player->entity.sector);
    if (r->game.local_player == r->player->entity.player_index) CHECK(id == r->game.view.sector);
    if (c->fail == 3) { r->player->inventory.health = 29; return -1; }
    return 0;
}

static int animation(void *context, SsaPlayerRuntime *r, unsigned id, unsigned ticks)
{
    TestContext *c = context;
    CHECK(c->phase == 3 && (id == 0x65 || id == 0x66) && ticks == 4);
    CHECK(r->player->animation == id && r->player->animation_ticks == ticks);
    CHECK(r->frame.animation_state == (id == 0x66));
    ++animations;
    if (c->fail == 4) return -1;
    return 0;
}

static int sound(void *context, SsaPlayerRuntime *r, unsigned id)
{
    TestContext *c = context;
    CHECK(c->phase == 1 && (id == 13 || id == 14));
    CHECK(r->frame.eye_z == r->camera.eye_z);
    ++sounds;
    if (c->fail == 5) { r->player->inventory.health = 31; return -1; }
    return 0;
}

static void pickup_event(void *context, enum SsaPickupEvent kind, unsigned a, unsigned b)
{
    TestContext *c = context;
    CHECK(kind == SSA_PICKUP_SOUND || kind == SSA_PICKUP_MESSAGE);
    CHECK(a != 0 && b == 0);
    ++c->pickup_events;
}

static void pickup_remove(void *context, SsaPickup *pickup)
{
    TestContext *c = context;
    CHECK(pickup == &c->pickup);
    CHECK(ssa_world_remove(c->runtime->world, &c->pickup_entity));
    ++c->removed;
}

static int pickup_execute(void *context, const SsaEntity *player, SsaEntity *other)
{
    TestContext *c = context;
    CHECK(player == &c->runtime->player->entity && other == &c->pickup_entity);
    return ssa_pickup_touch(&c->pickup_game, &c->pickup_player, &c->pickup, 0) < 0 ? -1 : 0;
}

static void run(SsaPlayerRuntime *r, SsaPlayerRuntimeInput *input,
                SsaPlayerRuntimeOps *ops, TestContext *c)
{
    SsaViewPose expected;
    size_t contacts;
    int32_t blocking;
    c->phase = 0;
    CHECK(ssa_player_runtime_step(r, input, ops) == 0);
    CHECK(c->phase == 3 && !r->updating);
    CHECK(r->player->entity.owner == r->world);
    CHECK(r->player->motion.x == r->player->entity.body.x);
    CHECK(r->player->motion.y == r->player->entity.body.y);
    CHECK(r->player->motion.z == r->player->entity.body.z);
    CHECK(r->player->motion.velocity_z == r->player->entity.body.velocity_z);
    CHECK(r->player->entity_health == r->player->inventory.health);
    CHECK(r->player->vertical.flags == r->player->motion.movement.flags);
    CHECK(r->view_collision.visited != r->world->collision.visited);
    contacts = r->world->special_count; blocking = r->world->collision.blocking_line;
    CHECK(ssa_view_pose(&r->view, &r->player->entity.body, &r->camera,
          r->player->motion.movement.heading, r->tables, &r->view_collision, &expected));
    CHECK(expected.x == r->pose.x && expected.y == r->pose.y && expected.z == r->pose.z);
    CHECK(expected.target_x == r->pose.target_x && expected.target_y == r->pose.target_y && expected.target_z == r->pose.target_z);
    CHECK(expected.third_person == r->pose.third_person && expected.boom_distance == r->pose.boom_distance);
    CHECK(contacts == r->world->special_count && blocking == r->world->collision.blocking_line);
    if (r->player->entity.body.z > (int32_t)r->player->entity.body.floor * 65536) ++airborne_frames;
    if (r->pose.third_person) ++third_person_frames;
    ++frames;
}

int main(int argc, char **argv)
{
    FILE *file;
    uint8_t *rom;
    long size;
    SsaMathTables tables;
    SsaSpawnDefinitions definitions;
    SsaInventoryRules rules;
    unsigned level, frame;
    CHECK(argc == 2); file = fopen(argv[1], "rb"); CHECK(file);
    CHECK(fseek(file, 0, SEEK_END) == 0); size = ftell(file); CHECK(size > 0); rewind(file);
    rom = malloc((size_t)size); CHECK(rom); CHECK(fread(rom, 1, (size_t)size, file) == (size_t)size); fclose(file);
    CHECK(ssa_math_tables_load(&tables, rom, (size_t)size));
    CHECK(ssa_spawn_definitions_load(&definitions, rom, (size_t)size));
    CHECK(ssa_inventory_rules_load(&rules, rom, (size_t)size) == 0);

    for (level = 0; level < 16; ++level) {
        SsaMapAssets map = {0};
        SsaSpawnPlan plan = {0};
        SsaWorld world = {0};
        SsaSlide slide = {0};
        SsaPlayerSpawn players[4] = {0};
        SsaPlayerRuntime r = {0};
        SsaPlayerRuntimeInput input = {0};
        TestContext context = {0};
        SsaPlayerRuntimeOps ops = {gameplay, sector, animation, sound, &context};
        SsaPlayerSpawn *p = &players[0];
        unsigned airborne_before = airborne_frames;
        int had_jump_room;
        CHECK(ssa_map_assets_load(&map, rom, (size_t)size, level));
        CHECK(ssa_spawn_plan_load(&plan, &map, &definitions, level, 1, 100));
        CHECK(ssa_world_init(&world, &map, 104)); CHECK(ssa_slide_init(&slide, &map));
        CHECK(ssa_players_spawn(players, &world, &plan, 1, 0)); CHECK(p->active);
        CHECK(p->inventory.health == 0 && p->entity_health == 100);
        CHECK(ssa_player_runtime_init(&r, p, &world, &slide, &tables) == 0);
        CHECK(p->inventory.health == 100 && !r.view.third_person);
        CHECK(ssa_player_runtime_init(&r, p, &world, &slide, &tables) == -1); ++error_cases;
        CHECK(r.view_collision.map == &map && r.view_collision.visited != world.collision.visited);
        input.motion.ticks = 1; input.motion.player_count = 1;
        input.controls_enabled = 1;
        context.input = &input; context.ops = &ops; context.runtime = &r;
        had_jump_room = p->entity.body.ceiling - p->entity.body.floor > 100;

        /* Hold jump through landing: exactly one launch, then release. The
         * real map ceiling can shorten the arc. H remains held without repeats. */
        CHECK(ssa_player_runtime_key(&r, 'H', 1, 0, 1));
        for (frame = 0; frame < 64; ++frame) {
            int32_t previous_vz = p->motion.velocity_z;
            input.jump_down = (uint8_t)(frame < 60); input.camera_counter = (uint16_t)frame;
            CHECK(!ssa_player_runtime_key(&r, 'H', 1, 1, 1));
            run(&r, &input, &ops, &context);
            if (frame > 0 && frame < 60 && previous_vz <= 0) CHECK(p->motion.velocity_z <= 0);
        }
        if (had_jump_room) CHECK(airborne_frames > airborne_before);
        CHECK(p->entity.body.z == (int32_t)p->entity.body.floor * 65536);
        CHECK(!ssa_player_runtime_key(&r, 'H', 0, 0, 1));
        CHECK(ssa_player_runtime_key(&r, 'H', 1, 0, 1) && !r.view.third_person);
        ssa_player_runtime_focus_lost(&r); CHECK(!r.view.h_down);
        CHECK(!ssa_player_runtime_key(&r, 'H', 1, 0, 0));
        CHECK(!ssa_player_runtime_key(&r, 'H', 1, 0, 1));
        ssa_player_runtime_focus_lost(&r);

        /* A genuine E1 pickup executes synchronously inside native horizontal
         * movement. Inventory health remains authoritative through camera/hooks. */
        context.pickup_entity.type = 0xe1;
        context.pickup_entity.body.x = p->entity.body.x;
        context.pickup_entity.body.y = p->entity.body.y;
        CHECK(ssa_world_insert(&world, &context.pickup_entity));
        context.pickup.item = 0x30;
        context.pickup_player.inventory = &p->inventory;
        context.pickup_player.entity_type = p->entity.type;
        context.pickup_game.rules = &rules; context.pickup_game.player_count = 1;
        context.pickup_game.ops.event = pickup_event; context.pickup_game.ops.remove = pickup_remove;
        context.pickup_game.ops.context = &context;
        world.execute_special = pickup_execute; world.execute_context = &context;
        p->inventory.health = 50; input.keys = 0x40;
        run(&r, &input, &ops, &context);
        CHECK(context.removed == 1 && context.pickup_events == 2 && context.pickup_game.collected == 1);
        CHECK(p->inventory.health == 75 && p->entity_health == 75);
        CHECK(!context.pickup_entity.owner); ++pickup_cases;
        world.execute_special = NULL; world.execute_context = NULL;

        for (frame = 0; frame < 256; ++frame) {
            uint16_t heading = p->motion.movement.heading;
            input.keys = frame % 64 < 16 ? 0x40 : frame % 64 < 32 ? 0x200 : frame % 64 < 48 ? 0x80 : 0x100;
            input.yaw_delta = frame % 2 ? -19 : 37;
            input.jump_down = (uint8_t)(frame % 64 < 20);
            input.controls_enabled = (uint8_t)(frame % 64 < 56);
            input.camera_counter = (uint16_t)(64 + frame);
            if (frame % 32 == 0) {
                CHECK(ssa_player_runtime_key(&r, 'H', 1, 0, 1));
                CHECK(!ssa_player_runtime_key(&r, 'H', 0, 0, 1));
            }
            run(&r, &input, &ops, &context);
            CHECK(p->motion.movement.heading == (uint16_t)((uint32_t)heading +
                  (input.controls_enabled ? (uint32_t)input.yaw_delta : 0)));
            if (!input.controls_enabled) CHECK(p->motion.movement.keys == 0);
        }

        /* GAME+68 controls protection, push and the multiplayer movement gate;
         * the distinct display index controls camera sound and view copying. */
        {
            SsaEntity peer = {0};
            const SsaEntity *peers[2] = {&p->entity, &peer};
            int32_t x = p->entity.body.x, y = p->entity.body.y;
            SsaPlayerLocalView saved_view;
            peer.body.x = x + 128 * 65536; peer.body.y = y;
            input.motion.players = peers; input.motion.player_count = 2;
            input.motion.local_player = 1; input.local_player = 0;
            input.keys = 0x40; input.controls_enabled = 1; input.yaw_delta = 0;
            p->entity.body.flags |= 0x80; r.frame.timer = 1;
            for (frame = 0; frame < 8; ++frame) {
                run(&r, &input, &ops, &context);
                CHECK(p->entity.body.x == x && p->entity.body.y == y);
                CHECK(r.game.protected_player == 1 && r.game.local_player == 0);
                CHECK(r.camera_frame.is_local && !(p->entity.body.flags & 0x80));
                ++metadata_cases;
            }
            saved_view = r.game.view;
            input.motion.local_player = 0; input.local_player = 1;
            p->entity.body.flags |= 0x80; r.frame.timer = 1;
            for (frame = 0; frame < 8; ++frame) {
                run(&r, &input, &ops, &context);
                CHECK(r.game.protected_player == 0 && r.game.local_player == 1);
                CHECK(!r.camera_frame.is_local && (p->entity.body.flags & 0x80));
                CHECK(memcmp(&saved_view, &r.game.view, sizeof(saved_view)) == 0);
                ++metadata_cases;
            }
            input.motion.players = NULL; input.motion.player_count = 1;
            input.motion.local_player = 0; input.local_player = 0;
        }

        /* Missing boundaries reject without mutation. Callback errors retain
         * effects, mirror health, clear the busy flag, and preserve old pose. */
        {
            SsaPlayerRuntimeOps broken = ops;
            SsaViewPose pose = r.pose;
            int16_t health = p->inventory.health;
            unsigned failure;
            broken.sound = NULL;
            CHECK(ssa_player_runtime_step(&r, &input, &broken) == -1);
            CHECK(p->inventory.health == health && !r.updating); ++error_cases;
            for (failure = 1; failure <= 3; ++failure) {
                context.fail = (int)failure; context.phase = 0;
                CHECK(ssa_player_runtime_step(&r, &input, &ops) == -1);
                CHECK(p->inventory.health == (failure == 1 ? 17 : failure == 2 ? 23 : 29));
                CHECK(p->entity_health == p->inventory.health && !r.updating);
                CHECK(memcmp(&pose, &r.pose, sizeof(pose)) == 0); ++error_cases;
            }
            input.keys = 0x40; input.controls_enabled = 1;
            r.frame.animation_state = 0; context.fail = 4; context.phase = 0;
            CHECK(ssa_player_runtime_step(&r, &input, &ops) == -1);
            CHECK(context.phase == 3 && !r.updating && r.frame.animation_state == 1);
            CHECK(memcmp(&pose, &r.pose, sizeof(pose)) == 0); ++error_cases;
            r.camera_frame.previous_bob = -1; input.camera_counter = 0;
            context.fail = 5; context.phase = 0;
            CHECK(ssa_player_runtime_step(&r, &input, &ops) == -1);
            CHECK(context.phase == 1 && !r.updating);
            CHECK(p->inventory.health == 31 && p->entity_health == 31);
            CHECK(memcmp(&pose, &r.pose, sizeof(pose)) == 0); ++error_cases;
            context.fail = 0;
            input.motion.player_count = 5;
            CHECK(ssa_player_runtime_step(&r, &input, &ops) == -1); ++error_cases;
            input.motion.player_count = 1;
            input.motion.ticks = 0;
            input.jump_down = (uint8_t)!r.jump.was_down;
            {
                uint8_t latch = r.jump.was_down;
                run(&r, &input, &ops, &context); CHECK(r.jump.was_down == latch);
            }
        }
        CHECK(ssa_player_runtime_free(&r) == 0); CHECK(!r.player && !r.view_collision.map);
        CHECK(p->entity.owner == &world); CHECK(ssa_player_runtime_free(&r) == 0);
        ssa_world_free(&world); ssa_slide_free(&slide);
        CHECK(ssa_spawn_plan_free(&plan)); ssa_map_assets_free(&map);
    }
    CHECK(airborne_frames > 0 && third_person_frames > 0 && sounds > 0 && animations > 0);
    CHECK(ssa_player_runtime_step(NULL, NULL, NULL) == -1);
    CHECK(ssa_player_runtime_free(NULL) == -1);
    CHECK(!ssa_player_runtime_key(NULL, 'H', 1, 0, 1));
    free(rom);
    printf("{\"assertions\":%u,\"real_map_frames\":%u,\"maps\":16,\"airborne_frames\":%u,"
           "\"third_person_frames\":%u,\"pickup_cases\":%u,\"error_cases\":%u,"
           "\"reentry_cases\":%u,\"footstep_events\":%u,\"animation_events\":%u,\"metadata_cases\":%u}\n",
           checks, frames, airborne_frames, third_person_frames, pickup_cases, error_cases,
           reentry_cases, sounds, animations, metadata_cases);
    return 0;
}
