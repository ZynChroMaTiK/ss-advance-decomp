#define SSA_PC_TEST_MAIN scene_pc_fixture_main
#include "test_pc_input.c"
#include "ssa/world_scene.h"
#include "ssa/sprite_assets.h"

static unsigned scene_frames,scene_airborne,scene_third,scene_guards;
static uint64_t scene_nonzero;
static void snapshot(SsaWorldScene *scene,const SsaSpriteAssets *palette,unsigned level,const char *folder)
{
    char path[1024];FILE *file;const uint8_t *pixels=ssa_world_scene_pixels(scene);
    CHECK(snprintf(path,sizeof(path),"%s/world-level-%02u.ppm",folder,level)>0);
    file=fopen(path,"wb");CHECK(file);CHECK(fprintf(file,"P6\n120 120\n255\n")>0);
    for(unsigned i=0;i<14400;++i) {
        unsigned p=palette->background_palette[scene->wall_renderer.bindings->family][pixels[i]];
        uint8_t rgb[3];rgb[0]=(uint8_t)(((p&31)<<3)|((p&31)>>2));
        rgb[1]=(uint8_t)((((p>>5)&31)<<3)|(((p>>5)&31)>>2));
        rgb[2]=(uint8_t)((((p>>10)&31)<<3)|(((p>>10)&31)>>2));
        CHECK(fwrite(rgb,1,3,file)==3);
    }
    CHECK(!fclose(file));
}
int main(int argc,char **argv)
{
    PcAssets a;SsaTextureAssets textures={0};SsaSkyAssets *sky;
    SsaVisibilityTables *vt;SsaWallRenderTables *wt;SsaWorldScene *scene;
    SsaSpriteAssets palettes={0};unsigned level,i;
    CHECK(argc==3);pc_assets(&a,argv[1]);
    sky=calloc(1,sizeof(*sky));vt=calloc(1,sizeof(*vt));wt=calloc(1,sizeof(*wt));scene=calloc(1,sizeof(*scene));
    CHECK(sky && vt && wt && scene);
    CHECK(ssa_texture_assets_load(&textures,a.rom,a.size)==1);
    CHECK(ssa_sky_assets_load(sky,a.rom,a.size)==1);
    CHECK(ssa_visibility_tables_load(vt,a.rom,a.size)==1);
    CHECK(ssa_wall_render_tables_load(wt,a.rom,a.size)==1);
    CHECK(ssa_sprite_assets_load(&palettes,a.rom,a.size)==1);
    for(level=0;level<16;++level) {
        PcCase pc;Fixture fixture;ProjectileCase projectile;SsaMapTextureBindings b={0};SsaRenderView view={0};
        pc_start(&pc,&fixture,&projectile,&a,level);view.math=&a.math;
        CHECK(ssa_map_texture_bindings_init(&b,&fixture.map,&textures,(unsigned)ssa_texture_assets_family(level))==1);
        CHECK(ssa_world_scene_init(scene,&b,&a.math,vt,wt,sky,level)==1);
        {
            SsaWorldScene *saved=malloc(sizeof(*saved));SsaRenderCamera camera={0};CHECK(saved);
            *saved=*scene;
            CHECK(ssa_world_scene_init(scene,&b,&a.math,vt,wt,sky,level)==-1);
            CHECK(ssa_world_scene_draw(scene,NULL,0,0)==-1);
            CHECK(!memcmp(scene,saved,sizeof(*scene)));scene_guards+=2;
            scene->updating=1;*saved=*scene;
            CHECK(ssa_world_scene_clear(scene,0)==-1);
            CHECK(ssa_world_scene_draw(scene,&camera,0,0)==-1);
            CHECK(ssa_world_scene_free(scene)==-1);
            CHECK(!memcmp(scene,saved,sizeof(*scene)));scene_guards+=3;scene->updating=0;
            for(unsigned guard=0;guard<3;++guard) {
                unsigned *flag=guard==0?&scene->visibility.updating:guard==1?&scene->wall_renderer.updating:&scene->plane_renderer.updating;
                *flag=1;*saved=*scene;
                CHECK(ssa_world_scene_clear(scene,0)==-1);
                CHECK(ssa_world_scene_draw(scene,&camera,0,0)==-1);
                CHECK(ssa_world_scene_free(scene)==-1);
                CHECK(!memcmp(scene,saved,sizeof(*scene)));scene_guards+=3;*flag=0;
            }
            free(saved);
        }
        for(i=0;i<128;++i) {
            static const int keys[]={'W','A','S','D'};unsigned nonzero=0;int status;
            if(i==16 || i==112) {pc_key(&pc,'H',1,0);pc_key(&pc,'H',0,0);}
            if(i%48==0)pc_key(&pc,' ',1,0);
            if(i%48==3)pc_key(&pc,' ',0,0);
            if(i%32==0) {if(i)pc_key(&pc,keys[i/32-1],0,0);pc_key(&pc,keys[i/32],1,0);}
            CHECK(ssa_pc_input_runtime_relative(&pc.runtime,(i&1)?1:-1,0)==1);pc_tick(&pc);
            CHECK(ssa_render_view_pose(&view,&fixture.player.pose,fixture.players[0].motion.movement.heading,&fixture.map)==0);
            ssa_sincos(&a.math,(uint16_t)(0u-view.camera.heading),&view.camera.sine,&view.camera.cosine);
            CHECK(ssa_world_scene_clear(scene,0)==1);
            status=ssa_world_scene_draw(scene,&view.camera,(uint8_t)pc.game.game.counter,fixture.players[0].motion.movement.heading);
            if(status!=1) fprintf(stderr,"scene failed level%u frame%u walls%u planes%u cursor%zu\n",level,i,scene->visibility.wall_count,scene->planes.count,scene->planes.opening_cursor);
            CHECK(status==1 && !scene->updating);
            for(unsigned p=0;p<14400;++p)nonzero+=ssa_world_scene_pixels(scene)[p]!=0;
            CHECK(nonzero);scene_nonzero+=nonzero;
            CHECK(scene->visibility.wall_count<=100 && scene->planes.count<=scene->planes.capacity);
            CHECK(scene->planes.opening_cursor<=sizeof(scene->openings));
            ++scene_frames;
            if(fixture.players[0].motion.z>(int32_t)fixture.players[0].entity.body.floor*65536)++scene_airborne;
            if(fixture.player.pose.third_person)++scene_third;
            if(i==0)snapshot(scene,&palettes,level,argv[2]);
        }
        CHECK(ssa_world_scene_free(scene)==1);ssa_map_texture_bindings_free(&b);pc_finish(&pc);
    }
    CHECK(scene_frames==2048 && scene_airborne && scene_third && scene_nonzero);
    CHECK(ssa_world_scene_pixels(scene)==NULL);CHECK(ssa_world_scene_clear(scene,0)==-1);++scene_guards;
    ssa_sprite_assets_free(&palettes);ssa_texture_assets_free(&textures);
    free(scene);free(wt);free(vt);free(sky);free(a.rom);
    printf("{\"assertions\":%u,\"native_world_frames\":%u,\"airborne\":%u,\"third_person\":%u,\"nonzero_pixels\":%llu,\"guards\":%u}\n",checks,scene_frames,scene_airborne,scene_third,(unsigned long long)scene_nonzero,scene_guards);
    return 0;
}
