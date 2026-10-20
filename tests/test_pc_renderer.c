#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include "ssa/pc_scene.h"
#include "ssa/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pc renderer line%d: %s\n",__LINE__,#x);abort();}++checks;}while(0)
static void put32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static void snapshot(const SsaPcRenderer *r,const char *path){
 uint8_t h[54]={0};FILE *f=fopen(path,"wb");CHECK(f);h[0]='B';h[1]='M';put32(h+2,54+r->width*r->height*4);
 put32(h+10,54);put32(h+14,40);put32(h+18,r->width);put32(h+22,0u-r->height);h[26]=1;h[28]=32;
 CHECK(fwrite(h,1,54,f)==54);CHECK(fwrite(r->pixels,4,(size_t)r->width*r->height,f)==(size_t)r->width*r->height);CHECK(!fclose(f));}
static void background_fov(SsaNativeGame *g)
{
 SsaPcRenderer r={0};static const unsigned sizes[][2]={{240,160},{427,240}};
 static const int pitches[]={0,4096,-2048};int pitch=g->render_pitch;
 unsigned fov=g->fov_offset,heading=g->pc_camera.heading,size,mode,offset;
 unsigned family=ssa_texture_assets_family(g->level),other=1;size_t sky_bytes=sizeof(g->sky.pixels[0]),s;
 uint8_t *art=malloc(sky_bytes),*marker=malloc(sky_bytes);CHECK(art && marker);
 memcpy(art,g->sky.pixels[family],sky_bytes);
 while(other<256 && (g->palette_effect.colors[other]&0x7fff)==(g->palette_effect.colors[0]&0x7fff))++other;
 CHECK(other<256);
 /* An altered sky pass identifies background pixels even when pitched sky
  * polygons write finite depth. Every changed texel has a different RGB color. */
 for(s=0;s<sky_bytes;++s)marker[s]=(uint8_t)((g->palette_effect.colors[art[s]]&0x7fff)==
     (g->palette_effect.colors[0]&0x7fff)?other:0);
 for(size=0;size<2;++size){size_t n=(size_t)sizes[size][0]*sizes[size][1],p;
  uint32_t *pixels=malloc(n*sizeof(*pixels)),*current=malloc(n*sizeof(*current));
  uint8_t *mask=malloc(n);CHECK(pixels && current && mask);
  CHECK(ssa_pc_renderer_resize(&r,sizes[size][0],sizes[size][1])==1);
  for(mode=0;mode<3;++mode){g->render_pitch=pitches[mode];g->pc_camera.heading=(uint16_t)(heading+mode*4096);g->fov_offset=10;
   CHECK(ssa_pc_scene_draw(&r,g)==1);memcpy(pixels,r.pixels,n*sizeof(*pixels));
   memcpy(g->sky.pixels[family],marker,sky_bytes);CHECK(ssa_pc_scene_draw(&r,g)==1);
   for(p=0;p<n;++p)mask[p]=(uint8_t)(pixels[p]!=r.pixels[p]);
   memcpy(g->sky.pixels[family],art,sky_bytes);
   for(offset=0;offset<=60;offset+=30){unsigned sky=0;g->fov_offset=offset;
    CHECK(ssa_pc_scene_draw(&r,g)==1);memcpy(current,r.pixels,n*sizeof(*current));
    memcpy(g->sky.pixels[family],marker,sky_bytes);CHECK(ssa_pc_scene_draw(&r,g)==1);
    /* Geometry may reveal/cover different sky pixels. Every pixel still
     * visible as background in both projections must retain its artwork. */
    for(p=0;p<n;++p)if(mask[p] && current[p]!=r.pixels[p]){CHECK(pixels[p]==current[p]);++sky;}
    memcpy(g->sky.pixels[family],art,sky_bytes);
    CHECK(sky>100);
   }
  }
  free(mask);free(current);free(pixels);
 }
 free(marker);free(art);g->render_pitch=pitch;g->fov_offset=fov;g->pc_camera.heading=(uint16_t)heading;ssa_pc_renderer_free(&r);
}
int main(int argc,char **argv){
 SsaNativeGame *g=calloc(1,sizeof(*g)),*copy=malloc(sizeof(*copy));SsaPcRenderer r={0};FILE *f;uint8_t *rom=malloc(0x800000);
 static const unsigned sizes[][2]={{1,1},{127,73},{240,160},{960,640},{1920,1080},{3840,2160},{8193,17}};
 unsigned i,changed=0,third=0,fallbacks=0;uint32_t *old;
 CHECK(argc>=2 && g && copy && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
 CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);CHECK(ssa_native_game_step(g)==1);
 background_fov(g);
 for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i){
  size_t p;CHECK(ssa_pc_renderer_resize(&r,sizes[i][0],sizes[i][1])==1);*copy=*g;
  CHECK(ssa_pc_renderer_draw(&r,g)==1);CHECK(!memcmp(g,copy,sizeof(*g)));CHECK(r.rays==r.width);
  if(r.width>=240)CHECK(r.wall_pixels>0);
  for(p=0;p<(size_t)r.width*r.height;++p)CHECK(!(r.pixels[p]&0xff000000));
  if(r.width==960){
   for(p=0;p<(size_t)r.width*r.height;++p){unsigned x=(unsigned)(p%r.width),y=(unsigned)(p/r.width);
    if(r.pixels[p]!=g->rgb888[(y/4)*240+x/4])++changed;}
   CHECK(changed>10000);if(argc>2)snapshot(&r,argv[2]);
  }
 }
 old=r.pixels;CHECK(ssa_pc_renderer_resize(&r,0,10)==-1);CHECK(r.pixels==old);
 CHECK(ssa_pc_renderer_resize(&r,UINT_MAX,UINT_MAX)==-1);CHECK(r.pixels==old);
 CHECK(ssa_pc_renderer_resize(&r,960,640)==1);
 /* New camera, jump and third person use live gameplay, never a stale capture. */
 ssa_pc_input_runtime_focus(&g->input_runtime,1);ssa_pc_input_runtime_enable(&g->input_runtime,1);
 /* Move away from the entrance wall before requesting the chase camera. */
 ssa_pc_input_runtime_key(&g->input_runtime,'W',1,0);
 for(i=0;i<30;++i)CHECK(ssa_native_game_step(g)==1);
 ssa_pc_input_runtime_key(&g->input_runtime,'W',0,0);
 ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
 ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0);
 for(i=0;i<24;++i){CHECK(ssa_native_game_step(g)==1);CHECK(ssa_pc_renderer_draw(&r,g)==1);
  CHECK(g->player.view.third_person);
  if(g->player.pose.third_person){CHECK(r.actor_pixels>0);++third;}else ++fallbacks;}
 CHECK(third>0);
 if(argc>3)snapshot(&r,argv[3]);
 if(argc>4){SsaPcRenderer legacy={0};legacy.width=240;legacy.height=160;legacy.pixels=g->rgb888;snapshot(&legacy,argv[4]);}
 /* Changing FOV refreshes flat and pitched views immediately, and returning
  * to70 reproduces every original pixel (including the fixed HUD/weapon). */
 {unsigned mode,offset;int32_t pitch=g->render_pitch;size_t count=(size_t)r.width*r.height,p;
  uint32_t *baseline=malloc(count*sizeof(*baseline));CHECK(baseline);
  CHECK(g->fov_offset==SSA_FOV_DEFAULT_OFFSET && ssa_pc_scene_fov_scale(10)==1.0);
  CHECK(ssa_pc_scene_fov_scale(0)>1.0 && ssa_pc_scene_fov_scale(60)<1.0);
  for(mode=0;mode<2;++mode){g->render_pitch=mode?5440:0;g->fov_offset=10;
   CHECK(ssa_pc_renderer_draw(&r,g)==1);memcpy(baseline,r.pixels,count*sizeof(*baseline));
   for(offset=0;offset<=60;offset+=60){unsigned different=0;g->fov_offset=offset;*copy=*g;
    CHECK(ssa_pc_renderer_draw(&r,g)==1 && !memcmp(copy,g,sizeof(*g)));
    for(p=0;p<count;++p)different+=baseline[p]!=r.pixels[p];
    CHECK(different>1000);}
   g->fov_offset=10;CHECK(ssa_pc_renderer_draw(&r,g)==1 && !memcmp(baseline,r.pixels,count*sizeof(*baseline)));}
  g->render_pitch=pitch;free(baseline);}
 /* The high-resolution weapon/HUD path shares the original compositor. A
  * transparent priority0 piece promotes the first piece over the third. */
 CHECK(ssa_pc_renderer_resize(&r,240,160)==1);
 /* This fixture supplies one synthetic OAM, rather than live anchored HUD
  * groups. Select that snapshot and suppress the separately drawn HUD. */
 g->hud_suppressed=1;
 for(i=0;i<128;++i)g->oam.entries[i].attribute0=0x200;
 memset(g->cache.tiles,0,sizeof(g->cache.tiles));memset(g->cache.tiles,1,64);memset(g->cache.tiles+128,2,64);
 for(i=0;i<3;++i){static const unsigned priorities[3]={3,0,1};
  g->oam.entries[i].attribute0=0x2000|60;g->oam.entries[i].attribute1=100;
  g->oam.entries[i].attribute2=(uint16_t)(i*2+(priorities[i]<<10));}
 CHECK(ssa_pc_renderer_draw(&r,g)==1);
 {unsigned c=g->sprite_assets.palette[1],red=c&31,green=(c>>5)&31,blue=(c>>10)&31;
  CHECK(r.pixels[60*240+100]==(((red*8+red/4)<<16)|((green*8+green/4)<<8)|(blue*8+blue/4)));}
 ssa_pc_renderer_free(&r);CHECK(!r.pixels && !r.depth && !r.hits);ssa_native_game_free(g);free(copy);free(g);free(rom);
 printf("{\"result\":\"pass\",\"checks\":%u,\"sizes\":7,\"different_from_upscale\":%u,\"chase_jump_frames\":24,\"third_person_frames\":%u,\"collision_fallback_frames\":%u}\n",checks,changed,third,fallbacks);return 0;
}
