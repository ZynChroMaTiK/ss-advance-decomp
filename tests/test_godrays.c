#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "ssa/pc_scene.h"
#include "ssa/actor_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "godray_fixture.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"godrays line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void put16(uint8_t *p,int v){p[0]=(uint8_t)v;p[1]=(uint8_t)((unsigned)v>>8);}
typedef struct Target {uint8_t pixels[128*128],covered[128*128];uint32_t depth;unsigned count,dither;int bottom,top;SsaGodrayPalette palette;} Target;
static void span(void *context,const SsaGodraySpan *s){Target *t=context;unsigned i;int64_t q=s->q,v=s->vq,u=s->uq;
    CHECK(s->y<128 && s->x+s->count<=128);t->bottom=s->bottom;t->top=s->top;
    for(i=0;i<s->count;++i,q+=s->dq,v+=s->dvq,u+=s->duq){uint32_t depth;unsigned shade=ssa_godray_sample(q/65536,v/65536,&depth);size_t at=s->y*128+s->x+i;
        if(t->dither)shade=ssa_godray_dither(q/65536,v/65536,u/65536,s->bottom,s->top);
        CHECK(shade<=16);if(!shade || depth>=t->depth)continue;
        CHECK(!t->covered[at]);t->covered[at]=1;t->pixels[at]=t->palette.blend[shade][t->pixels[at]];++t->count;}}
