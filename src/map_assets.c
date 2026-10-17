#include "ssa/map_assets.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
static int32_t s16(const uint8_t *p) {
    uint16_t n = u16(p);
    return n < 32768 ? (int32_t)n : (int32_t)n - 65536;
}
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)u16(p) | (uint32_t)u16(p+2) << 16;
}
static void put16(uint8_t *p, int32_t n) {
    uint16_t bits = (uint16_t)n;
    p[0] = (uint8_t)bits; p[1] = (uint8_t)(bits >> 8);
}
static int range(size_t size, uint32_t offset, size_t count, size_t stride) {
    return offset >= 0x48 && offset <= size && count <= (size-offset)/stride;
}

int ssa_lz77_decode(const uint8_t *src, size_t src_size, uint8_t *dst,
                    size_t dst_capacity, size_t *consumed) {
    size_t input = 4, output = 0, length;
    if (!src || src_size < 4 || src[0] != 0x10) return 0;
    length = (size_t)src[1] | (size_t)src[2] << 8 | (size_t)src[3] << 16;
    if (length > dst_capacity || (length && !dst)) return 0;
    while (output < length) {
        unsigned mask, flags;
        if (input >= src_size) return 0;
        flags = src[input++];
        for (mask = 128; mask && output < length; mask >>= 1) {
            if (flags & mask) {
                size_t distance, count, i;
                unsigned pair;
                if (src_size-input < 2) return 0;
                pair = (unsigned)src[input] << 8 | src[input+1]; input += 2;
                distance = (pair & 4095) + 1; count = (pair >> 12) + 3;
                if (distance > output || count > length-output) return 0;
                for (i = 0; i < count; ++i) {
                    dst[output] = dst[output-distance]; ++output;
                }
            } else {
                if (input >= src_size) return 0;
                dst[output++] = src[input++];
            }
        }
    }
    if (consumed) *consumed = input;
    return 1;
}

static int validate(SsaMapAssets *m) {
    size_t i;
    size_t starts[11], lengths[11];
    uint8_t *d = m->data;
    if (m->size < 0x48 || memcmp(d, "WOD0", 4) || (size_t)u16(d+4)*4!=m->size) return 0;
#define SECTION(name, count, ch, oh, stride) \
    m->count = u16(d+(ch)); m->name = u32(d+(oh)); \
    if (m->count && !range(m->size,m->name,m->count,(stride))) return 0
    SECTION(things,thing_count,6,0x1c,12);
    SECTION(textures,texture_count,8,0x20,16);
    SECTION(sectors,sector_count,10,0x24,20);
    SECTION(nodes,node_count,12,0x28,32);
    SECTION(leaves,leaf_count,14,0x2c,8);
    SECTION(segments,segment_count,16,0x30,16);
    SECTION(sides,side_count,18,0x34,12);
    SECTION(lines,line_count,20,0x38,40);
    SECTION(vertices,vertex_count,22,0x3c,4);
#undef SECTION
    m->reject = u32(d+0x40); m->blockmap = u32(d+0x44);
    /* Line preparation writes into data. Reject aliases between record arrays
     * so those writes cannot corrupt a later record's validated indices. */
#define SPAN(slot,name,count,stride) \
    starts[slot]=m->name; lengths[slot]=(size_t)m->count*(stride)
    SPAN(0,things,thing_count,12); SPAN(1,textures,texture_count,16);
    SPAN(2,sectors,sector_count,20); SPAN(3,nodes,node_count,32);
    SPAN(4,leaves,leaf_count,8); SPAN(5,segments,segment_count,16);
    SPAN(6,sides,side_count,12); SPAN(7,lines,line_count,40);
    SPAN(8,vertices,vertex_count,4);
#undef SPAN
    starts[9]=m->reject; lengths[9]=(size_t)u16(d+0x18)*2;
    starts[10]=m->blockmap; lengths[10]=(size_t)u16(d+0x1a)*2;
    for (i=0; i<11; ++i) {
        size_t j;
        for (j=i+1; j<11; ++j)
            if (lengths[i] && lengths[j] && starts[i]<starts[j]+lengths[j] &&
                starts[j]<starts[i]+lengths[i]) return 0;
    }
    if (m->reject && (!range(m->size,m->reject,u16(d+0x18),2) ||
        (size_t)u16(d+0x18)*2 < ((size_t)m->sector_count*m->sector_count+7)/8)) return 0;
    for (i=0; i<m->sector_count; ++i) {
        const uint8_t *s = d+m->sectors+i*20;
        if (u16(s+8) && !range(m->size,u32(s+12),u16(s+8),2)) return 0;
    }
    for (i=0; i<m->leaf_count; ++i) {
        const uint8_t *leaf = d+m->leaves+i*8;
        if (u16(leaf+4) >= m->sector_count ||
            (size_t)u16(leaf)+u16(leaf+2) > m->segment_count) return 0;
    }
    for (i=0; i<m->node_count; ++i) {
        unsigned side;
        for (side=0; side<2; ++side) {
            uint16_t child = u16(d+m->nodes+i*32+24+side*2);
            if (child & 0x8000) {
                if ((child & 0x7fff) >= m->leaf_count) return 0;
            } else if (child >= i) return 0; /* postorder BSP: excludes cycles */
        }
    }
    for (i=0; i<m->line_count; ++i) {
        const uint8_t *line = d+m->lines+i*40;
        if (u16(line)>=m->vertex_count || u16(line+2)>=m->vertex_count ||
            u16(line+12)>=m->sector_count) return 0;
        if ((u16(line+26)&4) && u16(line+14)>=m->sector_count) return 0;
    }
    if (m->blockmap) {
        size_t cells, end, block_size=(size_t)u16(d+0x1a)*2;
        const uint8_t *b;
        if (block_size<8 || !range(m->size,m->blockmap,block_size,1)) return 0;
        end=(size_t)m->blockmap+block_size;
        b=d+m->blockmap;
        if (s16(b+4)<=0 || s16(b+6)<=0) return 0;
        cells=(size_t)u16(b+4)*u16(b+6);
        if (cells>(block_size-8)/2) return 0;
        for (i=0; i<cells; ++i) {
            size_t pos=(size_t)m->blockmap+(size_t)u16(b+8+i*2)*2;
            if (pos>end || end-pos<2) return 0;
            pos+=2; /* original skips the leading dummy word */
            for (;;) {
                uint16_t line;
                if (pos>end || end-pos<2) return 0;
                line=u16(d+pos); pos+=2;
                if (line==0xffff) break;
                if (line>=m->line_count) return 0;
            }
        }
    }
    return 1;
}

