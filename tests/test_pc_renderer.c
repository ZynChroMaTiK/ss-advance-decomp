#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
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
int main(int argc,char **argv){
 SsaNativeGame *g=calloc(1,sizeof(*g)),*copy=malloc(sizeof(*copy));SsaPcRenderer r={0};FILE *f;uint8_t *rom=malloc(0x800000);
 static const unsigned sizes[][2]={{1,1},{127,73},{240,160},{960,640},{1920,1080},{3840,2160},{8193,17}};
 unsigned i,changed=0;uint32_t *old;
 CHECK(argc>=2 && g && copy && rom);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
 CHECK(ssa_native_game_init(g,rom,0x800000,0)==1);CHECK(ssa_native_game_step(g)==1);
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
 ssa_pc_input_runtime_key(&g->input_runtime,'H',1,0);ssa_pc_input_runtime_key(&g->input_runtime,'H',0,0);
 ssa_pc_input_runtime_key(&g->input_runtime,' ',1,0);
 for(i=0;i<24;++i){CHECK(ssa_native_game_step(g)==1);CHECK(ssa_pc_renderer_draw(&r,g)==1);CHECK(r.actor_pixels>0);}
 if(argc>3)snapshot(&r,argv[3]);
 if(argc>4){SsaPcRenderer legacy={0};legacy.width=240;legacy.height=160;legacy.pixels=g->rgb888;snapshot(&legacy,argv[4]);}
 ssa_pc_renderer_free(&r);CHECK(!r.pixels && !r.depth && !r.hits);ssa_native_game_free(g);free(copy);free(g);free(rom);
 printf("{\"result\":\"pass\",\"checks\":%u,\"sizes\":7,\"different_from_upscale\":%u,\"chase_jump_frames\":24}\n",checks,changed);return 0;
}
