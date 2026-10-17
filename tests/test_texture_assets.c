#include "ssa/texture_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    FILE *file;long length;unsigned f,m,i;unsigned char *rom;SsaTextureAssets a={0},before;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file!=NULL);
    CHECK(fseek(file,0,SEEK_END)==0);length=ftell(file);CHECK(length>0);rewind(file);
    rom=(unsigned char *)malloc((size_t)length);CHECK(rom!=NULL);
    CHECK(fread(rom,1,(size_t)length,file)==(size_t)length);fclose(file);
    CHECK(ssa_texture_assets_load(&a,rom,(size_t)length)==1);free(rom);
    for(f=0;f<2;++f)for(m=0;m<3;++m) {
        const SsaTextureArchive *ar=&a.families[f].archives[m];
        for(i=0;i<ar->count;++i) {
            const SsaTextureImage *im=ssa_texture_assets_image(&a,f,i,m);
            size_t bytes=(size_t)im->width*im->height;unsigned char *raster=(unsigned char *)malloc(bytes);
            CHECK(im==&ar->images[i]);CHECK(im->pixels!=NULL);
            CHECK(ssa_texture_archive_find(ar,im->name)==(int)i);
            CHECK(im->pixels[(size_t)im->width*im->height-1]<=255);
            CHECK(raster!=NULL);CHECK(ssa_texture_image_indices(im,raster,im->width,bytes)==1);
            CHECK(raster[bytes-1]==im->pixels[bytes-1]);free(raster);
        }
    }
    before=a;CHECK(ssa_texture_assets_load(&a,"",0)==-1);CHECK(memcmp(&a,&before,sizeof(a))==0);
    CHECK(ssa_texture_assets_load(NULL,"",0)==-1);CHECK(ssa_texture_archive_find(NULL,"ABCDEFGH")==-1);
    CHECK(ssa_texture_archive_find(&a.families[0].archives[0],NULL)==-1);
    CHECK(ssa_texture_assets_image(NULL,0,0,0)==NULL);
    ssa_texture_assets_free(&a);ssa_texture_assets_free(&a);ssa_texture_assets_free(NULL);
    CHECK(!a.loaded);CHECK(ssa_texture_assets_image(&a,0,0,0)==NULL);
    printf("{\"sanitizer_assertions\":%u}\n",checks);return 0;
}
