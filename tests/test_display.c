#include "ssa/display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,guards,frames;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"display line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static SsaDisplayRegisters r;
static SsaSpriteOam o;
static SsaSpriteAssets assets;
static SsaMathTables math;
static uint8_t world[14400],tiles[32768];
static uint16_t output[38400],before[38400];
static uint32_t rgb[38400];
static int compose(void)
{return ssa_display_compose(&r,world,sizeof(world),assets.background_palette[0],assets.palette,&o,tiles,sizeof(tiles),output,38400);}
static void reject(int expected)
{
    memcpy(before,output,sizeof(before));CHECK(compose()==expected);
    CHECK(!memcmp(before,output,sizeof(before)));++guards;
}
static void hide(void)
{
    unsigned i;memset(&o,0,sizeof(o));o.loaded=1;
    for(i=0;i<128;++i){o.entries[i].attribute0=0x200;o.entries[i].affine=(i%4==0||i%4==3)?256:0;}
}
int main(int argc,char **argv)
{
    FILE *f;uint8_t *rom;SsaSpriteDispatch dispatch={0};SsaWeaponObject objects[84]={0};
    SsaSpriteCacheEntry cache[311]={0};unsigned i,j,n;uint16_t old;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f!=NULL);rom=malloc(8u*1024u*1024u);CHECK(rom!=NULL);
    CHECK(fread(rom,1,8u*1024u*1024u,f)==8u*1024u*1024u);CHECK(!fclose(f));
    CHECK(ssa_sprite_assets_load(&assets,rom,8u*1024u*1024u)==1);
    CHECK(ssa_math_tables_load(&math,rom,8u*1024u*1024u));
    CHECK(ssa_sprite_oam_init(&o,rom,8u*1024u*1024u)==1);free(rom);
    CHECK(ssa_display_registers_default(&r)==1);
    for(i=0;i<14400;++i)world[i]=(uint8_t)(i*37);
    for(i=0;i<84;++i)dispatch.objects[i]=&objects[i];
    dispatch.active_count=1;
    for(i=0;i<311;++i) {
        SsaWeaponObject *p=&objects[0];const SsaSpriteImage *im=&assets.images[i];
        p->type=1;p->flags=5;p->previous_frame=(uint16_t)i;p->frame=(uint16_t)i;
        p->scale_x=p->scale_y=4096;p->x=120*256;p->y=80*256;p->layer=15;
        cache[i].resident=1;cache[i].tile=160;
        for(j=0;j<(unsigned)im->tile_count*64;++j)tiles[(160*64+j)&32767]=im->tiles[j];
        CHECK(ssa_sprite_oam_build(&o,&dispatch,&assets,cache,&math)==1);
        for(n=0;n<4;++n) {
            for(j=0;j<o.count;++j)o.entries[j].attribute1=(uint16_t)((o.entries[j].attribute1&0xcfff)|(n<<12));
            CHECK(compose()==1);CHECK(ssa_display_rgb888(output,38400,rgb,38400)==1);++frames;
            for(j=0;j<38400;j+=127) {
                unsigned red=output[j]&31,green=(output[j]>>5)&31,blue=(output[j]>>10)&31;
                CHECK(rgb[j]==(((red*8+red/4)<<16)|((green*8+green/4)<<8)|(blue*8+blue/4)));
            }
        }
    }
    hide();CHECK(compose()==1);
    for(i=0;i<38400;++i)CHECK(output[i]==(assets.background_palette[0][world[((i/240)*3/4)*120+(i%240)/2]]&32767));
    for(i=0;i<7;++i)if(i!=1){r.dispcnt=(uint16_t)(0x1440|i);reject(-2);}
    r.dispcnt=0x1441;
    r.dispcnt|=0x2000;reject(-2);r.dispcnt=0x1441;
    old=r.bg2cnt;r.bg2cnt=2;reject(-2);r.bg2cnt=old;
    r.bldcnt=0x40;reject(-2);r.bldcnt=0x80;reject(-2);r.bldcnt=0xc0;reject(-2);r.bldcnt=0;
    r.x=134217728;reject(-1);r.x=0;r.y=-134217729;reject(-1);r.y=0;
    o.loaded=0;reject(-1);o.loaded=1;o.updating=1;reject(-1);o.updating=0;
    o.count=129;reject(-1);o.count=0;o.affine_count=33;reject(-1);o.affine_count=0;
    o.entries[0].attribute0=0x2000|0xc000;reject(-1);
    o.entries[0].attribute0=0x2000|0xc00;reject(-1);
    o.entries[0].attribute0=0;reject(-2);
    o.entries[0].attribute0=0x2400;reject(-2);
    o.entries[0].attribute0=0x2800;reject(-2);
    o.entries[0].attribute0=0x3000;r.mosaic=0x1100;reject(-2);r.mosaic=0;
    CHECK(compose()==1);o.entries[0].attribute0=0x2000;o.entries[0].attribute2=0x800;
    o.entries[1].attribute0=0x2000;o.entries[1].attribute2=0x400;reject(-2);
    hide();for(i=0;i<20;++i){o.entries[i].attribute0=0x2000;o.entries[i].attribute1=0xc000;}reject(-2);
    hide();
#define BAD(call) do{memcpy(before,output,sizeof(before));CHECK((call)==-1);CHECK(!memcmp(before,output,sizeof(before)));++guards;}while(0)
    BAD(ssa_display_compose(NULL,world,14400,assets.palette,assets.palette,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,NULL,14400,assets.palette,assets.palette,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14399,assets.palette,assets.palette,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14400,NULL,assets.palette,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14400,assets.palette,NULL,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14400,assets.palette,assets.palette,&o,NULL,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14400,assets.palette,assets.palette,&o,tiles,32767,output,38400));
    BAD(ssa_display_compose(&r,world,14400,assets.palette,assets.palette,&o,tiles,32768,output,38399));
    BAD(ssa_display_compose(&r,(uint8_t *)output,14400,assets.palette,assets.palette,&o,tiles,32768,output,38400));
    BAD(ssa_display_compose(&r,world,14400,output,assets.palette,&o,tiles,32768,output,38400));
    CHECK(ssa_display_rgb888(output,38400,(uint32_t *)output,38400)==-1);++guards;
    CHECK(ssa_display_rgb888(NULL,38400,rgb,38400)==-1);++guards;
    CHECK(ssa_display_rgb888(output,38400,NULL,38400)==-1);++guards;
    CHECK(ssa_display_rgb888(output,38399,rgb,38400)==-1);++guards;
    CHECK(ssa_display_rgb888(output,38400,rgb,38399)==-1);++guards;
    for(i=0;i<64;++i) {
        r.pa=(int16_t)(i*997);r.pb=(int16_t)(i*641);r.pc=(int16_t)(i*1231);r.pd=(int16_t)(i*641);
        r.x=(i&1)?134217727:-134217728;r.y=(i&2)?134217727:-134217728;
        CHECK(compose()==1);++frames;
    }
    ssa_sprite_assets_free(&assets);
    printf("{\"assertions\":%u,\"guards\":%u,\"native_frames\":%u,\"real_images\":311}\n",checks,guards,frames);
    return 0;
}