static void prepare_lines(SsaMapAssets *m) {
    size_t i;
    for (i=0; i<m->line_count; ++i) {
        uint8_t *line=m->data+m->lines+i*40;
        const uint8_t *a=m->data+m->vertices+(size_t)u16(line)*4;
        const uint8_t *b=m->data+m->vertices+(size_t)u16(line+2)*4;
        int32_t x1=s16(a), y1=s16(a+2), x2=s16(b), y2=s16(b+2);
        int32_t dx,dy,type;
        put16(line+4,x2-x1); put16(line+6,y2-y1);
        put16(line+16,y1>y2?y1:y2); put16(line+18,y1<y2?y1:y2);
        put16(line+20,x1<x2?x1:x2); put16(line+22,x1>x2?x1:x2);
        dx=s16(line+4); dy=s16(line+6);
        /* Original tests integer dy/dx, not just the signs of dx and dy. */
        type=dx==0?1:dy==0?0:(dy/dx>0?2:3);
        put16(line+24,type);
    }
}

int ssa_map_assets_load(SsaMapAssets *out, const uint8_t *rom,
                        size_t rom_size, unsigned level) {
    SsaMapAssets m={0};
    size_t table=0x44aef4, offset, length;
    uint32_t address;
    if (!out || !rom || level>=16 || rom_size<table+16*4) return 0;
    address=u32(rom+table+level*4);
    if (address<0x08000000) return 0;
    offset=(size_t)(address-0x08000000);
    if (offset>rom_size || rom_size-offset<4 || rom[offset]!=0x10) return 0;
    length=(size_t)rom[offset+1] | (size_t)rom[offset+2]<<8 | (size_t)rom[offset+3]<<16;
    if (length<0x48) return 0;
    m.data=(uint8_t *)malloc(length);
    if (!m.data) return 0;
    m.size=length; m.rom_offset=(uint32_t)offset;
    if (!ssa_lz77_decode(rom+offset,rom_size-offset,m.data,length,NULL) || !validate(&m)) {
        free(m.data); return 0;
    }
    prepare_lines(&m);
    *out=m;
    return 1;
}

void ssa_map_assets_free(SsaMapAssets *map) {
    if (map) { free(map->data); memset(map,0,sizeof(*map)); }
}
