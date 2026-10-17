#include "ssa/sprite_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,guards;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"cache line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define GUARD(x) do{++guards;CHECK(x);}while(0)
int main(int argc,char **argv)
{
    SsaSpriteAssets *a=calloc(1,sizeof(*a));SsaSpriteCache *c=calloc(1,sizeof(*c));
    SsaSpriteCache *saved=calloc(1,sizeof(*saved));SsaSpriteDispatch d={0};
    SsaWeaponObject objects[84]={0};uint8_t *rom=malloc(8u*1024u*1024u);
    FILE *f;unsigned i,j,k;int blocks[50];
    CHECK(argc==2 && a && c && saved && rom);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,8u*1024u*1024u,f)==8u*1024u*1024u);CHECK(!fclose(f));
    CHECK(ssa_sprite_assets_load(a,rom,8u*1024u*1024u)==1);free(rom);
    CHECK(ssa_sprite_cache_init(c)==1);CHECK(c->free_count==1 && c->budget==10000);
    for(i=0;i<84;++i){d.objects[i]=&objects[i];d.active[i]=i;}
    *saved=*c;GUARD(ssa_sprite_cache_init(c)==-1);CHECK(!memcmp(saved,c,sizeof(*c)));
    /* Exhaust real tile ranges, release fragmented blocks and merge again. */
    for(k=0;k<128;++k) {
        for(i=0;i<50;++i){blocks[i]=ssa_sprite_cache_allocate(c,10);CHECK(blocks[i]>=160);}
        CHECK(ssa_sprite_cache_allocate(c,1)==-1);
        for(i=0;i<25;++i)CHECK(ssa_sprite_cache_release(c,(uint16_t)blocks[i*2])==1);
        for(i=0;i<25;++i)CHECK(ssa_sprite_cache_release(c,(uint16_t)blocks[i*2+1])==1);
        /* Original E1CC leaves its final adjacent pair until a later call. */
        CHECK(c->used_count==0 && c->free_count==2 && c->free_blocks[0].start==160 && c->free_blocks[0].count==500);
        CHECK(c->free_blocks[1].start==660 && c->free_blocks[1].count==12);
    }
    CHECK(ssa_sprite_cache_free(c)==1);CHECK(ssa_sprite_cache_init(c)==1);
    d.active_count=1;objects[0].type=2;objects[0].owner=1;objects[0].animation=0;
    objects[0].previous_frame=16383;
    /* Every actual weapon image reaches reserved tile0, including mirrors. */
    for(i=0;i<311;++i) {
        objects[0].frame=(uint16_t)i;
        CHECK(ssa_sprite_cache_prepare(c,&d,a,0)==1 && c->transfer_count==1);
        CHECK(objects[0].previous_frame==i && (objects[0].flags&4));
        CHECK(ssa_sprite_cache_flush(c,a)==1 && c->transfer_count==0);
        CHECK(!memcmp(c->tiles,a->images[i].tiles,a->images[i].tile_count*64u));++frames;
    }
    CHECK(ssa_sprite_cache_free(c)==1);CHECK(ssa_sprite_cache_init(c)==1);
    memset(objects,0,sizeof(objects));d.active_count=20;
    for(i=0;i<84;++i){objects[i].type=1;objects[i].screen_x=120;objects[i].screen_y=80;objects[i].animation=0;objects[i].previous_frame=65535;}
    for(i=0;i<2048;++i) {
        for(j=0;j<20;++j)objects[j].frame=(uint16_t)(((i/4+j*13)%311)|((i&3)<<14));
        CHECK(ssa_sprite_cache_prepare(c,&d,a,0)==1);
        CHECK(c->free_count<=50 && c->used_count<=50 && c->image_count<=50 && c->transfer_count<=8);
        CHECK(ssa_sprite_cache_flush(c,a)==1);++frames;
    }
    objects[0].frame=311;*saved=*c;
    GUARD(ssa_sprite_cache_prepare(c,&d,a,0)==-1);CHECK(!memcmp(c,saved,sizeof(*c)));
    objects[0].frame=0;
    c->updating=1;*saved=*c;
    GUARD(ssa_sprite_cache_prepare(c,&d,a,0)==-1);GUARD(ssa_sprite_cache_flush(c,a)==-1);
    GUARD(ssa_sprite_cache_allocate(c,1)==-1);GUARD(ssa_sprite_cache_release(c,160)==-1);
    GUARD(ssa_sprite_cache_free(c)==-1);CHECK(!memcmp(c,saved,sizeof(*c)));c->updating=0;
    d.updating=1;GUARD(ssa_sprite_cache_prepare(c,&d,a,0)==-1);d.updating=0;
    d.active[0]=84;GUARD(ssa_sprite_cache_prepare(c,&d,a,0)==-1);d.active[0]=0;
    GUARD(ssa_sprite_cache_prepare(c,&d,a,4)==-1);
    a->loaded=0;GUARD(ssa_sprite_cache_flush(c,a)==-1);a->loaded=1;
    /* A late malformed descriptor preserves its previously copied prefix. */
    c->transfer_count=2;c->transfers[0]=(SsaSpriteTransfer){0,511,2,0};
    c->transfers[1]=(SsaSpriteTransfer){311,0,1,0};
    CHECK(ssa_sprite_cache_flush(c,a)==-1 && !c->updating && !c->transfers[0].count);
    CHECK(!memcmp(c->tiles+511*64,a->images[0].tiles,64));
    CHECK(!memcmp(c->tiles,a->images[0].tiles+64,64));
    c->transfers[1].image=0;CHECK(ssa_sprite_cache_flush(c,a)==1 && !c->transfer_count);
    *saved=*c;CHECK(ssa_sprite_cache_reset(c,&d)==1);
    CHECK(!memcmp(saved->tiles,c->tiles,sizeof(c->tiles)) && c->free_count==1 && !c->image_count);
    for(i=0;i<d.active_count;++i)CHECK(!objects[d.active[i]].previous_frame);
    CHECK(ssa_sprite_cache_free(c)==1);ssa_sprite_assets_free(a);free(a);free(c);free(saved);
    printf("{\"assertions\":%u,\"frames\":%u,\"guards\":%u}\n",checks,frames,guards);return 0;
}
