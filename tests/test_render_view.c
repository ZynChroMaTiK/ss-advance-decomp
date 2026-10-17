#define SSA_PC_TEST_MAIN render_pc_fixture_main
#include "test_pc_input.c"
#include "ssa/render_view.h"
#include "ssa/raster_fill.h"

typedef struct RenderCase {
    SsaRenderView view;
    SsaRenderViewOps ops;
    uint8_t pixels[14400];
    unsigned events;
} RenderCase;
static unsigned render_frames,render_airborne,render_third,render_guards;
static int render_event(void *ctx,SsaRenderView *v,SsaRenderViewEvent event,uint32_t arg)
{
    RenderCase *r=ctx;
    CHECK(v==&r->view && v->updating);
    CHECK(ssa_render_view_step(v,&r->ops)==-1);
    CHECK(ssa_render_view_pose(v,NULL,0,NULL)==-1);
    if (event==SSA_RENDER_VIEW_CLEAR) {
        CHECK(r->events==0 && arg==0);
        CHECK(ssa_raster_fill_row(r->pixels,sizeof(r->pixels),0,14400,0)==1);
    } else {
        int32_t sine,cosine;
        /* This is an observation boundary for the unfinished world renderer,
         * not a fake successful scene draw. Only camera inputs are verified. */
        CHECK(event==SSA_RENDER_VIEW_WORLD && r->events==1 && arg==0);
        ssa_sincos(v->math,(uint16_t)(0u-v->camera.heading),&sine,&cosine);
        CHECK(v->camera.sine==sine && v->camera.cosine==cosine);
    }
    ++r->events;
    return 0;
}
int main(int argc,char **argv)
{
    PcAssets assets;unsigned level,i;
    CHECK(argc==2);pc_assets(&assets,argv[1]);
    for (level=0;level<16;++level) {
        PcCase pc;Fixture fixture;ProjectileCase projectile;RenderCase r={0};
        SsaPlayerRuntime *player;SsaPlayerSpawn *state;
        pc_start(&pc,&fixture,&projectile,&assets,level);
        player=&fixture.player;state=player->player;
        r.view.math=&assets.math;r.ops.event=render_event;r.ops.context=&r;
        for (i=0;i<128;++i) {
            static const int movement_keys[]={'W','A','S','D'};
            SsaPlayerRuntime saved;SsaRenderCamera before;int sector;
            if (i==16 || i==112) {pc_key(&pc,'H',1,0);pc_key(&pc,'H',0,0);}
            if (i%48==0) pc_key(&pc,' ',1,0);
            if (i%48==3) pc_key(&pc,' ',0,0);
            if (i%32==0) {
                if(i) pc_key(&pc,movement_keys[i/32-1],0,0);
                pc_key(&pc,movement_keys[i/32],1,0);
            }
            CHECK(ssa_pc_input_runtime_relative(&pc.runtime,(i&1)?1:-1,0)==1);
            pc_tick(&pc);
            saved=*player;r.view.players[0].flags=state->motion.movement.flags;
            r.view.players[0].keys=pc.game.runtime.input.entities.players[0].keys;
            r.view.ticks=pc.game.game.ticks;
            before=r.view.camera;
            CHECK(ssa_render_view_pose(&r.view,&player->pose,state->motion.movement.heading,NULL)==-1);
            CHECK(!memcmp(&r.view.camera,&before,sizeof(before)));++render_guards;
            CHECK(ssa_render_view_pose(&r.view,&player->pose,state->motion.movement.heading,&fixture.map)==0);
            sector=ssa_map_sector(&fixture.map,player->pose.x,player->pose.y);
            CHECK(r.view.camera.x==player->pose.x && r.view.camera.y==player->pose.y && r.view.camera.z==player->pose.z);
            CHECK(r.view.camera.heading==state->motion.movement.heading && r.view.camera.sector==sector);
            memset(r.pixels,0xa5,sizeof(r.pixels));r.events=0;
            CHECK(ssa_render_view_step(&r.view,&r.ops)==0 && r.events==2 && !r.view.updating);
            CHECK(!memcmp(player,&saved,sizeof(saved)));
            {
                static const uint8_t cleared[14400]={0};
                CHECK(!memcmp(r.pixels,cleared,sizeof(r.pixels)));
            }
            ++render_frames;
            if (state->entity.body.z>(int32_t)state->entity.body.floor*65536) ++render_airborne;
            if (player->pose.third_person) ++render_third;
        }
        pc_finish(&pc);
    }
    CHECK(render_frames==2048 && render_airborne && render_third);
    free(assets.rom);
    printf("{\"assertions\":%u,\"pc_game_frames\":%u,\"render_camera_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"guards\":%u}\n",checks,pc_frames,render_frames,render_airborne,render_third,render_guards);
    return 0;
}
