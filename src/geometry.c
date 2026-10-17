#include "ssa/geometry.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static int32_t add(int32_t a, int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t asr(int32_t a, unsigned n) {
    return bits(((uint32_t)a>>n) | (a<0 ? UINT32_MAX<<(32-n) : 0));
}
static int32_t mul(int32_t a, int32_t b) { return bits((uint32_t)a*(uint32_t)b); }
static int32_t fixed_mul(int32_t a, int32_t b) {
    int64_t p=(int64_t)a*b, q=p/65536;
    if (p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) {
    uint16_t u=u16(p);
    return u<=INT16_MAX ? (int16_t)u : (int16_t)(-1-(int32_t)(UINT16_MAX-u));
}
static const uint8_t *record(const SsaMapAssets *m, uint32_t offset, size_t index, size_t stride) {
    if (!m || !m->data || offset>m->size || stride>m->size-offset ||
        index>(m->size-offset-stride)/stride) return NULL;
    return m->data+offset+index*stride;
}

/* 0x0800B1B0 discards point fractions BEFORE subtracting the integer origin.
 * The sign shortcut and wrapped 32-bit products are intentional. */
int ssa_bsp_side(int32_t x, int32_t y, int32_t ox, int32_t oy, int32_t dx, int32_t dy) {
    int32_t px=asr(x,16), py=asr(y,16);
    uint32_t a,b;
    if (!dx) return px<=ox ? dy>0 : dy<0;
    if (!dy) return py<=oy ? dx<0 : dx>0;
    px=sub(px,ox); py=sub(py,oy);
    a=((uint32_t)dy^(uint32_t)px)>>31;
    b=((uint32_t)dx^(uint32_t)py)>>31;
    if (a!=b) return (int)a;
    return mul(py,dx)>=mul(dy,px);
}

/* 0x08003B0C keeps fractions and uses signed fixed-point products. */
int ssa_line_side(int32_t x, int32_t y, int16_t ox, int16_t oy, int16_t dx, int16_t dy) {
    int32_t fx=(int32_t)ox*65536, fy=(int32_t)oy*65536;
    if (!dx) return x<=fx ? dy>0 : dy<0;
    if (!dy) return y<=fy ? dx<0 : dx>0;
    return fixed_mul(sub(y,fy),dx)>=fixed_mul(dy,sub(x,fx));
}

int ssa_box_line_side(const SsaBounds *b, int16_t x, int16_t y, int16_t dx, int16_t dy, uint16_t slope) {
    int a=0,c=0;
    switch (slope) {
    case 0:
        a=b->top>(int32_t)y*65536; c=b->bottom>(int32_t)y*65536;
        if (dx<0) { a^=1; c^=1; }
        break;
    case 1:
        a=b->right<(int32_t)x*65536; c=b->left<(int32_t)x*65536;
        if (dy<0) { a^=1; c^=1; }
        break;
    case 2:
        a=ssa_line_side(b->left,b->top,x,y,dx,dy);
        c=ssa_line_side(b->right,b->bottom,x,y,dx,dy);
        break;
    case 3:
        a=ssa_line_side(b->right,b->top,x,y,dx,dy);
        c=ssa_line_side(b->left,b->bottom,x,y,dx,dy);
        break;
    default: break;
    }
    return a==c ? a : -1;
}

int ssa_map_leaf(const SsaMapAssets *m, int32_t x, int32_t y) {
    uint16_t index;
    size_t remaining;
    if (!m) return -1;
    index=(uint16_t)(m->node_count-1u);
    remaining=(size_t)m->node_count+1;
    while (index!=UINT16_MAX && remaining--) {
        const uint8_t *n;
        int side;
        if (index&0x8000) return (index&0x7fff)<m->leaf_count ? index&0x7fff : -1;
        if (index>=m->node_count || !(n=record(m,m->nodes,index,32))) return -1;
        side=ssa_bsp_side(x,y,s16(n),s16(n+2),s16(n+4),s16(n+6));
        index=u16(n+24+side*2);
    }
    return -1;
}

int ssa_map_sector(const SsaMapAssets *m, int32_t x, int32_t y) {
    int leaf=ssa_map_leaf(m,x,y), sector;
    const uint8_t *p;
    if (leaf<0 || !(p=record(m,m->leaves,(size_t)leaf,8))) return -1;
    sector=s16(p+4);
    return sector>=0 && sector<m->sector_count ? sector : -1;
}

int ssa_map_player_start(const SsaMapAssets *m, SsaPlayerStart *out) {
    size_t i;
    if (!m || !out) return 0;
    for (i=0;i<m->thing_count;++i) {
        const uint8_t *thing=record(m,m->things,i,12), *sector;
        SsaPlayerStart p={0};
        int s;
        if (!thing) return 0;
        if (u16(thing+6)!=1) continue;
        p.x=(int32_t)s16(thing)*65536; p.y=(int32_t)s16(thing+2)*65536;
        p.heading=u16(thing+4);
        s=ssa_map_sector(m,p.x,p.y);
        if (s<0 || !(sector=record(m,m->sectors,(size_t)s,20))) return 0;
        p.floor=s16(sector); p.ceiling=s16(sector+2); p.z=(int32_t)p.floor*65536;
        *out=p;
        return 1;
    }
    return 0;
}

int ssa_map_collision_init(SsaMapCollision *c, const SsaMapAssets *m) {
    if (!c || !m || !m->data) return 0;
    memset(c,0,sizeof(*c));
    c->visited=calloc(m->line_count ? m->line_count : 1,1);
    c->special_lines=malloc((m->line_count ? m->line_count : 1)*sizeof(uint16_t));
    if (!c->visited || !c->special_lines) { ssa_map_collision_free(c); return 0; }
    c->map=m;
    c->line_capacity=m->line_count;
    return 1;
}
void ssa_map_collision_free(SsaMapCollision *c) {
    if (!c) return;
    free(c->visited); free(c->special_lines); memset(c,0,sizeof(*c));
}

static int visit_line(SsaMapCollision *c, const SsaBounds *b, uint16_t index) {
    const SsaMapAssets *m=c->map;
    const uint8_t *l,*v,*front,*back;
    int16_t x,y,dx,dy,floor,ceil,low;
    int a,d;
    if (index>=m->line_count || !(l=record(m,m->lines,index,40))) return -1;
    if (c->visited[index]) return 1;
    c->visited[index]=1;
    if (b->right<=(int32_t)s16(l+20)*65536 || b->left>=(int32_t)s16(l+22)*65536 ||
        b->top<=(int32_t)s16(l+18)*65536 || b->bottom>=(int32_t)s16(l+16)*65536) return 1;
    if (u16(l)>=m->vertex_count || !(v=record(m,m->vertices,u16(l),4))) return -1;
    x=s16(v); y=s16(v+2); dx=s16(l+4); dy=s16(l+6);
    if (ssa_box_line_side(b,x,y,dx,dy,u16(l+24))!=-1) return 1;
    if (u16(l+36)) c->special_lines[c->special_count++]=index;
    if (!(u16(l+26)&4)) { c->blocking_line=index; return 0; }
    a=s16(l+12); d=s16(l+14);
    if (a<0 || d<0 || a>=m->sector_count || d>=m->sector_count ||
        !(front=record(m,m->sectors,(size_t)a,20)) || !(back=record(m,m->sectors,(size_t)d,20))) return -1;
    ceil=s16(front+2)<s16(back+2) ? s16(front+2) : s16(back+2);
    floor=s16(front)>s16(back) ? s16(front) : s16(back);
    low=s16(front)<s16(back) ? s16(front) : s16(back);
    if (ceil<c->ceiling) { c->ceiling=ceil; c->ceiling_line=index; }
    if (floor>c->floor) c->floor=floor;
    if (low<c->low_floor) c->low_floor=low;
    if (c->ceiling==c->floor) { c->blocking_line=index; return 0; }
    return 1;
}

int ssa_map_query_visit(SsaMapCollision *c, int32_t old_x, int32_t old_y, int32_t x, int32_t y, int32_t radius,
                        int (*visit)(void *, const SsaBounds *), void *context) {
    const SsaMapAssets *m;
    const uint8_t *sector,*blocks;
    SsaBounds b;
    int s, width,height,left,right,bottom,top,bx,by;
    int32_t ox,oy;
    size_t block_words;
    if (!c || !(m=c->map) || !c->visited || !c->special_lines || radius<0) return -1;
    if (m->line_count>c->line_capacity) return -1;
    c->special_count=0; c->floor=c->ceiling=c->low_floor=0;
    c->ceiling_line=c->blocking_line=-1;
    memset(c->visited,0,m->line_count);
    b.left=sub(old_x<x ? old_x : x,radius); b.right=add(old_x<x ? x : old_x,radius);
    b.bottom=sub(old_y<y ? old_y : y,radius); b.top=add(old_y<y ? y : old_y,radius);
    s=ssa_map_sector(m,x,y);
    if (s<0 || !(sector=record(m,m->sectors,(size_t)s,20)) || !(blocks=record(m,m->blockmap,0,8))) return -1;
    c->floor=c->low_floor=s16(sector); c->ceiling=s16(sector+2);
    if (m->size<0x48) return -1;
    block_words=u16(m->data+0x1a);
    if (block_words<4 || m->blockmap>m->size || block_words>(m->size-m->blockmap)/2) return -1;
    ox=(int32_t)s16(blocks)*65536; oy=(int32_t)s16(blocks+2)*65536;
    width=s16(blocks+4); height=s16(blocks+6);
    if (width<=0 || height<=0) return -1;
    if ((size_t)width*(unsigned)height>block_words-4) return -1;
    if (visit) {
        int result=visit(context,&b);
        if (result!=1) return result;
    }
    left=asr(sub(b.left,ox),23); right=asr(sub(b.right,ox),23);
    bottom=asr(sub(b.bottom,oy),23); top=asr(sub(b.top,oy),23);
    /* Original visits x outside, y inside; cells outside map are empty. */
    if (left<0) left=0;
    if (bottom<0) bottom=0;
    if (right>=width) right=width-1;
    if (top>=height) top=height-1;
    for (bx=left;bx<=right;++bx) for (by=bottom;by<=top;++by) {
        const uint8_t *cell=record(m,m->blockmap,4+(size_t)by*(unsigned)width+(unsigned)bx,2);
        size_t cursor;
        if (!cell) return -1;
        cursor=(size_t)u16(cell)+1; /* skip the leading blocklist word */
        for (;;) {
            const uint8_t *entry;
            uint16_t index;
            int result;
            if (cursor>=block_words) return -1;
            entry=record(m,m->blockmap,cursor++,2);
            if (!entry) return -1;
            index=u16(entry);
            if (index==UINT16_MAX) break;
            result=visit_line(c,&b,index);
            if (result!=1) return result;
        }
    }
    return 1;
}

int ssa_map_query(SsaMapCollision *c, int32_t old_x, int32_t old_y, int32_t x, int32_t y, int32_t radius) {
    return ssa_map_query_visit(c,old_x,old_y,x,y,radius,NULL,NULL);
}

int ssa_map_commit_move(const SsaMapCollision *c, SsaBody *b, int32_t x, int32_t y) {
    int32_t z,height;
    if (!c || !b) return -1;
    z=asr(b->z,16); height=asr(b->height,16);
    if (c->ceiling-c->floor<height) return 0;
    if (c->ceiling-z<height && !(b->flags&4)) return 0;
    if ((b->flags&4) && (z+height>c->ceiling || (z<c->floor && c->floor-c->low_floor>48))) {
        b->velocity_z=sub(b->velocity_z,0x80000);
        return 0;
    }
    if (c->floor-z>48) return 0;
    b->x=x; b->y=y; b->floor=(int16_t)c->floor; b->ceiling=(int16_t)c->ceiling;
    return 1;
}

int ssa_map_try_move(SsaMapCollision *c, SsaBody *b, int32_t radius, int32_t x, int32_t y) {
    int result;
    if (!b) return -1;
    result=ssa_map_query(c,b->x,b->y,x,y,radius);
    return result==1 ? ssa_map_commit_move(c,b,x,y) : result;
}
