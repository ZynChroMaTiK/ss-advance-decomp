#include "vulkan_renderer.h"
#include "ssa/native_game.h"
#include "ssa/pc_scene.h"
#include "ssa/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "godray_fixture.h"
static unsigned checks,draws,sprite_cases;static unsigned long long matched,pixels;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Vulkan check line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include "sprite_clip_fixture.h"
#include "sprite_shadow_fixture.h"
#include "reflection_sky_fixture.h"
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
/* Persistent uploads must observe mutation at the same address, live palette
 * changes, dimensions and mip levels, even after an unrelated pass. */
static void texture_cache_checks(SsaVulkanRenderer *v)
{
    const SsaPcHardware *hw=ssa_vulkan_backend(v);SsaPcRenderer r={0};const SsaPcSample *samples;
    uint32_t palette[256]={0};uint8_t texels[256],mip0[64],mip1[16];SsaPcMaterial m={0};
    CHECK(ssa_pc_renderer_resize(&r,32,32)==1);
    m.pixels=texels;m.width=m.height=16;m.actor=m.transparent=1;
    m.mip_pixels[0]=mip0;m.mip_pixels[1]=mip1;m.mip_width[0]=m.mip_height[0]=8;m.mip_width[1]=m.mip_height[1]=4;
    for(unsigned pass=0;pass<12;++pass){unsigned index=1+pass%3;uint32_t color=0x102030+pass*0x010305;
        memset(texels,index,sizeof texels);memset(mip0,index,sizeof mip0);memset(mip1,index,sizeof mip1);
        palette[index]=color;m.width=pass&1?8:16;m.height=pass&1?32:16;
        for(unsigned i=0;i<32*32;++i)r.pixels[i]=0;
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
        quad(hw,-1,-1,1,1,1,&m);CHECK(hw->end(hw->context,&r,&samples)==1);
        CHECK(r.pixels[16*32+16]==color);
        /* Dense geometry requests the same material repeatedly, including
         * a second key sharing its base/mip images. */
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
        for(unsigned i=0;i<128;++i){m.color=i&1;quad(hw,-1,-1,1,1,1,&m);}
        CHECK(hw->end(hw->context,&r,&samples)==1 && r.pixels[16*32+16]==color);
    }
    m.width=m.height=16;memset(texels,1,sizeof texels);memset(mip0,2,sizeof mip0);
    palette[1]=0xff0000;palette[2]=0x00ff00;palette[3]=0x0000ff;
    for(unsigned pass=0;pass<2;++pass){unsigned index=pass?2:3;
        memset(mip1,index,sizeof mip1);
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
        quad(hw,-.0625f,-.0625f,.0625f,.0625f,1,&m);
        CHECK(hw->end(hw->context,&r,&samples)==1 && r.pixels[16*32+16]==palette[index]);
    }
    /* Exceed the retained-entry budget, then verify eviction on the next
     * pass without invalidating any offsets already submitted this pass. */
    {uint8_t *many=malloc(4100);CHECK(many);memset(many,2,4100);
        memset(&m,0,sizeof m);m.width=m.height=1;m.actor=1;
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
        for(unsigned i=0;i<4100;++i){m.pixels=many+i;quad(hw,-1,-1,1,1,1,&m);}
        CHECK(hw->end(hw->context,&r,&samples)==1 && r.pixels[16*32+16]==palette[2]);
        many[0]=3;m.pixels=many;
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);quad(hw,-1,-1,1,1,1,&m);
        CHECK(hw->end(hw->context,&r,&samples)==1 && r.pixels[16*32+16]==palette[3]);free(many);
    }
    ssa_pc_renderer_free(&r);
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
    /* Actor fragments have already been silhouette-clipped by the scene.
     * The backend must paint them even across nearer world floor samples.
     * Geometry submitted later still resolves before the actor pass. */
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
        CHECK(r.pixels[16*32+24]==0x00ff00 && samples[16*32+24].z>2);
        CHECK(r.pixels[16*32+16]==0x00ff00 && samples[16*32+16].z>2);
    }
    /* Solid-colored authored mip fixtures expose which level the shader
     * actually samples as a textured quad shrinks on screen. */
    {uint8_t full[32*32],half[16*16],quarter[8*8];SsaPcMaterial m={0};unsigned level;
        memset(full,1,sizeof full);memset(half,2,sizeof half);memset(quarter,3,sizeof quarter);palette[3]=0x0000ff;
        m.pixels=full;m.width=m.height=32;m.mip_pixels[0]=half;m.mip_pixels[1]=quarter;
        m.mip_width[0]=m.mip_height[0]=16;m.mip_width[1]=m.mip_height[1]=8;
        for(level=0;level<3;++level){CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);
            quad(hw,-1,1,1,-1,(float)(1u<<level),&m);CHECK(hw->end(hw->context,&r,&samples)==1);
            CHECK(r.pixels[16*32+16]==palette[level+1]);}
        m.mip_pixels[0]=m.mip_pixels[1]=NULL;
        CHECK(hw->begin(hw->context,&r,palette,16,0,32)==1);quad(hw,-1,1,1,-1,4,&m);
        CHECK(hw->end(hw->context,&r,&samples)==1 && r.pixels[16*32+16]==palette[1]);
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
static void post_checks(SsaVulkanRenderer *v)
{
    SsaPcRenderer r={0};const SsaPcHardware *hw=ssa_vulkan_backend(v);unsigned changed=0;
    CHECK(hw->postprocess && ssa_pc_renderer_resize(&r,63,47)==1);
    r.effect_capacity=63*47;r.effect_samples=calloc(r.effect_capacity,sizeof(*r.effect_samples));CHECK(r.effect_samples);
    for(unsigned flags=1;flags<8;++flags){r.hardware_features=flags;
        for(unsigned i=0;i<63*47;++i){r.pixels[i]=0xabcdef;r.effect_samples[i].z=100;}
        CHECK(hw->postprocess(hw->context,&r,48)==1);
        for(unsigned i=0;i<63*47;++i)CHECK(r.pixels[i]==0xabcdef);
    }
    r.hardware_features=SSA_PC_AA;
    for(unsigned y=0;y<47;++y)for(unsigned x=0;x<63;++x)r.pixels[y*63+x]=x>y?0xffffff:0;
    CHECK(hw->postprocess(hw->context,&r,48)==1);
    for(unsigned i=0;i<63*47;++i)changed+=r.pixels[i]!=0 && r.pixels[i]!=0xffffff;
    CHECK(changed>20);
    /* The direct AA path needs no depth allocation and must match AA after
     * neutral AO, including borders, odd extents, resize and buffer reuse. */
    for(unsigned pass=0;pass<3;++pass){
        unsigned w=pass==0?63:pass==1?1:127,h=pass==0?47:pass==1?1:65;
        size_t n=(size_t)w*h;uint32_t *direct=malloc(n*sizeof(*direct));CHECK(direct);
        CHECK(ssa_pc_renderer_resize(&r,w,h)==1);
        free(r.effect_samples);r.effect_samples=NULL;r.effect_capacity=0;
        for(size_t i=0;i<n;++i)r.pixels[i]=(uint32_t)(i*2654435761u)&0xffffff;
        r.hardware_features=SSA_PC_AA;CHECK(hw->postprocess(hw->context,&r,48)==1);
        memcpy(direct,r.pixels,n*sizeof(*direct));
        r.effect_samples=calloc(n,sizeof(*r.effect_samples));r.effect_capacity=n;CHECK(r.effect_samples);
        for(size_t i=0;i<n;++i){r.pixels[i]=(uint32_t)(i*2654435761u)&0xffffff;r.effect_samples[i].z=FLT_MAX;}
        r.hardware_features=SSA_PC_AA|SSA_PC_AO;CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(!memcmp(direct,r.pixels,n*sizeof(*direct)));
        for(size_t i=0;i<n;++i)r.pixels[i]=(uint32_t)(i*2654435761u)&0xffffff;
        r.hardware_features=SSA_PC_AA;CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(!memcmp(direct,r.pixels,n*sizeof(*direct)));free(direct);
    }
    CHECK(ssa_pc_renderer_resize(&r,63,47)==1);
    r.hardware_features=SSA_PC_AO;changed=0;
    for(unsigned y=0;y<47;++y)for(unsigned x=0;x<63;++x){r.pixels[y*63+x]=0xc0c0c0;r.effect_samples[y*63+x].z=x<31?80.0f:100.0f;}
    CHECK(hw->postprocess(hw->context,&r,48)==1);
    for(unsigned i=0;i<63*47;++i){CHECK(r.pixels[i]<=0xc0c0c0 && r.pixels[i]>=0x7c7c7c);changed+=r.pixels[i]!=0xc0c0c0;}
    CHECK(changed>20 && r.pixels[23*63+10]==0xc0c0c0);
    for(unsigned i=0;i<63*47;++i){r.pixels[i]=0x203040;r.effect_samples[i].z=FLT_MAX;}
    CHECK(hw->postprocess(hw->context,&r,48)==1);
    for(unsigned i=0;i<63*47;++i)CHECK(r.pixels[i]==0x203040);
    CHECK(ssa_pc_renderer_resize(&r,1,1)==1);r.hardware_features=SSA_PC_AA|SSA_PC_AO;r.pixels[0]=0x987654;r.effect_samples[0].z=100;
    CHECK(hw->postprocess(hw->context,&r,1)==1 && r.pixels[0]==0x987654);
    ssa_pc_renderer_free(&r);
}
static void motion_checks(SsaVulkanRenderer *v)
{
    const SsaPcHardware *hw=ssa_vulkan_backend(v);SsaPcRenderer r={0};uint32_t original[63*47],baseline[63*47];
    CHECK(ssa_pc_renderer_resize(&r,63,47)==1);r.effect_capacity=63*47;
    r.effect_samples=calloc(r.effect_capacity,sizeof(*r.effect_samples));CHECK(r.effect_samples);
    r.motion.axes[0]=r.motion.axes[4]=r.motion.axes[8]=1;
    for(unsigned y=0;y<47;++y)for(unsigned x=0;x<63;++x){unsigned i=y*63+x;
        original[i]=(x/2)%2?0xffffff:0;r.effect_samples[i].z=100;}
    for(unsigned effects=0;effects<8;++effects){unsigned changed=0;
        unsigned base_passes=((effects&SSA_PC_AA)!=0)+((effects&SSA_PC_AO)!=0),before;
        r.hardware_features=effects;r.motion.valid=0;memcpy(r.pixels,original,sizeof original);
        CHECK(hw->postprocess(hw->context,&r,48)==1);memcpy(baseline,r.pixels,sizeof baseline);
        r.hardware_features=effects|SSA_PC_MOTION;r.motion.valid=1;r.motion.origin[0]=0;++r.motion.epoch;
        before=ssa_vulkan_timing(v).post_passes;
        memcpy(r.pixels,original,sizeof original);CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(ssa_vulkan_timing(v).post_passes-before==base_passes);
        CHECK(!memcmp(baseline,r.pixels,sizeof baseline)); /* First frame. */
        before=ssa_vulkan_timing(v).post_passes;
        r.motion.origin[0]=16;memcpy(r.pixels,original,sizeof original);
        CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(ssa_vulkan_timing(v).post_passes-before==base_passes+1);
        for(unsigned i=0;i<63*47;++i)changed+=r.pixels[i]!=baseline[i];CHECK(changed>0);
        before=ssa_vulkan_timing(v).post_passes;
        memcpy(r.pixels,original,sizeof original);CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(!memcmp(baseline,r.pixels,sizeof baseline)); /* Stationary. */
        CHECK(ssa_vulkan_timing(v).post_passes-before==base_passes);
        for(unsigned cut=0;cut<5;++cut){r.motion.origin[0]+=16;before=ssa_vulkan_timing(v).post_passes;
            if(cut==0)r.motion.valid=0;else if(cut==1){r.motion.valid=1;++r.motion.scene;}
            else if(cut==2)++r.motion.cut;else if(cut==3)++r.motion.epoch;else r.motion.origin[0]+=200;
            memcpy(r.pixels,original,sizeof original);CHECK(hw->postprocess(hw->context,&r,48)==1);
            CHECK(!memcmp(baseline,r.pixels,sizeof baseline));
            CHECK(ssa_vulkan_timing(v).post_passes-before==base_passes);}
        /* A skipped cut must become the next frame's history. */
        before=ssa_vulkan_timing(v).post_passes;r.motion.origin[0]+=16;
        memcpy(r.pixels,original,sizeof original);CHECK(hw->postprocess(hw->context,&r,48)==1);
        CHECK(ssa_vulkan_timing(v).post_passes-before==base_passes+1);
        CHECK(memcmp(baseline,r.pixels,sizeof baseline));
    }
    /* No metadata allocation/upload is required by an identity motion frame. */
    free(r.effect_samples);r.effect_samples=NULL;r.effect_capacity=0;
    r.hardware_features=SSA_PC_MOTION;++r.motion.epoch;
    unsigned before=ssa_vulkan_timing(v).post_passes;
    memcpy(r.pixels,original,sizeof original);CHECK(hw->postprocess(hw->context,&r,48)==1);
    CHECK(!memcmp(original,r.pixels,sizeof original));
    CHECK(hw->postprocess(hw->context,&r,48)==1 && ssa_vulkan_timing(v).post_passes==before);
    ssa_pc_renderer_free(&r);
}
static void retained_ao_checks(SsaVulkanRenderer *v)
{
    const SsaPcHardware *hw=ssa_vulkan_backend(v);SsaPcRenderer main={0},reflection={0};
    const SsaPcSample *samples;uint32_t palette[256]={0};SsaPcMaterial material={0};
    palette[1]=0xc0c0c0;palette[2]=0x203040;
    CHECK(ssa_pc_renderer_resize(&reflection,17,13)==1);
    for(unsigned pass=0;pass<3;++pass){unsigned width=pass==1?129:63,height=pass==1?71:47;
        size_t n=(size_t)width*height;uint32_t *input=malloc(n*4),*output=malloc(n*4);SsaPcSample *host=malloc(n*sizeof(*host));
        CHECK(input && output && host && ssa_pc_renderer_resize(&main,width,height)==1);
        main.hardware_features=SSA_PC_AO|(pass==1?SSA_PC_AA:0);
        memset(main.pixels,0,n*4);material.color=1;material.actor=0;
        CHECK(hw->begin(hw->context,&main,palette,48,0,height)==1);
        quad(hw,-200,-200,200,200,100,&material);quad(hw,-200,-200,0,200,80,&material);
        material.actor=1;material.color=2;quad(hw,5,-5,10,5,70,&material);
        CHECK(hw->end(hw->context,&main,&samples)==1);
        memcpy(input,main.pixels,n*4);memcpy(host,samples,n*sizeof(*host));
        for(size_t i=0;i<n;++i)if((unsigned)host[i].code&(1024|2048))host[i].z=FLT_MAX;
        /* Overwrite and resize the shared GPU attachments before main AO. */
        reflection.hardware_features=SSA_PC_AF;material.actor=0;
        CHECK(hw->begin(hw->context,&reflection,palette,8,0,13)==1);
        quad(hw,-20,-20,20,20,1,&material);CHECK(hw->end(hw->context,&reflection,&samples)==1);
        CHECK(!main.effect_samples && hw->postprocess(hw->context,&main,48)==1);
        memcpy(output,main.pixels,n*4);
        /* Compare retained raw GPU metadata with the former masked host path. */
        memcpy(main.pixels,input,n*4);main.effect_samples=host;main.effect_capacity=n;
        CHECK(hw->postprocess(hw->context,&main,48)==1 && !memcmp(output,main.pixels,n*4));
        free(main.effect_samples);main.effect_samples=NULL;main.effect_capacity=0;free(input);free(output);
    }
    ssa_pc_renderer_free(&main);ssa_pc_renderer_free(&reflection);
}
static void texel_ao_checks(SsaVulkanRenderer *v)
{
    const SsaPcHardware *hw=ssa_vulkan_backend(v);SsaPcRenderer r={0};unsigned changed=0;
    uint32_t wall[128*64];CHECK(ssa_pc_renderer_resize(&r,128,64)==1);
    r.effect_capacity=128*64;r.effect_samples=calloc(r.effect_capacity,sizeof(*r.effect_samples));CHECK(r.effect_samples);
    r.hardware_features=SSA_PC_AO;
    /* Eight screen pixels per base texel. Flat-plane UVs use world units;
     * wall UVs already use texels. Both must produce the same hard grid. */
    for(unsigned plane=0;plane<2;++plane){
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x){size_t at=y*128+x;
            float scale=plane?2.0f:1.0f;r.pixels[at]=0xc0c0c0;
            r.effect_samples[at]=(SsaPcSample){(x+0.5f)*scale/8.0f,(y+0.5f)*scale/8.0f,x<32?80.0f:100.0f,plane?256.0f:4096.0f};}
        CHECK(hw->postprocess(hw->context,&r,96)==1);
        for(unsigned y=8;y<56;++y)for(unsigned x=8;x<120;++x){
            CHECK(r.pixels[y*128+x]==r.pixels[(y/8*8+4)*128+x/8*8+4]);
            changed+=r.pixels[y*128+x]!=0xc0c0c0;}
        if(!plane)memcpy(wall,r.pixels,sizeof wall);
        else CHECK(!memcmp(wall,r.pixels,sizeof wall));
    }
    CHECK(changed>0);
    /* A slanted plane, shifted negative UVs and a neighboring depth step.
     * Group by world texel, not screen blocks; perspective must not split AO. */
    for(unsigned phase=0;phase<2;++phase){
        int keys[8192][2];uint32_t colors[8192];unsigned used=0,shaded=0;
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x){
            float ray=((float)x+0.5f-64)/320,z=(x<32?60.0f:80.0f)/(1-0.25f*ray);
            r.pixels[y*128+x]=0xc0c0c0;
            r.effect_samples[y*128+x]=(SsaPcSample){ray*z/2+phase*1.25f,
                (32.0f-y-0.5f)*z/640-phase*0.75f,z,4096};}
        CHECK(hw->postprocess(hw->context,&r,320)==1);
        for(unsigned y=12;y<52;++y)for(unsigned x=40;x<112;++x){
            SsaPcSample sample=r.effect_samples[y*128+x];int u=(int)floorf(sample.u),t=(int)floorf(sample.v);unsigned k;
            for(k=0;k<used;++k)if(keys[k][0]==u && keys[k][1]==t)break;
            if(k==used){keys[k][0]=u;keys[k][1]=t;colors[k]=r.pixels[y*128+x];++used;}
            CHECK(colors[k]==r.pixels[y*128+x]);shaded+=r.pixels[y*128+x]!=0xc0c0c0;
        }
        CHECK(used>20 && shaded>0);
    }
    ssa_pc_renderer_free(&r);
}
static void anisotropic_checks(SsaVulkanRenderer *v)
{
    const SsaPcHardware *hw=ssa_vulkan_backend(v);SsaPcRenderer r={0};const SsaPcSample *samples;
    uint32_t palette[256]={0};uint8_t texels[512*64],mip0[256*32],mip1[128*16];SsaPcMaterial m={0};
    CHECK(ssa_pc_renderer_resize(&r,64,64)==1);palette[1]=0xffffff;palette[2]=0;palette[3]=0x808080;
    for(unsigned x=0;x<512;++x)for(unsigned y=0;y<64;++y)texels[x*64+y]=(uint8_t)(1+(y/8)%2);
    memset(mip0,3,sizeof mip0);memset(mip1,3,sizeof mip1);
    m.pixels=texels;m.width=512;m.height=64;m.mip_pixels[0]=mip0;m.mip_pixels[1]=mip1;
    m.mip_width[0]=256;m.mip_height[0]=32;m.mip_width[1]=128;m.mip_height[1]=16;
    for(unsigned pass=0;pass<3;++pass){r.hardware_features=pass==1?SSA_PC_AF:0;
        CHECK(hw->begin(hw->context,&r,palette,32,0,64)==1);quad(hw,-1,-1,1,1,1,&m);
        CHECK(hw->end(hw->context,&r,&samples)==1);
        CHECK(pass==1?(r.pixels[4*64+32]==0 && r.pixels[12*64+32]==0xffffff):r.pixels[4*64+32]==0x808080);
        CHECK(samples[4*64+32].z==1);}
    /* Magnification, including fractional texel edges, must retain exact
     * palette colors and match nearest sampling pixel for pixel. */
    {uint32_t nearest[64*64];memset(&m,0,sizeof m);m.pixels=texels;m.width=m.height=16;
        for(unsigned x=0;x<16;++x)for(unsigned y=0;y<16;++y)texels[x*16+y]=(uint8_t)(1+((x^y)&1));
        for(unsigned pass=0;pass<2;++pass){r.hardware_features=pass?SSA_PC_AF:0;
            memset(r.pixels,0,64*64*sizeof(*r.pixels));
            CHECK(hw->begin(hw->context,&r,palette,32,0,64)==1);quad(hw,-1.03f,-1.03f,1.03f,1.03f,1,&m);
            CHECK(hw->end(hw->context,&r,&samples)==1);
            if(!pass)memcpy(nearest,r.pixels,sizeof nearest);
            else for(unsigned i=0;i<64*64;++i)CHECK(r.pixels[i]==nearest[i] && (r.pixels[i]==0 || r.pixels[i]==0xffffff));}
    }
    ssa_pc_renderer_free(&r);
}
static void scene_motion(SsaPcRenderer *gpu,SsaNativeGame *g,const char *path)
{
    SsaPcRenderer reference={0};unsigned saved=g->vulkan_features,heading=g->pc_camera.heading;size_t changed=0;
    reference.hardware=gpu->hardware;CHECK(ssa_pc_renderer_resize(&reference,gpu->width,gpu->height)==1);
    g->vulkan_features=SSA_PC_MOTION;CHECK(ssa_pc_renderer_draw(gpu,g)==1 && gpu->motion.valid);
    g->pc_camera.heading=(uint16_t)(heading+256);g->vulkan_features=0;
    CHECK(ssa_pc_renderer_draw(&reference,g)==1);g->vulkan_features=SSA_PC_MOTION;
    CHECK(ssa_pc_renderer_draw(gpu,g)==1);
    for(size_t i=0;i<(size_t)gpu->width*gpu->height;++i){changed+=gpu->pixels[i]!=reference.pixels[i];CHECK(gpu->depth[i]==reference.depth[i]);}
    CHECK(changed>0);
    if(path){char out[1024];CHECK(snprintf(out,sizeof out,"%s-motion.bmp",path)>0);snapshot(gpu,out);}
    g->vulkan_features=0;CHECK(ssa_pc_renderer_draw(gpu,g)==1);
    CHECK(!memcmp(gpu->pixels,reference.pixels,(size_t)gpu->width*gpu->height*4));
    g->vulkan_features=saved;g->pc_camera.heading=(uint16_t)heading;ssa_pc_renderer_free(&reference);
}
static void scene_effects(SsaPcRenderer *gpu,SsaPcRenderer *cpu,SsaNativeGame *g,const char *path)
{
    size_t n;uint32_t *baseline,*software;float *depth;uint16_t hud=g->hud_style;unsigned effects=g->vulkan_features;
    CHECK(ssa_pc_renderer_resize(gpu,641,359)==1 && ssa_pc_renderer_resize(cpu,641,359)==1);n=641*359;
    baseline=malloc(n*4);software=malloc(n*4);depth=malloc(n*sizeof(float));CHECK(baseline && software && depth);
    g->vulkan_features=0;CHECK(ssa_pc_renderer_draw(gpu,g)==1 && ssa_pc_renderer_draw(cpu,g)==1);
    memcpy(baseline,gpu->pixels,n*4);memcpy(software,cpu->pixels,n*4);memcpy(depth,gpu->depth,n*sizeof(float));
    for(unsigned flags=1;flags<8;++flags){size_t changed=0;g->vulkan_features=flags;
        CHECK(ssa_pc_renderer_draw(gpu,g)==1 && ssa_pc_renderer_draw(cpu,g)==1);
        CHECK(!gpu->effect_samples && !gpu->effect_capacity);
        CHECK(!memcmp(depth,gpu->depth,n*sizeof(float)) && !memcmp(software,cpu->pixels,n*4));
        for(size_t i=0;i<n;++i){CHECK(!(gpu->pixels[i]&0xff000000));changed+=gpu->pixels[i]!=baseline[i];}
        CHECK(changed>0);}
    if(path){char out[1024];CHECK(snprintf(out,sizeof out,"%s-effects.bmp",path)>0);snapshot(gpu,out);}
    g->vulkan_features=0;CHECK(ssa_pc_renderer_draw(gpu,g)==1 && !memcmp(baseline,gpu->pixels,n*4));
    g->vulkan_features=effects;g->hud_style=hud;free(depth);free(software);free(baseline);
}
int main(int argc,char **argv)
{
    SsaVulkanRenderer *v=ssa_vulkan_open();SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer gpu={0},cpu={0};
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned i;static const unsigned sizes[][2]={{1,1},{321,213},{960,640},{1280,720},{240,160}};
    CHECK(v && g && rom && argc>=2);primitives(v);texture_cache_checks(v);post_checks(v);motion_checks(v);retained_ao_checks(v);texel_ao_checks(v);anisotropic_checks(v);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    {SsaMenu *m=calloc(1,sizeof(*m));CHECK(m && ssa_menu_init(m,rom,0x800000,1)==1);
        CHECK(m->settings.renderer==SSA_RENDER_VULKAN);m->settings.renderer=SSA_RENDER_SOFTWARE;
        m->page=m->draw_page=SSA_MENU_GRAPHICS;m->selection=1;m->fading=m->fade_volume=0;m->vulkan_available=1;
        CHECK(ssa_menu_step(m,SSA_MENU_LEFT)==1 && m->settings.renderer==SSA_RENDER_VULKAN);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_SOFTWARE);
        CHECK(ssa_menu_step(m,SSA_MENU_RIGHT)==1 && m->settings.renderer==SSA_RENDER_GBA);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.renderer==SSA_RENDER_VULKAN);
        for(unsigned effect=9;effect<=12;++effect){m->selection=effect;
            CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1);}
        CHECK(m->settings.vulkan_antialiasing && m->settings.vulkan_anisotropic && m->settings.vulkan_ambient_occlusion && m->settings.vulkan_motion_blur);
        for(unsigned legacy=0;legacy<2;++legacy){m->settings.renderer=legacy;
            for(unsigned effect=9;effect<=12;++effect){m->selection=effect;CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1);}
            CHECK(m->settings.vulkan_antialiasing && m->settings.vulkan_anisotropic && m->settings.vulkan_ambient_occlusion && m->settings.vulkan_motion_blur);
            m->selection=7;CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==8);
            CHECK(ssa_menu_step(m,SSA_MENU_UP)==1 && m->selection==7);
            CHECK(ssa_menu_pointer(m,40,72,1)==0 && m->selection==7);}
        m->settings.renderer=SSA_RENDER_VULKAN;m->selection=3;
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==10);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==9);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==11);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==12);
        CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==4);
        m->vulkan_available=0;m->selection=9;
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.vulkan_antialiasing);
        CHECK(ssa_menu_pointer(m,40,72,1)==0);m->vulkan_available=1;
        CHECK(ssa_menu_pointer(m,40,92,1)==SSA_MENU_CONFIRM && m->selection==12);
        for(unsigned renderer=0;renderer<SSA_RENDERERS;++renderer){
            m->settings.renderer=renderer;m->vsync_available=1;m->settings.vsync=0;
            CHECK(ssa_menu_pointer(m,40,44,1)==SSA_MENU_CONFIRM && m->selection==13);
            CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.vsync);
            CHECK(ssa_menu_step(m,SSA_MENU_DOWN)==1 && m->selection==(renderer==SSA_RENDER_GBA?4:3));
            m->vsync_available=0;m->selection=13;
            CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==1 && m->settings.vsync);
            CHECK(ssa_menu_pointer(m,40,44,1)==0);
            CHECK(ssa_menu_step(m,SSA_MENU_UP)==1 && m->selection==(renderer==SSA_RENDER_GBA?1:2));
        }
        m->settings.renderer=SSA_RENDER_VULKAN;m->settings.vsync=0;
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
    sprite_clip_fixture(&gpu,g);sprite_clip_fixture(&cpu,g);
    sprite_shadow_fixture(&gpu,g);sprite_shadow_fixture(&cpu,g);
    sprite_order(&gpu,g);
    g->mipmaps=1;
    CHECK(ssa_pc_renderer_resize(&gpu,240,160)==1);
    g->render_pitch=0;compare(&gpu,&cpu,g);
    g->render_pitch=4096;compare(&gpu,&cpu,g);
    g->sprite_shadows_enabled=1;g->render_pitch=-4096;compare(&gpu,&cpu,g);
    g->mipmaps=0;
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
    g->mipmaps=1;compare(&gpu,&cpu,g); /* Mirrored texture/sprite mip policy. */
    reflection_sky_fixture(&gpu,g);reflection_sky_fixture(&cpu,g);
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
        {SsaRenderCamera saved=g->pc_camera;int pitch=g->render_pitch;size_t n=(size_t)gpu.width*gpu.height;
            CHECK(godray_fixture_camera(g));g->render_pitch=0;g->water_reflections=0;g->surface_decals_enabled=0;g->godrays_enabled=1;
            CHECK(ssa_pc_renderer_draw(&gpu,g)==1 && ssa_pc_renderer_draw(&cpu,g)==1);
            CHECK(gpu.godray_triangles>0 && gpu.godray_triangles==cpu.godray_triangles);
            CHECK(!memcmp(gpu.pixels,cpu.pixels,n*sizeof(*gpu.pixels)));
            CHECK(!memcmp(gpu.depth,cpu.depth,n*sizeof(*gpu.depth)));
            g->pc_camera=saved;g->render_pitch=pitch;g->water_reflections=1;
        }
        cpu.hardware=NULL;if(cpu.reflection)cpu.reflection->hardware=NULL;
    }
    /* Backend is process-owned; resize, level teardown and mode switching
     * cannot leave a live frame or an owner pointing into a freed level. */
    gpu.hardware=NULL;CHECK(ssa_pc_renderer_draw(&gpu,g)==1);gpu.hardware=ssa_vulkan_backend(v);CHECK(ssa_pc_renderer_draw(&gpu,g)==1);
    scene_effects(&gpu,&cpu,g,argc>2?argv[2]:NULL);
    scene_motion(&gpu,g,argc>2?argv[2]:NULL);
    CHECK(ssa_vulkan_validation_errors(v)==0);
    printf("{\"result\":\"pass\",\"checks\":%u,\"draws\":%u,\"sprite_order_cases\":%u,\"matching_pixels\":%llu,\"compared_pixels\":%llu,\"validation_errors\":%u,\"device\":\"%s\"}\n",checks,draws,sprite_cases,matched,pixels,ssa_vulkan_validation_errors(v),ssa_vulkan_device(v));
    ssa_pc_renderer_free(&gpu);ssa_pc_renderer_free(&cpu);ssa_native_game_free(g);free(g);free(rom);ssa_vulkan_close(v);return 0;
}
