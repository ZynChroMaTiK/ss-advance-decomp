#include "ssa/map_texture_bindings.h"
#include "ssa/switches.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do {++checks;if(!(c)){fprintf(stderr,"map texture check %u line %u failed: %s\n",checks,__LINE__,#c);abort();}} while(0)
static unsigned checks,switch_changes,enqueue_calls,sounds,lookups,guards;
static unsigned u16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static int enqueue(void *ctx,unsigned id) {(void)ctx;(void)id;++enqueue_calls;return 0;}
static int sound(void *ctx,unsigned id) {(void)ctx;CHECK(id==41);++sounds;return 0;}
int main(int argc,char **argv) {
    FILE *file;long length;uint8_t *rom;SsaTextureAssets assets={0};unsigned level,i,j;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));length=ftell(file);CHECK(length>0);
    rewind(file);rom=malloc((size_t)length);CHECK(rom);CHECK(fread(rom,1,(size_t)length,file)==(size_t)length);CHECK(!fclose(file));
    CHECK(ssa_texture_assets_load(&assets,rom,(size_t)length)==1);
    for(level=0;level<16;++level) {
        SsaMapAssets map={0};SsaMapTextureBindings b={0};SsaSwitches switches={0};
        SsaSwitchOps ops={enqueue,sound,NULL};const SsaMapTextureRecord *record;
        CHECK(ssa_map_assets_load(&map,rom,(size_t)length,level)==1);
        CHECK(ssa_map_texture_bindings_init(&b,&map,&assets,(unsigned)ssa_texture_assets_family(level))==1);
        for(i=0;i<map.side_count;++i)for(j=0;j<3;++j) {
            unsigned id=u16(map.data+map.sides+i*12+4+j*2);
            int result=ssa_map_side_texture(&b,i,j,&record);CHECK(result==(id==65535?0:1));
            if(result)CHECK(record==&b.records[id]);else CHECK(!record);
            ++lookups;
        }
        CHECK(ssa_switches_init(&switches,&map,rom,(size_t)length)==0);
        for(i=0;i<switches.count;++i)if(!(switches.entries[i].flags&1)) {
            unsigned line=switches.entries[i].line,side=u16(map.data+map.lines+line*40+8);
            unsigned before=u16(map.data+map.sides+side*12+6);
            CHECK(ssa_map_side_texture(&b,side,1,&record)==1 && record==&b.records[before]);
            CHECK(ssa_switch_activate(&switches,line,&ops)==0);
            CHECK(ssa_map_side_texture(&b,side,1,&record)==1);
            CHECK(record==&b.records[u16(map.data+map.sides+side*12+6)]);
            CHECK(record!=&b.records[before]);++switch_changes;
        }
        CHECK(ssa_switches_free(&switches)==0);
        {
            uint32_t saved=map.sides;const SsaMapTextureRecord *sentinel=&b.records[0];
            map.sides=(uint32_t)map.size;record=sentinel;CHECK(ssa_map_side_texture(&b,0,0,&record)==-1 && record==sentinel);
            map.sides=saved;++guards;
            b.assets=NULL;CHECK(ssa_map_side_texture(&b,0,0,&record)==-1 && record==sentinel);b.assets=&assets;++guards;
        }
        ssa_map_texture_bindings_free(&b);CHECK(!b.map && !b.records && !b.assets);ssa_map_assets_free(&map);
    }
    {
        uint8_t header[0x48]={0};SsaMapAssets empty={0};SsaMapTextureBindings b={0};SsaMapTextureRecord r={0};
        SsaTextureArchive archive={0};empty.data=header;empty.size=sizeof(header);
        CHECK(ssa_map_texture_bindings_init(&b,&empty,&assets,0)==1);CHECK(!b.count && !ssa_map_texture_record(&b,0));
        ssa_map_texture_bindings_free(&b);
        CHECK(ssa_map_texture_bind_records(NULL,0,&archive)==1);
        CHECK(ssa_map_texture_bind_records(&r,65536,&archive)==-1);++guards;
    }
    CHECK(switch_changes && enqueue_calls==switch_changes && sounds==switch_changes);
    ssa_texture_assets_free(&assets);free(rom);
    printf("{\"assertions\":%u,\"live_side_lookups\":%u,\"native_switch_changes\":%u,\"guards\":%u}\n",checks,lookups,switch_changes,guards);return 0;
}
