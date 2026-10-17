#include "ssa/actor_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;} } while(0)
int main(int argc,char **argv) {
    FILE *f;long length;uint8_t *rom,*fb,*before;SsaActorAssets *a;
    unsigned g,i;int16_t clips[240];SsaActorProjection p;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f!=NULL);
    CHECK(fseek(f,0,SEEK_END)==0);length=ftell(f);CHECK(length>0);rewind(f);
    rom=(uint8_t *)malloc((size_t)length);a=(SsaActorAssets *)calloc(1,sizeof(*a));
    fb=(uint8_t *)malloc(14400);before=(uint8_t *)malloc(14400);CHECK(rom&&a&&fb&&before);
    CHECK(fread(rom,1,(size_t)length,f)==(size_t)length);CHECK(fclose(f)==0);
    CHECK(ssa_actor_assets_load(a,rom,(size_t)length)==1);free(rom);
    for(i=0;i<120;i++){clips[2*i]=0;clips[2*i+1]=120;}
    for(g=0;g<SSA_ACTOR_RESOURCE_COUNT;g++)for(i=1;i<=a->resources[g].count;i++) {
        const SsaActorImage *im=ssa_actor_assets_image(a,g,i);unsigned variant;
        CHECK(im!=NULL);
        for(variant=0;variant<2;variant++) {
            memset(&p,0,sizeof(p));p.animation=(uint16_t)g;p.frame_flags=i|(variant?0x80000000u:0);
            p.scale=4096;p.width=(int16_t)im->width;p.clipped_width=p.width;
            p.height=(int16_t)(((uint32_t)im->height<<12)/(a->reciprocal[6144]>>8));
            memset(fb,0x55,14400);
            CHECK(ssa_actor_raster_draw(a,&p,clips,240,fb,14400)==1);
            memcpy(before,fb,14400);p.left=-1;
            CHECK(ssa_actor_raster_draw(a,&p,clips,240,fb,14400)==-1);
            CHECK(memcmp(before,fb,14400)==0);
        }
    }
    CHECK(ssa_actor_assets_image(a,142,1)==NULL);
    CHECK(ssa_actor_assets_image(a,4,0)==NULL);
    CHECK(ssa_actor_raster_draw(NULL,&p,clips,240,fb,14400)==-1);
    ssa_actor_assets_free(a);ssa_actor_assets_free(a);free(a);free(fb);free(before);
    printf("{\"asan_ubsan_assertions\":%u}\n",checks);return 0;
}
