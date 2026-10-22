/* Isolated wall-clock rendering benchmark; no campaign progression or audio.
 * argv: ROM, frames per case, width, height, optional "software", optional Vulkan effect mask0..15, optional "stationary" camera. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "vulkan_renderer.h"
#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "godray_fixture.h"
static double now(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart/f.QuadPart;}
static uint64_t hash_bytes(const void *data,size_t size){
    const uint8_t *p=data;uint64_t hash=UINT64_C(14695981039346656037);
    while(size--){hash^=*p++;hash*=UINT64_C(1099511628211);}return hash;
}
int main(int argc,char **argv)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};SsaVulkanRenderer *v=NULL;
    unsigned count=argc>2?(unsigned)atoi(argv[2]):30,w=argc>3?(unsigned)atoi(argv[3]):1920,h=argc>4?(unsigned)atoi(argv[4]):1080;
    unsigned effects=argc>6?(unsigned)atoi(argv[6]):0;
    int stationary=argc>7 && !strcmp(argv[7],"stationary");
    int software=argc>5 && !strcmp(argv[5],"software");uint8_t *rom=malloc(0x800000);FILE *f;
    if(argc<2 || !count || !g || !rom)return 1;
    f=fopen(argv[1],"rb");if(!f || fread(rom,1,0x800000,f)!=0x800000)return 1;fclose(f);
    if(!software){v=ssa_vulkan_open();if(!v)return 1;r.hardware=ssa_vulkan_backend(v);}
    if(ssa_pc_renderer_resize(&r,w,h)!=1)return 1;
    printf("{\"device\":\"%s\",\"width\":%u,\"height\":%u,\"frames\":%u,\"cases\":[",software?"software":ssa_vulkan_device(v),w,h,count);
    for(unsigned c=0;c<8;++c){double start,elapsed;SsaVulkanTiming a,b;
        SsaActorSource saved_sources[100];SsaEntity crowd[64];unsigned saved_count;uint16_t heading;
        if(c==0 || c==2){if(c)ssa_native_game_free(g);
            if(ssa_native_game_init(g,rom,0x800000,c>=2?3:0)!=1 || ssa_native_game_step(g)!=1)return 1;}
        if(c>=2){g->pc_camera.x=-2048*65536;g->pc_camera.y=768*65536;g->pc_camera.z=224*65536;g->pc_camera.sector=9;}
        g->vulkan_features=effects;
        g->render_pitch=c>=2?-4096:2048;g->surface_decals_enabled=c!=0;g->water_reflections=c==3 || c==4;g->sprite_shadows_enabled=c==4 || c==7;g->godrays_enabled=c==5;
        if(c==5){if(!godray_fixture_camera(g))return 1;g->render_pitch=0;}
        heading=g->pc_camera.heading;
        saved_count=g->spawn.pool.active_count;memcpy(saved_sources,g->sources,sizeof saved_sources);
        if(c>=6){SsaActorSource actor={0};unsigned i;int32_t floor;
            for(i=0;i<saved_count;++i)if(g->sources[i].entity && g->sources[i].entity->type>=0xcd && g->sources[i].entity->type<=0xd8){actor=g->sources[i];break;}
            if(!actor.entity)return 1;
            floor=fixture_s16(g->map.data+g->map.sectors+g->pc_camera.sector*20)*65536;
            for(i=0;i<64;++i){crowd[i]=*actor.entity;crowd[i].body.x=g->pc_camera.x+((int)(i%8)-4)*40*65536;
                crowd[i].body.y=g->pc_camera.y+(128+(int)(i/8)*40)*65536;crowd[i].body.z=floor;
                crowd[i].sector=g->pc_camera.sector;g->sources[i]=actor;g->sources[i].entity=&crowd[i];g->sources[i].flags=0;}
            g->spawn.pool.active_count=64;}

        for(unsigned i=0;i<3;++i)if(ssa_pc_renderer_draw(&r,g)!=1)return 1;
        a=ssa_vulkan_timing(v);start=now();
        for(unsigned i=0;i<count;++i){g->pc_camera.heading=(uint16_t)(stationary?heading:(c==5?heading:0)+i*17);if(ssa_pc_renderer_draw(&r,g)!=1)return 1;}
        elapsed=now()-start;b=ssa_vulkan_timing(v);
        printf("%s{\"scene\":\"%s\",\"decals\":%u,\"reflections\":%u,\"shadows\":%u,\"godrays\":%u,\"ms\":%.3f,\"fps\":%.2f,\"passes\":%.2f,\"prepare_ms\":%.3f,\"wait_ms\":%.3f,\"readback_ms\":%.3f,\"effects_ms\":%.3f,\"pixel_hash\":\"%016llx\",\"depth_hash\":\"%016llx\",\"water_hash\":\"%016llx\",\"actor_pixels\":%llu,\"wall_pixels\":%llu}",c?",":"",c>=6?"crowded":c==5?"godrays":c==4?"pool-shadows":c>=2?"pool":"start",g->surface_decals_enabled,g->water_reflections,g->sprite_shadows_enabled,g->godrays_enabled,elapsed*1000/count,count/elapsed,(double)(b.passes-a.passes)/count,(b.prepare-a.prepare)*1000/count,(b.wait-a.wait)*1000/count,(b.readback-a.readback)*1000/count,(b.effects-a.effects)*1000/count,
            (unsigned long long)hash_bytes(r.pixels,(size_t)w*h*sizeof(*r.pixels)),
            (unsigned long long)hash_bytes(r.depth,(size_t)w*h*sizeof(*r.depth)),
            (unsigned long long)hash_bytes(r.water.mask,r.water.mask?(size_t)w*h*sizeof(*r.water.mask):0),
            (unsigned long long)r.actor_pixels,(unsigned long long)r.wall_pixels);fflush(stdout);
        memcpy(g->sources,saved_sources,sizeof saved_sources);g->spawn.pool.active_count=saved_count;
    }
    puts("]}");ssa_pc_renderer_free(&r);ssa_native_game_free(g);ssa_vulkan_close(v);free(g);free(rom);return 0;
}
