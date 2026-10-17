#include "ssa/map_texture_bindings.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) {return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static int32_t signed16(unsigned n) {return n<32768?(int32_t)n:(int32_t)n-65536;}
static int span(const SsaMapAssets *m,uint32_t offset,size_t count,size_t stride) {
    return m && m->data && (!count || offset>=0x48) && offset<=m->size && count<=(m->size-offset)/stride;
}
static int ready(const SsaMapTextureBindings *b) {
    return b && b->map && b->assets && b->assets->loaded && b->records &&
        b->family<SSA_TEXTURE_FAMILIES && b->count==b->map->texture_count &&
        span(b->map,b->map->textures,b->count,16);
}
int ssa_map_texture_bind_records(SsaMapTextureRecord *records,size_t count,const SsaTextureArchive *archive) {
    size_t i;
    if(!archive || (archive->count && !archive->images) || count>65535 || (count && !records))return -1;
    for(i=0;i<count;++i) {
        int index=ssa_texture_archive_find(archive,records[i].name);
        if(index>=0) {
            records[i].width=archive->images[index].width;records[i].height=archive->images[index].height;
            records[i].texture_index=signed16((unsigned)index);
        } else records[i].texture_index=0;
    }
    return 1;
}
int ssa_map_texture_bindings_init(SsaMapTextureBindings *out,const SsaMapAssets *map,const SsaTextureAssets *assets,unsigned family) {
    SsaMapTextureBindings b={0};size_t i;
    if(!out || out->map || out->assets || out->records || out->count || out->family || !map || !assets ||
       !assets->loaded || family>=SSA_TEXTURE_FAMILIES ||
       !span(map,map->textures,map->texture_count,16))return -1;
    b.records=calloc(map->texture_count?map->texture_count:1,sizeof(*b.records));if(!b.records)return -1;
    b.map=map;b.assets=assets;b.family=(uint8_t)family;b.count=map->texture_count;
    for(i=0;i<b.count;++i) {
        const uint8_t *p=map->data+map->textures+i*16;
        memcpy(b.records[i].name,p,8);b.records[i].width=u16(p+8);b.records[i].height=u16(p+10);
    }
    if(ssa_map_texture_bind_records(b.records,b.count,&assets->families[family].archives[0])<0) {
        free(b.records);return -1;
    }
    *out=b;return 1;
}
void ssa_map_texture_bindings_free(SsaMapTextureBindings *b) {
    if(!b)return;
    free(b->records);memset(b,0,sizeof(*b));
}
const SsaMapTextureRecord *ssa_map_texture_record(const SsaMapTextureBindings *b,unsigned index) {
    if(!ready(b) || index>=b->count)return NULL;
    return &b->records[index];
}
const SsaTextureImage *ssa_map_texture_image(const SsaMapTextureBindings *b,unsigned index,unsigned mip) {
    const SsaMapTextureRecord *record=ssa_map_texture_record(b,index);
    if(!record || record->texture_index<0)return NULL;
    return ssa_texture_assets_image(b->assets,b->family,(unsigned)record->texture_index,mip);
}
int ssa_map_side_texture(const SsaMapTextureBindings *b,unsigned side,unsigned layer,const SsaMapTextureRecord **out) {
    unsigned id;const SsaMapTextureRecord *record;
    if(!ready(b) || !out || layer>SSA_MAP_SIDE_LOWER || side>=b->map->side_count ||
       !span(b->map,b->map->sides,b->map->side_count,12))return -1;
    id=u16(b->map->data+b->map->sides+(size_t)side*12+4+layer*2);
    if(id==65535) {*out=NULL;return 0;}
    if(id>=32768 || !(record=ssa_map_texture_record(b,id)))return -1;
    *out=record;return 1;
}
