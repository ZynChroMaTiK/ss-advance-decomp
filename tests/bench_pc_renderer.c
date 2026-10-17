/* Fixed scene CPU-time benchmark. Same executable source before/after changes;
 * timings are local measurements, not a hardware-independent FPS guarantee. */
#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(int argc,char **argv){
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};uint8_t *rom=malloc(0x800000);
    FILE *f;unsigned i,j,count=24,w=1280,h=720;static const int pitches[]={0,5440,-5440};
    if(argc<2 || !g || !rom)return 1;
    if(argc>2)count=(unsigned)atoi(argv[2]);
    if(argc>4){w=(unsigned)atoi(argv[3]);h=(unsigned)atoi(argv[4]);}
    f=fopen(argv[1],"rb");if(!f || fread(rom,1,0x800000,f)!=0x800000)return 1;fclose(f);
    if(ssa_native_game_init(g,rom,0x800000,0)!=1 || ssa_native_game_step(g)!=1 ||
       ssa_pc_renderer_resize(&r,w,h)!=1)return 1;
    printf("{\"width\":%u,\"height\":%u,\"frames_per_case\":%u,\"cases\":[",w,h,count);
    for(j=0;j<8;++j){clock_t start;double seconds;int moving=j>=3 && j<6,sweep=j>=6;
        g->render_pitch=sweep?(j==6?1000:-1000):pitches[j%3];
        if(ssa_pc_renderer_draw(&r,g)!=1)return 1;
        start=clock();
        for(i=0;i<count;++i){if(moving)g->pc_camera.heading=(uint16_t)(i*107);
            if(sweep)g->render_pitch=(j==6?1:-1)*(1000+(int)(i%24)*317);
            if(ssa_pc_renderer_draw(&r,g)!=1)return 1;}
        seconds=(double)(clock()-start)/CLOCKS_PER_SEC;
        printf("%s{\"pitch\":%d,\"turning\":%s,\"vertical_sweep\":%s,\"ms_per_frame\":%.6f,\"fps\":%.3f}",j?",":"",
            g->render_pitch,moving?"true":"false",sweep?"true":"false",seconds*1000/count,count/seconds);
    }
    puts("]}");ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);free(rom);return 0;
}
