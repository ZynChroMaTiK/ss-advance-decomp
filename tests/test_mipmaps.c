#include "ssa/native_game.h"
#include "ssa/pc_scene.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"mipmaps line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct Spy {const SsaNativeGame *g;const SsaActorImage *sprite;unsigned enabled,walls,actors;SsaPcSample samples[60*40];} Spy;
static int begin(void *ctx,SsaPcRenderer *r,const uint32_t *palette,double focal,unsigned first,unsigned end){
    Spy *s=ctx;(void)palette;(void)focal;(void)first;(void)end;
    CHECK(r->width==60 && r->height==40);s->walls=s->actors=0;memset(s->samples,0,sizeof s->samples);return 1;
}
static int triangle(void *ctx,const SsaPcVertex *v,const SsaPcMaterial *m){
    Spy *s=ctx;unsigned i,level;const SsaTextureFamily *f=&s->g->textures.families[s->g->bindings.family];(void)v;
    if(m->actor){CHECK(m->pixels==s->sprite->pixels && m->transparent);++s->actors;}
    else if(m->pixels){++s->walls;
        for(i=0;i<f->archives[0].count && f->archives[0].images[i].pixels!=m->pixels;++i){}
        CHECK(i<f->archives[0].count);
        for(level=1;level<3;++level){const SsaTextureImage *im=ssa_texture_assets_image(&s->g->textures,s->g->bindings.family,i,level);
            if(s->enabled)CHECK(im && m->mip_pixels[level-1]==im->pixels && m->mip_width[level-1]==im->width && m->mip_height[level-1]==im->height);
            else CHECK(!m->mip_pixels[level-1]);}}
    return 1;
}
static int end(void *ctx,SsaPcRenderer *r,const SsaPcSample **samples){Spy *s=ctx;(void)r;*samples=s->samples;return 1;}
int main(int argc,char **argv){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};SsaSettings settings=SSA_SETTINGS_DEFAULT;
    uint8_t *rom=malloc(0x800000);uint32_t before[60*40],frame;unsigned pitch,i,different=0;FILE *f;
    CHECK(argc>1 && g && rom);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_native_game_step(g)==1 && ssa_native_game_prepare_view(g)==1);
    CHECK(ssa_pc_renderer_resize(&r,60,40)==1);settings.renderer=SSA_RENDER_SOFTWARE;settings.mipmaps=0;
    CHECK(ssa_native_game_apply_settings(g,&settings)==1 && !g->mipmaps);
    frame=g->frame_game.counter;
    for(pitch=0;pitch<2;++pitch){g->render_pitch=pitch?4096:0;g->mipmaps=0;
        CHECK(ssa_pc_scene_draw(&r,g)==1);memcpy(before,r.pixels,sizeof before);
        g->mipmaps=1;CHECK(ssa_pc_scene_draw(&r,g)==1);
        for(i=0;i<60*40;++i)different+=r.pixels[i]!=before[i];
        g->mipmaps=0;CHECK(ssa_pc_scene_draw(&r,g)==1 && !memcmp(before,r.pixels,sizeof before));}
    CHECK(different>20 && frame==g->frame_game.counter);
    {Spy *spy=calloc(1,sizeof(*spy));SsaPcHardware hw={spy,begin,triangle,end,NULL};SsaEntity entity={0};
        SsaActorSource saved=g->sources[0];unsigned count=g->spawn.pool.active_count,mode;int32_t sn,cs;
        const SsaActorResource *resource=&g->actor_assets.resources[63];CHECK(spy && resource->mip_flags==10);
        ssa_sincos(&g->math,g->pc_camera.heading,&sn,&cs);
        entity.body.x=g->pc_camera.x+sn*1024;entity.body.y=g->pc_camera.y+cs*1024;entity.body.z=g->pc_camera.z-32*65536;
        g->sources[0]=(SsaActorSource){&entity,63,0,0,0,0};g->spawn.pool.active_count=1;g->render_pitch=0;r.hardware=&hw;spy->g=g;
        for(mode=0;mode<3;++mode){g->gba_view=mode==2;g->mipmaps=mode==1;spy->enabled=mode!=0;
            spy->sprite=ssa_actor_assets_image(&g->actor_assets,63,1+(spy->enabled?resource->frames:0));
            CHECK(ssa_pc_scene_draw(&r,g)==1 && spy->walls && spy->actors);}
        r.hardware=NULL;g->sources[0]=saved;g->spawn.pool.active_count=count;free(spy);}
    settings.renderer=SSA_RENDER_GBA;settings.mipmaps=0;
    CHECK(ssa_native_game_apply_settings(g,&settings)==1 && g->mipmaps && g->gba_view && !settings.mipmaps);
    settings.renderer=SSA_RENDER_SOFTWARE;CHECK(ssa_native_game_apply_settings(g,&settings)==1 && !g->mipmaps && !g->gba_view);
    ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"changed_pixels\":%u}\n",checks,different);return 0;
}
