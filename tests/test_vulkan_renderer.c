#include "vulkan_renderer.h"
#include "ssa/native_game.h"
#include "ssa/pc_scene.h"
#include "ssa/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
static unsigned checks,draws,sprite_cases;static unsigned long long matched,pixels;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Vulkan check line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i));}
static void snapshot(const SsaPcRenderer *r,const char *path){
    unsigned char h[54]={0};FILE *f=fopen(path,"wb");CHECK(f);h[0]='B';h[1]='M';put32(h+2,54+r->width*r->height*4);
    put32(h+10,54);put32(h+14,40);put32(h+18,r->width);put32(h+22,0u-r->height);h[26]=1;h[28]=32;
    CHECK(fwrite(h,1,54,f)==54 && fwrite(r->pixels,4,(size_t)r->width*r->height,f)==(size_t)r->width*r->height);CHECK(!fclose(f));
}
static void compare(SsaPcRenderer *gpu,SsaPcRenderer *cpu,SsaNativeGame *g)
{
    size_t n=(size_t)gpu->width*gpu->height,i,match=0;uint32_t frame=g->frame_game.counter;
    CHECK(ssa_pc_renderer_resize(cpu,gpu->width,gpu->height)==1);
    CHECK(ssa_pc_renderer_draw(gpu,g)==1);++draws;
    CHECK(ssa_pc_renderer_draw(cpu,g)==1);++draws;
    CHECK(g->frame_game.counter==frame);
    for(i=0;i<n;++i){CHECK(!(gpu->pixels[i]&0xff000000));match+=gpu->pixels[i]==cpu->pixels[i];}
    fprintf(stderr,"Vulkan/software %ux%u pitch%d: %.3f%% identical\n",gpu->width,gpu->height,g->render_pitch,100.0*match/n);
    /* Hardware edge coverage and float interpolation differ at polygon and
     * nearest-texel boundaries. Large differences catch missing/reversed scenes. */
    CHECK(match*100>=n*96);matched+=match;pixels+=n;
}
static void quad(const SsaPcHardware *hw,float left,float top,float right,float bottom,float z,const SsaPcMaterial *m)
{
    SsaPcVertex q[4]={{left,top,z,0,0},{right,top,z,m->width,0},
        {right,bottom,z,m->width,m->height},{left,bottom,z,0,m->height}};
    CHECK(hw->triangle(hw->context,q,m)==1);q[1]=q[2];q[2]=q[3];CHECK(hw->triangle(hw->context,q,m)==1);
}
static void primitives(SsaVulkanRenderer *v)
{
    SsaPcRenderer r={0};const SsaPcHardware *hw=ssa_vulkan_backend(v);uint32_t palette[256]={0};
    const SsaPcSample *samples;uint8_t texels[4]={2,0,0,2};
    SsaPcMaterial solid={0},sprite={0};
    SsaPcVertex tri[3]={{-4,-4,2,0,0},{4,-4,2,0,0},{0,4,2,0,0}};
    CHECK(ssa_pc_renderer_resize(&r,32,32)==1);memset(r.pixels,0,32*32*4);
    palette[1]=0xff0000;palette[2]=0x00ff00;solid.color=1;
    CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
    CHECK(hw->triangle(hw->context,tri,&solid)==1);
    /* A nearer transparent indexed quad, followed by a hidden far triangle. */
    sprite.pixels=texels;sprite.width=sprite.height=2;sprite.transparent=sprite.actor=1;
    {SsaPcVertex q[4]={{-1,1,1,0,0},{1,1,1,2,0},{1,-1,1,2,2},{-1,-1,1,0,2}};
        CHECK(hw->triangle(hw->context,q,&sprite)==1);q[1]=q[2];q[2]=q[3];CHECK(hw->triangle(hw->context,q,&sprite)==1);}
    for(unsigned i=0;i<3;++i){tri[i].x*=2;tri[i].y*=2;tri[i].z=4;}solid.color=0;
    CHECK(hw->triangle(hw->context,tri,&solid)==1);CHECK(hw->end(hw->context,&r,&samples)==1);
    CHECK(r.pixels[8*32+8]==0x00ff00 && samples[8*32+8].z==1);
    CHECK(r.pixels[8*32+20]==0xff0000 && samples[8*32+20].z==2);
    /* A reflected row interval retains full viewport projection and leaves
     * caller-owned rows outside it untouched (including odd row boundaries). */
    for(unsigned p=0;p<32*32;++p)r.pixels[p]=0x123456;
    CHECK(hw->begin(hw->context,&r,palette,16,5,5)==-1);
    CHECK(hw->begin(hw->context,&r,palette,16,5,33)==-1);
    CHECK(hw->begin(hw->context,&r,palette,16,5,17)==1);solid.color=1;
    CHECK(hw->triangle(hw->context,tri,&solid)==1 && hw->end(hw->context,&r,&samples)==1);
    CHECK(r.pixels[10*32+16]==0xff0000 && samples[10*32+16].z==4);
    for(unsigned y=0;y<32;++y)if(y<5 || y>=17)for(unsigned x=0;x<32;++x)CHECK(r.pixels[y*32+x]==0x123456);
    /* A sprite may win the software-sized near tie against a wall, but not
     * show through a genuinely nearer wall. Geometry submitted after sprites
     * is still resolved in the world pass first. Aux depth stays unbiased. */
    {uint8_t texel=2;SsaPcMaterial flat={0},billboard={0};palette[3]=0x0000ff;
        flat.color=1;billboard.pixels=&texel;billboard.width=billboard.height=1;
        billboard.actor=billboard.transparent=1;
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
        quad(hw,-4,4,4,-4,2,&flat);
        quad(hw,-1.8f,1.8f,0,-1.8f,2.0005f,&billboard);
        quad(hw,0,1.8f,1.8f,-1.8f,2.01f,&billboard);
        flat.color=3;quad(hw,-0.3f,0.3f,0.3f,-0.3f,1,&flat);
        CHECK(hw->end(hw->context,&r,&samples)==1);
        CHECK(r.pixels[16*32+8]==0x00ff00 && samples[16*32+8].z>2);
        CHECK(r.pixels[16*32+24]==0xff0000 && samples[16*32+24].z==2);
        CHECK(r.pixels[16*32+16]==0x0000ff && samples[16*32+16].z==1);
    }
    ssa_pc_renderer_free(&r);
}
static void sprite_order(SsaPcRenderer *r,SsaNativeGame *g)
{
    const unsigned ids[3]={1,2,6};SsaActorResource resources[3];
    SsaActorSource saved[100];int32_t offsets[100];unsigned count=g->spawn.pool.active_count;
    unsigned heading=g->pc_camera.heading,fov=g->fov_offset;int pitch=g->render_pitch;
    uint8_t red[32*32],green[32*32];SsaActorImage images[2]={{0,-16,-16,32,32,red},{0,-16,-16,32,32,green}};
    uint32_t *world,*a_image,*b_image,colors[2];size_t n=321*213,p;unsigned view,mode,i;
    CHECK(ssa_pc_renderer_resize(r,321,213)==1);
    world=malloc(n*4);a_image=malloc(n*4);b_image=malloc(n*4);CHECK(world && a_image && b_image);
    memcpy(saved,g->sources,sizeof(saved));memcpy(offsets,g->source_z_offsets,sizeof(offsets));
    memset(g->source_z_offsets,0,sizeof(offsets));
    for(i=0;i<2;++i){unsigned c=g->palette_effect.colors[i+1],red=c&31,green=(c>>5)&31,blue=(c>>10)&31;
        colors[i]=((red*8+red/4)<<16)|((green*8+green/4)<<8)|(blue*8+blue/4);}
    /* Use actual actor projection/near clipping with simple overlapping art,
     * including transparent gaps that must reveal the preceding sprite. */
    for(p=0;p<32*32;++p){red[p]=1;green[p]=(p/32)%4?2:0;}
    for(i=0;i<3;++i){resources[i]=g->actor_assets.resources[ids[i]];
        g->actor_assets.resources[ids[i]]=(SsaActorResource){0,0,1,1,8,1,&images[i?1:0]};}
    for(view=0;view<3;++view)for(mode=0;mode<6;++mode){
        SsaEntity a={0},b={0};SsaActorSource sa={&a,1,0,0,0,0},sb={&b,2,0,0,0,0};
        int32_t sine,cosine;unsigned visible=0,b_wins=mode!=1 && mode!=2;
        double da=mode==3?72:64,db=mode==2?72:mode==4?64.0002:64;
        g->pc_camera.heading=(uint16_t)(heading+view*137);g->render_pitch=view==0?0:view==1?4096:-4096;
        g->fov_offset=view==0?0:view==1?10:60;
        ssa_sincos(&g->math,g->pc_camera.heading,&sine,&cosine);
        a.body.x=g->pc_camera.x+(int32_t)(sine*da);a.body.y=g->pc_camera.y+(int32_t)(cosine*da);
        b.body.x=g->pc_camera.x+(int32_t)(sine*db);b.body.y=g->pc_camera.y+(int32_t)(cosine*db);
        a.body.z=g->pc_camera.z;b.body.z=g->pc_camera.z+(mode==5?0:8*65536);
        g->spawn.pool.active_count=0;CHECK(ssa_pc_scene_draw(r,g)==1);memcpy(world,r->pixels,n*4);
        g->spawn.pool.active_count=1;g->sources[0]=sa;CHECK(ssa_pc_scene_draw(r,g)==1);memcpy(a_image,r->pixels,n*4);
        g->sources[0]=sb;CHECK(ssa_pc_scene_draw(r,g)==1);memcpy(b_image,r->pixels,n*4);
        g->spawn.pool.active_count=2;g->sources[0]=mode==1?sb:sa;g->sources[1]=mode==1?sa:sb;
        if(mode==5){g->spawn.pool.active_count=1;g->sources[0]=sa;g->sources[0].flags=16;}
        CHECK(ssa_pc_scene_draw(r,g)==1);
        for(p=0;p<n;++p){uint32_t near=b_wins?b_image[p]:a_image[p],far=b_wins?a_image[p]:b_image[p];
            /* Same-colored world texels cannot identify an opaque sprite by
             * color difference; exclude those ambiguous mask pixels. */
            if(world[p]==colors[0] || world[p]==colors[1])continue;
            if(near!=world[p] || far!=world[p]){
                if(r->pixels[p]!=(near!=world[p]?near:far))fprintf(stderr,"sprite view%u mode%u pixel%zu world%x a%x b%x got%x\n",view,mode,p,world[p],a_image[p],b_image[p],r->pixels[p]);
                CHECK(r->pixels[p]==(near!=world[p]?near:far));++visible;}}
        CHECK(visible>100);++sprite_cases;
    }
    for(i=0;i<3;++i)g->actor_assets.resources[ids[i]]=resources[i];
    memcpy(g->sources,saved,sizeof(saved));memcpy(g->source_z_offsets,offsets,sizeof(offsets));
    g->spawn.pool.active_count=count;g->pc_camera.heading=(uint16_t)heading;g->render_pitch=pitch;g->fov_offset=fov;
    free(world);free(a_image);free(b_image);
}
int main(int argc,char **argv)
{
    SsaVulkanRenderer *v=ssa_vulkan_open();SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer gpu={0},cpu={0};
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned i;static const unsigned sizes[][2]={{1,1},{321,213},{960,640},{1280,720},{240,160}};
    CHECK(v && g && rom && argc>=2);primitives(v);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    {SsaMenu *m=calloc(1,sizeof(*m));CHECK(m && ssa_menu_init(m,rom,0x800000,1)==1);
        CHECK(m->settings.renderer==SSA_RENDER_VULKAN);m->settings.renderer=SSA_RENDER_SOFTWARE;
        m->page=m->draw_page=SSA_MENU_GRAPHICS;m->selection=1;m->fading=m->fade_volume=0;m->vulkan_available=1;
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.renderer==SSA_RENDER_VULKAN);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_SOFTWARE);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_GBA);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.renderer==SSA_RENDER_VULKAN);
        m->selection=2;CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.aspect_ratio==SSA_ASPECT_GBA);
        m->page=SSA_MENU_SETTINGS;m->selection=3;CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.fov_offset==11);
        m->page=SSA_MENU_GRAPHICS;
        m->selection=1;m->vulkan_available=0;m->settings.renderer=SSA_RENDER_GBA;
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.renderer==SSA_RENDER_SOFTWARE);
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.renderer==SSA_RENDER_GBA);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_SOFTWARE);
        for(i=0;i<2;++i){m->vulkan_available=i;m->page=m->draw_page=SSA_MENU_CLEAR_DATA;m->selection=0;
            CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1);
            CHECK(m->settings.renderer==(i?SSA_RENDER_VULKAN:SSA_RENDER_SOFTWARE));}
        free(m);}
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_native_game_step(g)==1);gpu.hardware=ssa_vulkan_backend(v);
    sprite_order(&gpu,g);
    for(i=0;i<5;++i){CHECK(ssa_pc_renderer_resize(&gpu,sizes[i][0],sizes[i][1])==1);g->render_pitch=i%2?4096:-2048;compare(&gpu,&cpu,g);}
    CHECK(ssa_pc_renderer_resize(&gpu,960,640)==1);g->render_pitch=4096;g->surface_decals_enabled=1;g->hud_style=SSA_HUD_CLASSIC;
    compare(&gpu,&cpu,g);if(argc>2)snapshot(&gpu,argv[2]);
    /* Paired scene-only render at level aim also exercises the native raycast
     * reference versus GPU polygon coverage. */
    g->render_pitch=0;compare(&gpu,&cpu,g);
    ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
    for(i=0;i<30;++i)CHECK(ssa_native_game_step(g)==1);
    ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
    ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
    CHECK(ssa_native_game_step(g)==1 && g->player.pose.third_person);g->render_pitch=-2048;
    compare(&gpu,&cpu,g);CHECK(gpu.actor_pixels>0);
    for(i=0;i<=60;i+=60){g->fov_offset=i;compare(&gpu,&cpu,g);}
    ssa_native_game_free(g);CHECK(ssa_native_game_init(g,rom,0x800000,3)==1 && ssa_native_game_step(g)==1);
    g->pc_camera.x=-2048*65536;g->pc_camera.y=768*65536;g->pc_camera.z=224*65536;
    g->pc_camera.sector=9;g->pc_camera.heading=0;g->surface_decals_enabled=1;g->water_reflections=1;g->render_pitch=-4096;
    CHECK(ssa_pc_renderer_resize(&gpu,480,320)==1);compare(&gpu,&cpu,g);
    CHECK(gpu.water.passes==1 && gpu.reflection && gpu.reflection->hardware==gpu.hardware);
    if(argc>3)snapshot(&gpu,argv[3]);
    /* Parallel effect jobs must match serial composition exactly, including
     * animated water and the reflected scene. Use one GPU to avoid edge-rule
     * differences and repeat at an odd target size across animation phases. */
    {SsaPcHardware serial=*ssa_vulkan_backend(v);serial.rows=NULL;cpu.hardware=&serial;
        CHECK(ssa_pc_renderer_resize(&gpu,641,427)==1 && ssa_pc_renderer_resize(&cpu,641,427)==1);
        for(i=0;i<8;++i){size_t n=(size_t)gpu.width*gpu.height;g->frame_game.counter=i*23;
            g->pc_camera.heading=(uint16_t)(i*113);
            CHECK(ssa_pc_renderer_draw(&gpu,g)==1 && ssa_pc_renderer_draw(&cpu,g)==1);
            CHECK(memcmp(gpu.pixels,cpu.pixels,n*sizeof(*gpu.pixels))==0);
            CHECK(memcmp(gpu.depth,cpu.depth,n*sizeof(*gpu.depth))==0);
            CHECK(memcmp(gpu.water.mask,cpu.water.mask,n*sizeof(*gpu.water.mask))==0);
            CHECK(gpu.actor_pixels==cpu.actor_pixels && gpu.wall_pixels==cpu.wall_pixels);
        }
        g->surface_decals_enabled=g->water_reflections=0;
        CHECK(ssa_pc_renderer_draw(&gpu,g)==1 && ssa_pc_renderer_draw(&cpu,g)==1);
        CHECK(memcmp(gpu.pixels,cpu.pixels,(size_t)gpu.width*gpu.height*sizeof(*gpu.pixels))==0);
        CHECK(memcmp(gpu.depth,cpu.depth,(size_t)gpu.width*gpu.height*sizeof(*gpu.depth))==0);
        CHECK(gpu.actor_pixels==cpu.actor_pixels && gpu.wall_pixels==cpu.wall_pixels);
        g->surface_decals_enabled=g->water_reflections=1;
        cpu.hardware=NULL;if(cpu.reflection)cpu.reflection->hardware=NULL;
    }
    /* Backend is process-owned; resize, level teardown and mode switching
     * cannot leave a live frame or an owner pointing into a freed level. */
    gpu.hardware=NULL;CHECK(ssa_pc_renderer_draw(&gpu,g)==1);gpu.hardware=ssa_vulkan_backend(v);CHECK(ssa_pc_renderer_draw(&gpu,g)==1);
    CHECK(ssa_vulkan_validation_errors(v)==0);
    printf("{\"result\":\"pass\",\"checks\":%u,\"draws\":%u,\"sprite_order_cases\":%u,\"matching_pixels\":%llu,\"compared_pixels\":%llu,\"validation_errors\":%u,\"device\":\"%s\"}\n",checks,draws,sprite_cases,matched,pixels,ssa_vulkan_validation_errors(v),ssa_vulkan_device(v));
    ssa_pc_renderer_free(&gpu);ssa_pc_renderer_free(&cpu);ssa_native_game_free(g);free(g);free(rom);ssa_vulkan_close(v);return 0;
}
