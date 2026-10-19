#include "ssa/player_hazards.h"
#include <limits.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) {
    uint16_t n=u16(p);
    return n<=INT16_MAX ? (int16_t)n : (int16_t)(-1-(int32_t)(UINT16_MAX-n));
}
static int32_t bits(uint32_t n) { return n<=INT32_MAX ? (int32_t)n : -1-(int32_t)~n; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t asr(int32_t a,unsigned n) {
    return bits(((uint32_t)a>>n) | (a<0 ? UINT32_MAX<<(32-n) : 0));
}
static const uint8_t *record(const SsaMapAssets *m,uint32_t offset,size_t i,size_t stride) {
    if (!m || !m->data || offset>m->size || stride>m->size-offset ||
        i>(m->size-offset-stride)/stride) return NULL;
    return m->data+offset+i*stride;
}

int ssa_player_interaction_query(SsaMapCollision *c,const SsaEntity *e,
                                  uint8_t facing,const SsaMathTables *tables) {
    int32_t sine,cosine;
    if(!e || !tables)return -1;
    ssa_sincos(tables,(uint16_t)((unsigned)facing<<8),&sine,&cosine);
    return ssa_player_interaction_query_at(c,e,add(e->body.x,bits((uint32_t)sine<<6)),
        add(e->body.y,bits((uint32_t)cosine<<6)));
}
int ssa_player_interaction_query_at(SsaMapCollision *c,const SsaEntity *e,int32_t x,int32_t y) {
    const SsaMapAssets *m;
    const uint8_t *blocks;
    SsaBounds b;
    int32_t r,ox,oy;
    int width,height,left,right,bottom,top,bx,by;
    size_t words;
    if (!c || !(m=c->map) || !e || !c->visited || !c->special_lines ||
        m->line_count>c->line_capacity || e->radius<0 || m->size<0x48 ||
        !(blocks=record(m,m->blockmap,0,8))) return -1;
    /* 0429C computes destination-sector heights, but no caller uses them.
     * Validate that lookup, while exposing only the contacts as our contract. */
    if (ssa_map_sector(m,x,y)<0) return -1;
    r=e->radius;
    b.left=sub(x,r); b.right=add(x,r); b.bottom=sub(y,r); b.top=add(y,r);
    if (e->body.x<x) b.left=sub(b.left,sub(x,e->body.x));
    else b.right=sub(b.right,sub(x,e->body.x));
    if (e->body.y<y) b.bottom=sub(b.bottom,sub(y,e->body.y));
    else b.top=sub(b.top,sub(y,e->body.y));
    words=u16(m->data+0x1a);
    if (words<4 || words>(m->size-m->blockmap)/2) return -1;
    width=s16(blocks+4); height=s16(blocks+6);
    if (width<=0 || height<=0 || (size_t)width*(unsigned)height>words-4) return -1;
    ox=(int32_t)s16(blocks)*65536; oy=(int32_t)s16(blocks+2)*65536;
    left=asr(sub(b.left,ox),23); right=asr(sub(b.right,ox),23);
    bottom=asr(sub(b.bottom,oy),23); top=asr(sub(b.top,oy),23);
    if (left<0) left=0;
    if (bottom<0) bottom=0;
    if (right>=width) right=width-1;
    if (top>=height) top=height-1;
    c->special_count=0;
    memset(c->visited,0,m->line_count);
    for (bx=left;bx<=right;++bx) for (by=bottom;by<=top;++by) {
        size_t cursor=(size_t)u16(blocks+8+((size_t)by*(unsigned)width+(unsigned)bx)*2)+1;
        for (;;) {
            const uint8_t *line,*v;
            uint16_t index;
            if (cursor>=words) return -1;
            index=u16(blocks+cursor++*2);
            if (index==UINT16_MAX) break;
            if (index>=m->line_count) return -1;
            if (c->visited[index]) continue;
            c->visited[index]=1;
            line=record(m,m->lines,index,40);
            if (!line) return -1;
            if (b.right<=(int32_t)s16(line+20)*65536 || b.left>=(int32_t)s16(line+22)*65536 ||
                b.top<=(int32_t)s16(line+18)*65536 || b.bottom>=(int32_t)s16(line+16)*65536) continue;
            if (u16(line)>=m->vertex_count || !(v=record(m,m->vertices,u16(line),4))) return -1;
            if (ssa_box_line_side(&b,s16(v),s16(v+2),s16(line+4),s16(line+6),u16(line+24))!=-1) continue;
            if (u16(line+36)) c->special_lines[c->special_count++]=index;
        }
    }
    return 0;
}

static int valid_game(const SsaPlayerHazardGame *g) {
    return g && g->hazard_count<=UINT8_MAX && (!g->hazard_count || g->hazard_sectors);
}
int ssa_player_hazard_sample(const SsaMapAssets *m,const SsaEntity *e,const SsaPlayerHazardGame *g) {
    static const int8_t dx[16]={0,-2,2,-2,2,-1,1,-1,1,0,-2,2,-2,2,-1,1};
    static const int8_t dy[16]={0,-2,-2,2,2,-1,-1,1,1,0,-2,-2,2,2,-1,-1};
    int32_t x,y,r;
    unsigned n;
    int sector;
    size_t i;
    if (!m || !m->data || !e || !valid_game(g)) return -1;
    if (!g->hazard_count) return 0;
    n=g->frame_counter&15u;
    r=(dx[n]==1 || dx[n]==-1) ? e->radius/2 : e->radius;
    x=dx[n]<0 ? sub(e->body.x,r) : dx[n]>0 ? add(e->body.x,r) : e->body.x;
    y=dy[n]<0 ? sub(e->body.y,r) : dy[n]>0 ? add(e->body.y,r) : e->body.y;
    sector=ssa_map_sector(m,x,y);
    if (sector<0) return -1;
    for (i=0;i<g->hazard_count;++i) if (g->hazard_sectors[i]==sector) return 1;
    return 0;
}
static void kill(SsaInventory *i) { i->health=0; i->stat_c4=0; i->stat_c6=0; }
int ssa_player_hazards_apply(SsaPlayerHazards *p,const SsaMapAssets *m,const SsaPlayerHazardGame *g) {
    const uint8_t *sector;
    int sample;
    if (!p || !p->entity || !p->inventory || !valid_game(g) || !m ||
        p->entity->sector<0 || p->entity->sector>=m->sector_count ||
        !(sector=record(m,m->sectors,(size_t)p->entity->sector,20))) return -1;
    if ((sector[10]==255 || (sector[10]==35 && g->level_counter>5)) &&
        asr(p->entity->body.z,16)==s16(sector)) kill(p->inventory);
    sample=ssa_player_hazard_sample(m,p->entity,g);
    if (sample<0) return -1;
    if (sample) kill(p->inventory);
    return 0;
}
static int hazards_step(SsaPlayerHazards *p,SsaMapCollision *c,
                              const SsaMathTables *tables,const SsaPlayerHazardGame *g,
                              const SsaPlayerHazardOps *ops,int immediate_use) {
    SsaEntity *entity;
    SsaMovement *movement;
    SsaInventory *inventory;
    size_t i;
    int eligible;
    if (!p || !(entity=p->entity) || !(movement=p->movement) || !(inventory=p->inventory) ||
        !c || !c->map || !tables || !valid_game(g) || !ops || !ops->line) return -1;
    eligible=immediate_use || p->weapon_action==0xdc00;
    if (!(g->local_flags&8)) {
        if (ssa_player_interaction_query(c,entity,p->facing,tables)<0) return -1;
        for (i=0;i<c->special_count;++i) {
            const uint8_t *line=record(c->map,c->map->lines,c->special_lines[i],40);
            unsigned kind;
            if (!line) return -1;
            kind=u16(line+36);
            if (kind==1 || (kind==2 && (movement->keys&0x10000u) && eligible)) {
                if (ops->line(ops->context,p,kind,c->special_lines[i])<0 ||
                    p->entity!=entity || p->movement!=movement || p->inventory!=inventory) return -1;
            }
        }
    }
    return ssa_player_hazards_apply(p,c->map,g);
}
int ssa_player_hazards_step(SsaPlayerHazards *p,SsaMapCollision *c,
    const SsaMathTables *tables,const SsaPlayerHazardGame *g,const SsaPlayerHazardOps *ops) {
    return hazards_step(p,c,tables,g,ops,0);
}
int ssa_player_hazards_step_immediate_use(SsaPlayerHazards *p,SsaMapCollision *c,
    const SsaMathTables *tables,const SsaPlayerHazardGame *g,const SsaPlayerHazardOps *ops) {
    return hazards_step(p,c,tables,g,ops,1);
}