static void clear(Target *t){memset(t->pixels,4,sizeof t->pixels);memset(t->covered,0,sizeof t->covered);t->count=0;t->depth=UINT32_MAX;}
static void dithering(void){unsigned gradient,x,y;
    CHECK(!ssa_godray_dither(0,1,0,0,96));
    CHECK(!ssa_godray_dither(1024,-1,-1024,-128,-32));
    CHECK(!ssa_godray_dither(1024,512,0,96,96));
    CHECK(ssa_godray_dither(1024,1024,0,0,96)==16);
    /* Halfway between shades8/10, at world texel centers. */
    for(y=0;y<8;++y)for(x=0;x<8;++x){unsigned expected=8+2*!((x+y)&1);
        CHECK(ssa_godray_dither(16384,9216,(int64_t)x*16384+1000,(int)y*2-53,(int)y*2+43)==expected);
        CHECK(ssa_godray_dither(16384,9216,(int64_t)x*16384+15000,(int)y*2-53,(int)y*2+43)==expected);
    }
    for(gradient=1;gradient<1024;gradient+=2){unsigned sum=0;
        for(y=0;y<8;++y)for(x=0;x<8;++x){int bottom=(int)y*2+1-(int)gradient;
            int64_t u=((int64_t)x-4)*1024+256;
            unsigned shade=ssa_godray_dither(1024,gradient,u,bottom,bottom+1024);
            unsigned low=(gradient/128)*2;
            CHECK(shade==low || (low<16 && shade==low+2));sum+=shade;
            CHECK(shade==ssa_godray_dither(1024,gradient,u+8*1024,bottom+16,bottom+1040));
        }
        CHECK(abs((int)sum-(int)gradient)<=1);
    }
    /* Sweep through texel interiors vertically as well as horizontally.
     * Include odd boundaries, short shafts, negative coordinates and different
     * reciprocal depths: no sample inside the same cell may change shade. */
    {static const int bottoms[]={-129,-128,-1,0,1,17};
        static const int heights[]={1,3,17,96};unsigned b,h,k,scale;
        for(b=0;b<sizeof bottoms/sizeof *bottoms;++b)
        for(h=0;h<sizeof heights/sizeof *heights;++h){int bottom=bottoms[b],top=bottom+heights[h],row;
            for(row=bottom/2-1;row<=top/2+1;++row)for(x=0;x<8;++x){unsigned first=17;
                for(k=1;k<512;k+=16){int z=row*512+(int)k;
                    if(z<=bottom*256 || z>=top*256)continue;
                    for(scale=1;scale<=16;scale*=4){int64_t q=(int64_t)heights[h]*256*scale;
                        unsigned shade=ssa_godray_dither(q,(int64_t)(z-bottom*256)*scale,
                            ((int64_t)x-4)*q+q*k/512,bottom,top);
                        if(first==17)first=shade;
                        CHECK(shade==first);
                    }
                }
            }
        }
    }
}
static void isolated(void){
    uint8_t data[88]={0};uint16_t palette[256]={0};SsaMapAssets map={0};SsaGodrayCamera camera={0};Target *t=calloc(1,sizeof(*t));SsaGodrayOps ops={span,t};uint8_t baseline[128*128];unsigned i,y;
    CHECK(t);map.data=data;map.size=sizeof data;map.vertex_count=2;map.line_count=1;map.sector_count=2;map.lines=8;map.sectors=48;
    put16(data+6,100);put16(data+10,1);put16(data+22,1);put16(data+34,4);put16(data+50,256);put16(data+70,128);data[79]=1;
    for(i=0;i<32;++i)palette[i]=(uint16_t)(i|(i<<5)|(i<<10));
    CHECK(ssa_godray_palette(&t->palette,palette)==1);
    for(i=0;i<256;++i)CHECK(t->palette.blend[0][i]==i);
    for(i=1;i<=16;++i)CHECK(t->palette.blend[i][4]>=t->palette.blend[i-1][4]);
    camera.x=-64*256;camera.y=50*256;camera.z=104*256;camera.sine=65536;camera.pitch_cosine=65536;
    camera.focal_x=camera.focal_y=64*256+64;camera.width=camera.height=128;
    clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && t->count>100 && t->bottom==32 && t->top==128);memcpy(baseline,t->pixels,sizeof baseline);
    for(y=0;y<128;++y){unsigned color=0;for(i=0;i<128;++i)if(t->covered[y*128+i]){if(color){CHECK(color==t->pixels[y*128+i]);}color=t->pixels[y*128+i];}}
    {uint8_t dithered[128*128];t->dither=1;clear(t);
        CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && memcmp(baseline,t->pixels,sizeof baseline));
        memcpy(dithered,t->pixels,sizeof dithered);clear(t);
        CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && !memcmp(dithered,t->pixels,sizeof dithered));
        put16(data+8,1);put16(data+10,0);put16(data+20,1);put16(data+22,0);clear(t);
        CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && !memcmp(dithered,t->pixels,sizeof dithered));
        put16(data+8,0);put16(data+10,1);put16(data+20,0);put16(data+22,1);
        camera.clip=1;camera.clip_z=104;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2);
        for(y=0;y<128;++y)for(i=0;i<128;++i)if(t->covered[y*128+i])CHECK(t->pixels[y*128+i]==dithered[y*128+i]);
        camera.clip=0;t->dither=0;
    }
    clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && !memcmp(baseline,t->pixels,sizeof baseline));
    clear(t);t->depth=1;CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && !t->count);
    camera.x=-4096*256;camera.focal_x=camera.focal_y=4096*256+64;clear(t);
    CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && t->count>100); /* No far cutoff. */
    camera.x=-64*256;camera.focal_x=camera.focal_y=64*256+64;
    put16(data+48,-128);clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && t->bottom==32);
    put16(data+48,96);clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && t->bottom==112);put16(data+48,0);
    put16(data+8,1);put16(data+10,0);put16(data+20,1);put16(data+22,0);clear(t);
    CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && !memcmp(baseline,t->pixels,sizeof baseline));
    /* Reflection clipping retains the original height gradient above water. */
    camera.clip=1;camera.clip_z=104;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==2 && t->count);
    for(y=0;y<128;++y)for(i=0;i<128;++i){unsigned at=y*128+i;
        if(t->covered[at])CHECK(t->pixels[at]==baseline[at]);
        if(y>=64)CHECK(!t->covered[at]);}
    camera.clip_z=128;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==0 && !t->count);camera.clip=0;
    data[79]=0;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)==0 && !t->count);data[79]=1;
    camera.x=64*256;camera.sine=-65536;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)>0 && t->count>100);
    camera.x=-8*256;camera.y=50*256;camera.sine=0;camera.cosine=65536;clear(t);CHECK(ssa_godrays_draw(&map,&camera,&ops)>0);
    memset(palette,0,sizeof palette);CHECK(ssa_godray_palette(&t->palette,palette)==1);
    for(i=0;i<256;++i)CHECK(t->palette.colors[t->palette.blend[16][i]]==0);
    free(t);
}
static void corners(void){
    uint8_t data[132]={0};uint16_t palette[256]={0};SsaMapAssets m={0};SsaGodrayCamera c={0};
    Target *t=calloc(1,sizeof(*t));SsaGodrayOps ops={span,t};unsigned i,view,y,x;
    CHECK(t);m.data=data;m.size=sizeof data;m.vertex_count=3;m.line_count=2;m.sector_count=2;m.lines=12;m.sectors=92;
    /* Two perpendicular sky/roof boundaries sharing vertex1 at(0,0). */
    put16(data+2,-128);put16(data+8,-128);
    put16(data+12+2,1);put16(data+52,1);put16(data+52+2,2);
    for(i=0;i<2;++i){put16(data+12+i*40+14,1);put16(data+12+i*40+26,4);}
    put16(data+94,256);put16(data+114,128);data[123]=1;
    for(i=0;i<32;++i)palette[i]=(uint16_t)(i|(i<<5)|(i<<10));
    CHECK(ssa_godray_palette(&t->palette,palette)==1);
    c.x=c.y=-64*256;c.z=80*256;c.sine=c.cosine=46341;c.pitch_cosine=65536;
    c.focal_x=c.focal_y=64*256;c.width=c.height=128;
    for(view=0;view<3;++view){c.x=(-64*256)+(int)view*64;clear(t);
        CHECK(ssa_godrays_draw(&m,&c,&ops)>=4);
        /* span() rejects double coverage. These rows cross the shared corner:
         * no unlit pixel may interrupt either side or the join. */
        for(y=40;y<75;++y)for(x=24;x<104;++x)CHECK(t->covered[y*128+x]);}
    free(t);
}
static void snapshot(const uint32_t *pixels,unsigned width,unsigned height,const char *path){FILE *f=fopen(path,"wb");unsigned i;uint32_t fields[13]={54+width*height*4,0,54,40,width,0u-height,0x200001,0,width*height*4,0,0,0,0};
    CHECK(f);fputs("BM",f);for(i=0;i<13;++i){unsigned n=fields[i],j;for(j=0;j<4;++j){fputc(n&255,f);n>>=8;}}
    CHECK(fwrite(pixels,4,(size_t)width*height,f)==(size_t)width*height);CHECK(!fclose(f));}
