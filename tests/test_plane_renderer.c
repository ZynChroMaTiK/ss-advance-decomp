#include "ssa/plane_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    SsaPlanePool pool;SsaPlaneRenderer r={0};SsaSkyAssets *sky;FILE *f;long length;
    unsigned char *rom,fb[14400];unsigned level,x,y;SsaPlane *p;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);length=ftell(f);CHECK(length>0);rewind(f);
    rom=(unsigned char *)malloc((size_t)length);sky=(SsaSkyAssets *)calloc(1,sizeof(*sky));CHECK(rom && sky);
    CHECK(fread(rom,1,(size_t)length,f)==(size_t)length);fclose(f);
    CHECK(ssa_sky_assets_load(sky,rom,(size_t)length)==1);free(rom);
    for(level=0;level<16;++level) {
        CHECK(ssa_plane_pool_init(&pool,level)==1);CHECK(pool.capacity==(level==2?80u:64u));
        p=ssa_plane_find(&pool,100,0);CHECK(p!=NULL);CHECK(ssa_plane_check(&pool,p,0,119)==p);
        for(x=0;x<120;++x){p->top[x+1]=0;p->bottom[x+1]=59;}
        memset(fb,199,sizeof(fb));CHECK(ssa_plane_render(&r,&pool,sky,level,65535,20,fb,sizeof(fb))==1);
        CHECK(r.sky_offset==639 && r.height_distance==80);
        for(y=0;y<60;++y)for(x=0;x<120;++x) {
            unsigned family=level<=5 || level==12 || level==13?0:1;
            CHECK(fb[y*120+x]==sky->pixels[family][y*768+639+x]);
        }
        CHECK(fb[14399]==199);CHECK(ssa_plane_pool_reset(&pool)==1);
        p=ssa_plane_find(&pool,-40,42);CHECK(p!=NULL);CHECK(ssa_plane_check(&pool,p,0,119)==p);
        for(x=0;x<120;++x){p->top[x+1]=60;p->bottom[x+1]=119;}
        CHECK(ssa_plane_render(&r,&pool,sky,level,0,20,fb,sizeof(fb))==1);
        for(y=60;y<120;++y)for(x=0;x<120;++x)CHECK(fb[y*120+x]==42);
    }
    free(sky);printf("{\"sanitizer_assertions\":%u}\n",checks);return 0;
}
