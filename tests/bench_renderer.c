#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
static double seconds(void){
#ifdef _WIN32
    LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return (double)n.QuadPart/f.QuadPart;
#else
    return (double)clock()/CLOCKS_PER_SEC;
#endif
}
static uint64_t digest(const void *data,size_t size){
    const unsigned char *p=data;uint64_t h=UINT64_C(14695981039346656037);
    while(size--)h=(h^*p++)*UINT64_C(1099511628211);return h;
}
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"benchmark line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);FILE *f;unsigned level,mode,sample,frame;
    REQUIRE(argc>1 && rom);f=fopen(argv[1],"rb");REQUIRE(f);REQUIRE(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    puts("[");
    for(level=0;level<=6;level+=6){
        SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};unsigned i,water=1;
        REQUIRE(g && ssa_native_game_init(g,rom,0x800000,level)==1 && ssa_native_game_step(g)==1);
        REQUIRE(ssa_pc_renderer_resize(&r,960,640)==1);
        while(water<256 && !g->surface_decals.water_floor[water])++water;REQUIRE(water<256);
        for(mode=0;mode<6;++mode){double times[3],t;uint64_t pixels,depth;
            /* Water fixtures change materials only, not gameplay or geometry. */
            if(mode==2)for(i=0;i<g->map.sector_count;++i)g->map.data[g->map.sectors+i*20+10]=(uint8_t)water;
            g->surface_decals_enabled=mode>=2;g->water_reflections=mode>=2;
            g->render_pitch=mode%2?-4096:0;
            if(mode==4)REQUIRE(ssa_pc_renderer_resize(&r,1920,1080)==1);
            REQUIRE(ssa_pc_renderer_draw(&r,g)==1);
            for(sample=0;sample<3;++sample){t=seconds();
                for(frame=0;frame<12;++frame){g->frame_game.counter=frame*7;REQUIRE(ssa_pc_renderer_draw(&r,g)==1);}
                times[sample]=(seconds()-t)*1000/12;}
            if(times[0]>times[1]){t=times[0];times[0]=times[1];times[1]=t;}
            if(times[1]>times[2]){t=times[1];times[1]=times[2];times[2]=t;}
            if(times[0]>times[1])times[1]=times[0];
            pixels=digest(r.pixels,(size_t)r.width*r.height*4);depth=digest(r.depth,(size_t)r.width*r.height*sizeof(float));
            printf("%s{\"level\":%u,\"mode\":%u,\"width\":%u,\"height\":%u,\"ms\":%.4f,\"pixels\":\"%016llx\",\"depth\":\"%016llx\",\"reflection_passes\":%u}",
                level||mode?",\n":"",level,mode,r.width,r.height,times[1],(unsigned long long)pixels,(unsigned long long)depth,r.water.passes);
        }
        ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);
    }
    puts("\n]");free(rom);return 0;
}