static void reference_occlusion(void){
    SsaWorldScene *world=calloc(1,sizeof(*world));SsaActorScene *scene=calloc(1,sizeof(*scene));
    SsaActorAssets *assets=calloc(1,sizeof(*assets));SsaGodrayReference *effect=calloc(1,sizeof(*effect));
    uint8_t data[144]={0},texel=8,top[120],bottom[120];uint16_t palette[256]={0};SsaMapAssets map={0};
    SsaActorImage image={0};SsaRenderCamera camera={0};uint8_t *fb;unsigned i,lit=0;size_t sample=40*120+60;
    CHECK(world && scene && assets && effect);fb=world->framebuffer+SSA_SCENE_FRAME_PREFIX;
    map.data=data;map.size=sizeof data;map.vertex_count=2;map.line_count=2;map.sector_count=2;map.segment_count=1;map.lines=8;map.sectors=88;map.segments=128;
    put16(data+6,100);put16(data+10,1);put16(data+22,1);put16(data+34,4);put16(data+90,256);put16(data+110,128);data[119]=1;put16(data+132,1);
    for(i=0;i<32;++i)palette[i]=(uint16_t)(i|(i<<5)|(i<<10));
    world->visibility.map=&map;world->visibility.head=world->visibility.tail=65535;
    scene->loaded=1;scene->geometry=world;scene->assets=assets;scene->queue.visibility=&world->visibility;
    camera.x=-64*65536;camera.y=50*65536;camera.z=104*65536;camera.sine=-65536;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1);
    for(i=0;i<14400;++i)lit+=fb[i]!=4;
    CHECK(lit>100 && fb[sample]!=4);
    /* A closer solid wall hides the entire curtain. Portal clips hide only
     * the ceiling/floor region, using the original signed opening arrays. */
    world->visibility.head=world->visibility.tail=0;world->visibility.wall_count=1;
    world->visibility.walls[0].next=65535;world->visibility.walls[0].right=119;
    world->visibility.walls[0].depth_left=world->visibility.walls[0].depth_right=0x80000;
    scene->queue.wall_clips[0].flags=3;scene->queue.wall_clips[0].bottom_height=32767;scene->queue.wall_clips[0].top_height=-32768;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1);
    for(i=0;i<14400;++i)CHECK(fb[i]==4);
    memset(top,50,sizeof top);memset(bottom,40,sizeof bottom);
    scene->queue.wall_clips[0].top=top;scene->queue.wall_clips[0].top_size=120;scene->queue.wall_clips[0].top_height=80;
    scene->queue.wall_clips[0].bottom=bottom;scene->queue.wall_clips[0].bottom_size=120;scene->queue.wall_clips[0].bottom_height=128;
    scene->queue.wall_clips[0].flags=2;memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]==4);
    scene->queue.wall_clips[0].flags=1;memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]==4);
    world->visibility.head=world->visibility.tail=65535;
    /* An actual one-pixel sprite verifies opaque, transparent, behind-light
     * and fixed-screen coverage without changing any geometry clips. */
    assets->loaded=1;assets->resources[4].images=&image;assets->resources[4].count=1;
    assets->reciprocal[4096]=1048576;assets->reciprocal[6144]=699050;
    image.width=image.height=1;image.pixels=&texel;
    scene->queue.actor_count=scene->queue.projection_count=scene->queue.draw_count=1;
    scene->queue.projections[0]=(SsaActorProjection){0,60,40,1,1,1,4096,0,0,0,0,4,0,60,1};
    scene->queue.clip_words[0]=-1;scene->queue.clip_words[1]=120;
    scene->queue.actors[0].depth=0x40000;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]==4);
    scene->queue.actors[0].depth=0x8000;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]!=4);
    scene->queue.fixed_screen_mask=1;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]==4);
    texel=0;
    memset(fb,4,14400);CHECK(ssa_godrays_reference(effect,scene,&camera,palette)==1 && fb[sample]!=4);
    free(effect);free(assets);free(scene);free(world);
}
static unsigned reference(SsaNativeGame *g){SsaRenderCamera c={0};uint8_t before[14400];unsigned i,changed=0;int32_t sine,cosine;
    c.x=g->pc_camera.x;c.y=g->pc_camera.y;c.z=g->pc_camera.z;c.heading=g->pc_camera.heading;c.sector=g->pc_camera.sector;
    ssa_sincos(&g->math,c.heading,&sine,&cosine);c.sine=-sine;c.cosine=cosine;
    CHECK(ssa_world_scene_clear(&g->world_scene,0)==1);
    CHECK(ssa_world_scene_draw(&g->world_scene,&c,0,c.heading)==1);
    CHECK(ssa_actor_scene_draw_offsets(&g->actor_scene,&c,g->frame_game.flags,g->sources,g->spawn.pool.active_count,g->source_z_offsets)==1);
    memcpy(before,ssa_world_scene_pixels(&g->world_scene),14400);
    CHECK(ssa_godrays_reference(&g->godrays,&g->actor_scene,&c,g->palette_effect.colors)==1);
    for(i=0;i<14400;++i){unsigned pixel=ssa_world_scene_pixels(&g->world_scene)[i];changed+=pixel!=before[i];
        if(!g->godrays.actor_depth[i])CHECK(pixel==before[i]);}
    CHECK(g->godrays.triangles>0);return changed;
}
static void reflections(SsaNativeGame *g){
    SsaWaterRender *w=calloc(1,sizeof(*w));SsaPcRenderer *mirror=NULL;
    uint32_t *out=calloc(240*160,sizeof(*out)),*baseline=calloc(240*160,sizeof(*baseline));
    uint8_t saved[17][256],indexed[120*120];unsigned i,s,changed=0;
    CHECK(w && out && baseline);g->godrays_enabled=1;g->surface_decals_enabled=0;
    CHECK(ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
        g->pc_camera.x/65536.0,g->pc_camera.y/65536.0,g->pc_camera.z/65536.0,0,1,1,240,160)==1);
    /* Synthetic full-water stencil isolates the reflected light pass from
     * real-map water placement and from the main camera's light pass. */
    for(i=0;i<240*160;++i)w->mask[i]=UINT32_C(0xff010000);
    memcpy(saved,g->godrays.palette.blend,sizeof saved);
    for(s=0;s<17;++s)for(i=0;i<256;++i)g->godrays.palette.blend[s][i]=(uint8_t)i;
    CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,baseline,NULL)==1);
    memcpy(g->godrays.palette.blend,saved,sizeof saved);
    CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,out,NULL)==1 && mirror->godray_triangles>0);
    for(i=0;i<240*160;++i)changed+=out[i]!=baseline[i];
    CHECK(changed>20);
    /* The same pass also feeds indexed GBA reflection composition. */
    CHECK(ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
        g->pc_camera.x/65536.0,g->pc_camera.y/65536.0,g->pc_camera.z/65536.0,0,1,1,120,120)==1);
    for(i=0;i<120*120;++i)w->mask[i]=UINT32_C(0xff010000);
    memset(indexed,0,sizeof indexed);g->gba_view=1;
    CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,NULL,indexed)==1 && mirror->godray_triangles>0);
    g->godrays_enabled=0;g->surface_decals_enabled=1;
    CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,NULL,indexed)==1 && !mirror->godray_triangles);
    g->gba_view=0;ssa_pc_renderer_free(mirror);free(mirror);ssa_water_free(w);free(w);free(out);free(baseline);
}
int main(int argc,char **argv){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};uint8_t *rom=malloc(0x800000);FILE *f;unsigned i,visible=0,gba_changed;
    dithering();isolated();corners();reference_occlusion();CHECK(argc>=2 && g && rom);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_native_game_step(g)==1);
    CHECK(ssa_native_game_prepare_view(g)==1);CHECK(godray_fixture_camera(g));
    reflections(g);
    gba_changed=reference(g);CHECK(gba_changed>20);
    if(argc>4){uint32_t pixels[14400];for(i=0;i<14400;++i){unsigned c=g->palette_effect.colors[ssa_world_scene_pixels(&g->world_scene)[i]];
        pixels[i]=((c&31)*255/31<<16)|(((c>>5)&31)*255/31<<8)|((c>>10)&31)*255/31;}
        snapshot(pixels,120,120,argv[4]);}
    CHECK(ssa_pc_renderer_resize(&r,480,320)==1);g->godrays_enabled=1;g->surface_decals_enabled=0;
    for(i=0;i<4;++i){g->render_pitch=i==1?4096:i==2?-4096:0;
        if(i==3){int32_t sine,cosine;ssa_sincos(&g->math,g->pc_camera.heading,&sine,&cosine);
            g->pc_camera.x+=sine*96;g->pc_camera.y+=cosine*96;g->pc_camera.sector=(int16_t)ssa_map_sector(&g->map,g->pc_camera.x,g->pc_camera.y);
            CHECK(g->pc_camera.sector>=0);g->pc_camera.heading=(uint16_t)(g->pc_camera.heading+32768);}
        CHECK(ssa_pc_scene_draw(&r,g)==1);visible+=r.godray_triangles>0;
        if(argc>2 && !i){snapshot(r.pixels,r.width,r.height,argv[2]);}
        if(argc>3 && i==3){snapshot(r.pixels,r.width,r.height,argv[3]);}}
    CHECK(visible);g->godrays_enabled=0;g->surface_decals_enabled=1;CHECK(ssa_pc_scene_draw(&r,g)==1 && !r.godray_triangles);
    g->godrays_enabled=1;g->surface_decals_enabled=0;CHECK(ssa_native_game_draw(g)==1);
    ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"real_views_with_shafts\":%u,\"gba_changed_pixels\":%u}\n",checks,visible,gba_changed);return 0;
}
