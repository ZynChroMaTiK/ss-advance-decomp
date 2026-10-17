#include "ssa/switches.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p) | (uint32_t)u16(p+2)<<16; }
static void put16(uint8_t *p, unsigned v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static int same_name(const uint8_t *a,const uint8_t *b,const uint8_t *classes) {
    unsigned i;
    for (i=0;i<8;++i) {
        unsigned x=a[i],y=b[i];
        if ((x+((classes[x]&1)?32:0)) != (y+((classes[y]&1)?32:0))) return 0;
        if (!x || !y) return 1;
    }
    return 1;
}
static uint8_t *record(SsaMapAssets *m, uint32_t offset, size_t i, size_t stride) {
    if (!m || !m->data || offset>m->size || stride>m->size-offset ||
        i>(m->size-offset-stride)/stride) return NULL;
    return m->data+offset+i*stride;
}
static uint8_t *side(SsaMapAssets *m, unsigned line) {
    uint8_t *l;
    unsigned index;
    if (line>=m->line_count || !(l=record(m,m->lines,line,40))) return NULL;
    index=u16(l+8);
    if (index>=32768 || index>=m->side_count) return NULL;
    return record(m,m->sides,index,12);
}
int ssa_switches_free(SsaSwitches *s) {
    if (!s || s->updating) return -1;
    free(s->entries); memset(s,0,sizeof(*s)); return 0;
}
int ssa_switches_init(SsaSwitches *out,SsaMapAssets *m,const uint8_t *rom,size_t size) {
    const uint8_t *names[4];
    SsaSwitches s={0};
    size_t i,j,k;
    if (!out || out->map || out->entries || out->count || out->updating || !m || !m->data ||
        !rom || size<0x44dd9c) return -1;
    for (i=0;i<4;++i) {
        uint32_t address=u32(rom+0x44dd8c+i*4);
        size_t offset;
        if (address<0x08000000) return -1;
        offset=address-0x08000000;
        if (offset>size || size-offset<8) return -1;
        names[i]=rom+offset;
    }
    for (i=0;i<m->line_count;++i) {
        uint8_t *l=record(m,m->lines,i,40);
        if (!l) return -1;
        if (u16(l+36)==2) ++s.count;
    }
    s.entries=calloc(s.count ? s.count : 1,sizeof(*s.entries));
    if (!s.entries) return -1;
    s.map=m;
    for (i=0,j=0;i<m->line_count;++i) {
        uint8_t *l=record(m,m->lines,i,40),*sd,*texture;
        unsigned texture_id;
        SsaSwitchEntry *e;
        if (u16(l+36)!=2) continue;
        e=&s.entries[j++]; e->line=(uint16_t)i; e->type=2; e->trigger=u16(l+38);
        if (!(sd=side(m,(unsigned)i))) goto fail;
        texture_id=u16(sd+6);
        if (texture_id>=32768 || texture_id>=m->texture_count ||
            !(texture=record(m,m->textures,texture_id,16))) goto fail;
        for (k=0;k<4;++k) if (same_name(texture,names[k],rom+0x449385)) {
            size_t pair;
            for (pair=0;pair<m->texture_count;++pair) {
                uint8_t *candidate=record(m,m->textures,pair,16);
                if (!candidate) goto fail;
                if (same_name(candidate,names[k^1u],rom+0x449385)) break;
            }
            if (pair==m->texture_count) goto fail;
            if (k&1) { e->on_texture=(uint8_t)texture_id; e->off_texture=(uint8_t)pair; e->flags|=1; }
            else { e->off_texture=(uint8_t)texture_id; e->on_texture=(uint8_t)pair; e->flags&=(uint16_t)~1u; }
            break;
        }
    }
    *out=s; return 0;
fail:
    ssa_switches_free(&s); return -1;
}
int ssa_switch_activate(SsaSwitches *s,unsigned line,const SsaSwitchOps *ops) {
    size_t i;
    SsaSwitchEntry *e;
    uint8_t *sd;
    int result;
    if (!s || s->updating || !s->map || (s->count && !s->entries) || !ops ||
        !ops->enqueue || !ops->sound || !(sd=side(s->map,line))) return -1;
    for (i=0;i<s->count;++i) if (s->entries[i].line==line) break;
    if (i==s->count) return -1;
    e=&s->entries[i];
    if (e->flags&1) return 0;
    s->updating=1; e->flags|=1; put16(sd+6,e->on_texture);
    result=ops->enqueue(ops->context,e->trigger);
    if (result>=0) result=ops->sound(ops->context,41);
    s->updating=0;
    return result<0 ? -1 : 0;
}
