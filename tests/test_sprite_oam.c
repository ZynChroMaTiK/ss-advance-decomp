#include "ssa/sprite_oam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,frames;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;} } while(0)
int main(int argc,char **argv) {
    FILE *f;long length;uint8_t *rom;SsaSpriteAssets *assets;SsaMathTables *math;
    SsaSpriteOam o={0},before;SsaSpriteDispatch d={0};SsaWeaponObject objects[84]={0},saved[84];
    SsaSpriteCacheEntry cache[311]={0};unsigned image,variant,i,frame;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f!=NULL);
    CHECK(fseek(f,0,SEEK_END)==0);length=ftell(f);CHECK(length>0);rewind(f);
    rom=(uint8_t *)malloc((size_t)length);assets=(SsaSpriteAssets *)calloc(1,sizeof(*assets));
    math=(SsaMathTables *)malloc(sizeof(*math));CHECK(rom&&assets&&math);
    CHECK(fread(rom,1,(size_t)length,f)==(size_t)length);CHECK(fclose(f)==0);
    CHECK(ssa_sprite_assets_load(assets,rom,(size_t)length)==1);
    CHECK(ssa_math_tables_load(math,rom,(size_t)length)==1);
    CHECK(ssa_sprite_oam_init(&o,rom,(size_t)length)==1);free(rom);
    for(i=0;i<84;i++){d.objects[i]=&objects[i];d.active[i]=i;}
    for(i=0;i<311;i++){cache[i].resident=3;cache[i].tile=160;}
    for(image=0;image<311;image++)for(variant=0;variant<8;variant++) {
        SsaWeaponObject *p=&objects[0];d.active_count=1;
        memset(p,0,sizeof(*p));p->flags=5;p->previous_frame=(uint16_t)(image|((variant&3u)<<14));
        p->x=40*256;p->y=80*256;p->scale_x=(uint16_t)(variant&4?8192:4096);p->scale_y=4096;
        p->unknown2c=(uint16_t)(variant*8191);p->layer=(uint8_t)(variant*2);
        CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==1);frames++;
        CHECK(o.count<=128&&o.affine_count<=32&&!o.updating&&!(o.flags&1));
    }
    for(frame=0;frame<2048;frame++) {
        d.active_count=84;
        for(i=0;i<84;i++) {
            SsaWeaponObject *p=&objects[i];memset(p,0,sizeof(*p));
            p->previous_frame=(uint16_t)(((i+frame)%311)|((frame&3u)<<14));p->flags=5;
            p->x=(int32_t)((i%16)*4096);p->y=(int32_t)((frame%256)*256)-16384;
            p->scale_x=(uint16_t)((frame%5)*4096);p->scale_y=(uint16_t)(4096+(i%3)*4096);
            p->unknown2c=(uint16_t)(frame*31);p->layer=(uint8_t)(i%16);
        }
        CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==1);frames++;
        CHECK(o.count<=128&&o.affine_count<=32&&!o.updating);
        before=o;memcpy(saved,objects,sizeof(saved));d.active_count=85;
        CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==-1);
        CHECK(memcmp(&before,&o,sizeof(o))==0&&memcmp(saved,objects,sizeof(saved))==0);
    }
    d.active_count=1;objects[0].layer=16;before=o;memcpy(saved,objects,sizeof(saved));
    CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==-1);
    CHECK(memcmp(&before,&o,sizeof(o))==0&&memcmp(saved,objects,sizeof(saved))==0);
    objects[0].layer=0;d.updating=1;
    CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==-1);d.updating=0;o.updating=1;
    CHECK(ssa_sprite_oam_build(&o,&d,assets,cache,math)==-1);o.updating=0;
    CHECK(ssa_sprite_oam_build(NULL,&d,assets,cache,math)==-1);
    ssa_sprite_assets_free(assets);free(assets);free(math);
    printf("{\"asan_ubsan_assertions\":%u,\"sanitizer_frames\":%u}\n",checks,frames);return 0;
}
