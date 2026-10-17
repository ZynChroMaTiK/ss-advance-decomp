/* Actual map construction, movement, animation, geometry and actor pixels.
 * This fixture deliberately stops before gameplay AI/combat and OS delivery. */
#include "ssa/actor_scene.h"
#include "ssa/actor_raster.h"
#include "ssa/map_actors.h"
#include "ssa/player_spawn.h"
#include "ssa/player_frame.h"
#include "ssa/pc_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

static unsigned checks,guards,frames,airborne,chase,body_frames,commands,draws;
static unsigned moved,changed_pixels,missing_failures,prefix_failures,owner_checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"actor scene line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define GUARD(x) do {++guards;CHECK(x);}while(0)
typedef struct Assets {
    SsaMathTables math;
    SsaVisibilityTables visibility;
    SsaWallRenderTables walls;
    SsaTextureAssets textures;
    SsaSkyAssets sky;
    SsaSpawnDefinitions definitions;
    SsaActorAnimations animations;
    SsaActorAssets actors;
} Assets;
typedef struct Fixture {
    Assets *assets;
    SsaMapAssets map;
    SsaMapTextureBindings bindings;
    SsaWorld world;
    SsaSlide slide;
    SsaMapCollision view_collision;
    SsaMapActors actors;
    SsaPlayerSpawn players[4];
    SsaActorAnimation player_animation;
    SsaJump jump;
    SsaCamera eye;
    SsaCameraFrame camera_frame;
    SsaView view;
    SsaViewPose pose;
    SsaRenderView render;
    SsaWorldScene geometry;
    SsaActorScene scene;
    SsaPcInput input;
    SsaActorSource sources[100];
    unsigned walking,frame;
} Fixture;
static int32_t signed_bits(uint32_t n)
{return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static void assets_load(Assets *a,const uint8_t *rom,size_t size)
{
    CHECK(ssa_math_tables_load(&a->math,rom,size));
    CHECK(ssa_visibility_tables_load(&a->visibility,rom,size)==1);
    CHECK(ssa_wall_render_tables_load(&a->walls,rom,size)==1);
    CHECK(ssa_texture_assets_load(&a->textures,rom,size)==1);
    CHECK(ssa_sky_assets_load(&a->sky,rom,size)==1);
    CHECK(ssa_spawn_definitions_load(&a->definitions,rom,size));
    CHECK(ssa_actor_animations_load(&a->animations,rom,size)==1);
    CHECK(ssa_actor_assets_load(&a->actors,rom,size)==1);
}
static void start(Fixture *f,Assets *a,const uint8_t *rom,size_t size,unsigned level)
{
    SsaSpawnPlan player_plan={0};int family=ssa_texture_assets_family(level);
    memset(f,0,sizeof(*f));f->assets=a;CHECK(family>=0);
    CHECK(ssa_map_assets_load(&f->map,rom,size,level));
    CHECK(ssa_map_texture_bindings_init(&f->bindings,&f->map,&a->textures,(unsigned)family)==1);
    CHECK(ssa_world_init(&f->world,&f->map,100));
    CHECK(ssa_slide_init(&f->slide,&f->map));
    CHECK(ssa_map_collision_init(&f->view_collision,&f->map));
    CHECK(ssa_map_actors_init(&f->actors,&f->world,&a->definitions,&a->animations,level)==1);
    CHECK(ssa_map_actors_step(&f->actors,0)==1);
    player_plan.map=&f->map;
    CHECK(ssa_players_spawn(f->players,&f->world,&player_plan,1,0));
    CHECK(f->players[0].active && (f->players[0].entity.body.flags&0x80));
    CHECK(f->world.entity_count==f->actors.plan.object_count+1);
    CHECK(ssa_actor_animation_set(&f->player_animation,f->players[0].animation,
        f->players[0].animation_ticks)==1);
    CHECK(ssa_actor_animation_step(&f->player_animation,&a->animations,
        f->players[0].entity.body.flags,0)==1);
    CHECK(ssa_world_scene_init(&f->geometry,&f->bindings,&a->math,&a->visibility,
        &a->walls,&a->sky,level)==1);
    CHECK(ssa_actor_scene_init(&f->scene,&f->geometry,&f->world,&a->actors,&a->math)==1);
    CHECK(ssa_pc_input_init(&f->input,64)==1);
    ssa_jump_init(&f->jump);ssa_view_init(&f->view);
    f->eye.eye_z=signed_bits((uint32_t)f->players[0].entity.body.z+
        (uint32_t)f->players[0].vertical.eye_offset);
}
static size_t sources(Fixture *f)
{
    unsigned i;size_t n=f->actors.plan.object_count;SsaPlayerSpawn *p=&f->players[0];
    for(i=0;i<n;++i) {
        const SsaSpawnObject *o=ssa_map_actor_object(&f->actors,i);
        const SsaActorAnimation *a=ssa_map_actor_animation(&f->actors,i);
        CHECK(o && a);
        f->sources[i]=(SsaActorSource){&o->entity,a->animation,a->frame,
            o->entity.body.flags,o->facing,o->entity.player_index};
    }
    f->sources[n]=(SsaActorSource){&p->entity,f->player_animation.animation,
        f->player_animation.frame,p->entity.body.flags,p->motion.facing,p->entity.player_index};
    if(f->pose.third_person)f->sources[n].flags&=UINT16_C(0xff7f);
    CHECK((p->entity.body.flags&0x80)!=0);
    CHECK(((f->sources[n].flags&0x80)==0)==!!f->pose.third_person);
    return n+1;
}
static void geometry(Fixture *f)
{
    CHECK(ssa_world_scene_clear(&f->geometry,0)==1);
    CHECK(ssa_world_scene_draw(&f->geometry,&f->render.camera,(uint8_t)f->frame,
        f->players[0].motion.movement.heading)==1);
}
static void draw(Fixture *f,int primary_frame)
{
    uint8_t before[14400],body[14400]={0};unsigned i,visible=0;
    SsaPlayerSpawn *p=&f->players[0];SsaBody canonical=p->entity.body;
    SsaActorQueue *q=&f->scene.queue;size_t n=sources(f);
    geometry(f);memcpy(before,ssa_world_scene_pixels(&f->geometry),sizeof(before));
    CHECK(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,n)==1);
    ++draws;
    CHECK(!f->scene.updating && !f->scene.projector.camera && !q->updating);
    CHECK(memcmp(&canonical,&p->entity.body,sizeof(canonical))==0);
    CHECK(q->draw_count<=32 && q->projection_count<=32);
    commands+=q->draw_count;
    for(i=0;i<14400;++i)
        if(before[i]!=ssa_world_scene_pixels(&f->geometry)[i])++changed_pixels;
    for(i=0;i<q->draw_count;++i) {
        const SsaActorRecord *r=&q->actors[q->draw_order[i]];
        const SsaActorProjection *projection=&q->projections[r->projection];
        if(projection->animation==f->player_animation.animation ||
           projection->animation==f->player_animation.animation+5) {
            CHECK(f->pose.third_person);
            CHECK(ssa_actor_raster_draw(&f->assets->actors,projection,q->clip_words+r->clip_offset,
                SSA_ACTOR_CLIP_WORDS-r->clip_offset,body,sizeof(body))==1);
        }
    }
    for(i=0;i<14400;++i)if(body[i])++visible;
    if(visible && primary_frame)++body_frames;
    if(!f->pose.third_person)CHECK(!visible);
}
static void key(Fixture *f,int keycode,int down)
{CHECK(ssa_pc_input_key(&f->input,keycode,down,0)==1);}
static void tick(Fixture *f,unsigned index)
{
    SsaPlayerSpawn *p=&f->players[0];const SsaEntity *players[1]={&p->entity};
    SsaMotionFrame frame={players,1,1,0,0};SsaMotionOptions options;
    SsaPcInputSample sample;int32_t x=p->entity.body.x,y=p->entity.body.y;
    if(index==0)key(f,'W',1);
    if(index==24)key(f,'W',0);
    if(index==32)key(f,'D',1);
    if(index==48)key(f,'D',0);
    if(index==184)key(f,'S',1);
    if(index==192){key(f,'S',0);key(f,'A',1);}
    if(index==200)key(f,'A',0);
    if(index==4 || index==124){key(f,' ',1);key(f,' ',0);}
    if(index==80 || index==180){key(f,'H',1);key(f,'H',0);}
    if(index>=60 && index<76)CHECK(ssa_pc_input_relative(&f->input,1,0)==1);
    CHECK(ssa_pc_input_sample(&f->input,&sample)==1);
    p->motion.movement.keys=sample.keys;
    p->motion.movement.heading=(uint16_t)((uint32_t)p->motion.movement.heading+(uint32_t)sample.yaw_delta);
    if(sample.view_toggle){ssa_view_key(&f->view,'H',1,0,1);ssa_view_key(&f->view,'H',0,0,1);}
    options.jump=&f->jump;options.jump_down=sample.jump_down;
    options.controls_enabled=sample.enabled;options.retain_heading=1;
    CHECK(ssa_player_motion_world_step(&p->motion,&p->entity,&p->vertical,&f->world,
        &f->slide,&f->assets->math,&frame,&options)==0);
    if(!f->walking && ssa_player_starts_moving(&p->motion.movement)) {
        f->walking=1;CHECK(ssa_actor_animation_set(&f->player_animation,0x66,4)>=0);
    } else if(f->walking && ssa_player_stops_moving(&p->motion.movement)) {
        f->walking=0;CHECK(ssa_actor_animation_set(&f->player_animation,0x65,4)>=0);
    }
    CHECK(ssa_map_actors_step(&f->actors,1)==1);
    CHECK(ssa_actor_animation_step(&f->player_animation,&f->assets->animations,p->entity.body.flags,1)==1);
    f->camera_frame.counter=(uint16_t)index;f->camera_frame.is_local=1;f->camera_frame.footsteps_muted=1;
    (void)ssa_camera_step(&p->entity.body,&p->vertical,&f->eye,&f->camera_frame,&f->assets->math);
    CHECK(ssa_view_pose(&f->view,&p->entity.body,&f->eye,p->motion.movement.heading,
        &f->assets->math,&f->view_collision,&f->pose));
    CHECK(ssa_render_view_pose(&f->render,&f->pose,p->motion.movement.heading,&f->map)==0);
    ssa_sincos(&f->assets->math,(uint16_t)-f->render.camera.heading,
        &f->render.camera.sine,&f->render.camera.cosine);
    if(x!=p->entity.body.x || y!=p->entity.body.y)++moved;
    if(p->entity.body.z>(int32_t)p->entity.body.floor*65536)++airborne;
    if(f->pose.third_person)++chase;
    f->frame=index;draw(f,1);++frames;
}
static void bad_calls(Fixture *f)
{
    SsaActorScene empty={0};SsaActorAssets *a=&f->assets->actors;
    const SsaMathTables *math=f->scene.projector.math;
    size_t n=sources(f);unsigned *locks[]={&f->scene.updating,&f->scene.queue.updating,
        &f->scene.projector.updating,&f->geometry.updating,&f->geometry.visibility.updating,
        &f->geometry.wall_renderer.updating,&f->geometry.plane_renderer.updating,&f->world.query_active};
    unsigned i;uint8_t pixels[14400];SsaActorSource saved,duplicates[2];
    GUARD(ssa_actor_scene_init(NULL,&f->geometry,&f->world,a,math)==-1);
    GUARD(ssa_actor_scene_init(&empty,NULL,&f->world,a,math)==-1);
    GUARD(ssa_actor_scene_init(&empty,&f->geometry,NULL,a,math)==-1);
    GUARD(ssa_actor_scene_init(&empty,&f->geometry,&f->world,NULL,math)==-1);
    GUARD(ssa_actor_scene_init(&empty,&f->geometry,&f->world,a,NULL)==-1);
    GUARD(ssa_actor_scene_init(&f->scene,&f->geometry,&f->world,a,math)==-1);
    GUARD(ssa_actor_scene_free(&empty)==-1);
    GUARD(ssa_actor_scene_free(NULL)==-1);
    GUARD(ssa_actor_scene_draw(NULL,&f->render.camera,0,f->sources,n)==-1);
    GUARD(ssa_actor_scene_draw(&f->scene,NULL,0,f->sources,n)==-1);
    GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,NULL,n)==-1);
    GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,101)==-1);
    for(i=0;i<sizeof(locks)/sizeof(locks[0]);++i) {
        *locks[i]=1;
        GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,n)==-1);
        if(i<3)GUARD(ssa_actor_scene_free(&f->scene)==-1);
        CHECK(!f->scene.projector.camera);*locks[i]=0;
    }
    for(i=3;i<sizeof(locks)/sizeof(locks[0]);++i) {
        *locks[i]=1;
        GUARD(ssa_actor_scene_init(&empty,&f->geometry,&f->world,a,math)==-1);
        GUARD(ssa_actor_scene_free(&f->scene)==-1);
        CHECK(f->scene.loaded && !empty.loaded);*locks[i]=0;
    }
    f->scene.projector.math=NULL;
    GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,n)==-1);
    f->scene.projector.math=math;
    a->loaded=0;GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,n)==-1);a->loaded=1;
    geometry(f);memcpy(pixels,ssa_world_scene_pixels(&f->geometry),sizeof(pixels));
    saved=f->sources[0];f->sources[0].entity=NULL;
    GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,f->sources,n)==-1);
    CHECK(!f->scene.updating && !f->scene.projector.camera);
    CHECK(!memcmp(pixels,ssa_world_scene_pixels(&f->geometry),sizeof(pixels)));
    f->sources[0]=saved;
    geometry(f);duplicates[0]=f->sources[0];duplicates[1]=duplicates[0];
    GUARD(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,duplicates,2)==-1);
    CHECK(!f->scene.updating && !f->scene.projector.camera);
    draw(f,0);
}
static void missing_sources(Fixture *f)
{
    SsaActorSource subset[100];size_t n=sources(f),omit,i,j;
    uint8_t before[14400];SsaBody body=f->players[0].entity.body;
    for(omit=0;omit<n;++omit) {
        for(i=0,j=0;i<n;++i)if(i!=omit)subset[j++]=f->sources[i];
        geometry(f);memcpy(before,ssa_world_scene_pixels(&f->geometry),sizeof(before));
        i=(size_t)(ssa_actor_scene_draw(&f->scene,&f->render.camera,0,subset,n-1)==-1);
        if(i) {
            ++missing_failures;
            if(f->scene.queue.projection_count)++prefix_failures;
            /* Collection fails before raster stage: queue prefix survives,
             * framebuffer and canonical player remain untouched. */
            CHECK(!memcmp(before,ssa_world_scene_pixels(&f->geometry),sizeof(before)));
        }
        CHECK(!f->scene.updating && !f->scene.projector.camera);
        CHECK(!memcmp(&body,&f->players[0].entity.body,sizeof(body)));
    }
    draw(f,0);
}
static void finish(Fixture *f)
{
    size_t count=f->world.entity_count;SsaMapActors empty={0};
    GUARD(ssa_map_actors_init(&empty,&f->world,&f->assets->definitions,
        &f->assets->animations,0)==-1);
    f->world.query_active=1;
    GUARD(ssa_actor_scene_free(&f->scene)==-1);
    GUARD(ssa_map_actors_free(&f->actors)==-1);
    GUARD(ssa_map_actors_step(&f->actors,1)==-1);f->world.query_active=0;
    f->actors.updating=1;GUARD(ssa_map_actors_free(&f->actors)==-1);f->actors.updating=0;
    GUARD(ssa_map_actors_step(&f->actors,256)==-1);
    CHECK(ssa_actor_scene_free(&f->scene)==1);
    CHECK(!f->scene.loaded && !f->scene.geometry && !f->scene.world);
    CHECK(f->geometry.loaded && f->world.entity_count==count && f->assets->actors.loaded);++owner_checks;
    CHECK(ssa_world_scene_free(&f->geometry)==1);
    CHECK(f->world.entity_count==count && f->players[0].entity.owner==&f->world);++owner_checks;
    CHECK(ssa_map_actors_free(&f->actors)==1);
    CHECK(f->world.entity_count==1 && f->players[0].entity.owner==&f->world);++owner_checks;
    CHECK(!f->actors.loaded && !ssa_map_actor_object(&f->actors,0));
    CHECK(ssa_world_remove(&f->world,&f->players[0].entity));
    CHECK(!f->world.entity_count && !f->players[0].entity.owner);++owner_checks;
    ssa_map_collision_free(&f->view_collision);ssa_slide_free(&f->slide);ssa_world_free(&f->world);
    ssa_map_texture_bindings_free(&f->bindings);ssa_map_assets_free(&f->map);
}
int main(int argc,char **argv)
{
    FILE *file;uint8_t *rom;Assets *a;Fixture *f;unsigned level,i;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file!=NULL);
    rom=malloc(8u*1024u*1024u);a=calloc(1,sizeof(*a));f=calloc(1,sizeof(*f));CHECK(rom && a && f);
    CHECK(fread(rom,1,8u*1024u*1024u,file)==8u*1024u*1024u);CHECK(!fclose(file));
    assets_load(a,rom,8u*1024u*1024u);
    for(level=0;level<16;++level) {
        unsigned oldbody=body_frames,oldair=airborne,oldmoved=moved,oldpixels=changed_pixels;
        start(f,a,rom,8u*1024u*1024u,level);
        for(i=0;i<240;++i){tick(f,i);if(i==0)bad_calls(f);if(i==100)missing_sources(f);}
        if(!(body_frames>oldbody && airborne>oldair && moved>oldmoved && changed_pixels>oldpixels))
            fprintf(stderr,"level %u: body %u airborne %u moved %u pixels %u\n",level,
                body_frames-oldbody,airborne-oldair,moved-oldmoved,changed_pixels-oldpixels);
        CHECK(body_frames>oldbody && airborne>oldair && moved>oldmoved && changed_pixels>oldpixels);
        finish(f);
    }
    CHECK(missing_failures>0 && prefix_failures>0 && frames==3840 && chase>0);
    ssa_actor_assets_free(&a->actors);ssa_texture_assets_free(&a->textures);
    free(f);free(a);free(rom);
    printf("{\"assertions\":%u,\"guards\":%u,\"maps\":16,\"frames\":%u,"
        "\"airborne_frames\":%u,\"third_person_frames\":%u,\"visible_sam_frames\":%u,"
        "\"moving_frames\":%u,\"scene_draws\":%u,\"actor_commands\":%u,\"actor_changed_pixels\":%u,"
        "\"missing_source_failures\":%u,\"preserved_queue_prefix_failures\":%u,"
        "\"ownership_checks\":%u}\n",checks,guards,frames,airborne,chase,body_frames,
        moved,draws,commands,changed_pixels,missing_failures,prefix_failures,owner_checks);
    return 0;
}
