#include "ssa/world_visibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,frames,guards;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#define GUARD(x) do {++guards;CHECK(x);} while(0)
static uint16_t rd(const uint8_t *p){return (uint16_t)(p[0]|(unsigned)p[1]<<8);}
static void wr(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static unsigned rng=0xb6b0;
static unsigned random32(void){rng=rng*1664525u+1013904223u;return rng;}
static void check_queue(SsaWorldVisibility *v)
{
    unsigned count=0,index=v->head,previous=65535;
    while(index!=65535) {
        SsaVisibleWall *w;
        CHECK(index<v->wall_count && ++count<=v->wall_count);w=v->walls+index;
        CHECK(w->previous==previous && w->left<w->right && w->right<=120);
        CHECK(w->range_start+w->range_count<=v->range_count);
        CHECK((w->segment_flags&0x7fff)<v->map->segment_count);
        for(unsigned j=0;j<w->range_count;++j) {
            const SsaVisibilityRange *r=v->ranges+w->range_start+j;
            CHECK(r->left>=0 && r->left<=r->right && r->right<120);
        }
        previous=index;index=w->next;
    }
    CHECK(previous==v->tail);
}
int main(void)
{
    FILE *file=fopen("orig/SSA.gba","rb");size_t size;uint8_t *rom;
    SsaMathTables *math=calloc(1,sizeof(*math));SsaVisibilityTables *tables=calloc(1,sizeof(*tables));
    CHECK(file && math && tables);CHECK(!fseek(file,0,SEEK_END));size=(size_t)ftell(file);rewind(file);
    rom=malloc(size);CHECK(rom && fread(rom,1,size,file)==size);fclose(file);
    CHECK(ssa_math_tables_load(math,rom,size)==1);CHECK(ssa_visibility_tables_load(tables,rom,size)==1);
    GUARD(ssa_visibility_tables_load(NULL,rom,size)==-1);
    GUARD(ssa_visibility_tables_load(tables,NULL,size)==-1);
    GUARD(ssa_visibility_tables_load(tables,rom,0x44aed7)==-1);
    for(unsigned level=0;level<16;++level) {
        SsaMapAssets m={0},bad;SsaPlanePool pool;SsaWorldVisibility *v=calloc(1,sizeof(*v)),empty={0};
        SsaPlayerStart start;SsaRenderCamera c={0};SsaSegmentProjection projection;
        CHECK(v && ssa_map_assets_load(&m,rom,size,level)==1 && ssa_map_player_start(&m,&start)==1);
        CHECK(ssa_plane_pool_init(&pool,level)==1);
        CHECK(ssa_world_visibility_init(v,&m,math,tables,&pool)==1);
        GUARD(ssa_world_visibility_init(v,&m,math,tables,&pool)==-1);
        GUARD(ssa_world_visibility_init(&empty,NULL,math,tables,&pool)==-1);
        GUARD(ssa_world_visibility_init(&empty,&m,NULL,tables,&pool)==-1);
        GUARD(ssa_world_visibility_init(&empty,&m,math,NULL,&pool)==-1);
        GUARD(ssa_world_visibility_init(&empty,&m,math,tables,NULL)==-1);
        for(unsigned j=0;j<8;++j) {
            bad=m;
            switch(j) {
            case 0:bad.data=NULL;break;case 1:bad.nodes=UINT32_MAX;break;
            case 2:bad.leaves=(uint32_t)bad.size;break;case 3:bad.segments=(uint32_t)bad.size;break;
            case 4:bad.sectors=(uint32_t)bad.size;break;case 5:bad.sides=(uint32_t)bad.size;break;
            case 6:bad.vertices=(uint32_t)bad.size;break;default:bad.reject=(uint32_t)bad.size;break;
            }
            GUARD(ssa_world_visibility_init(&empty,&bad,math,tables,&pool)==-1 && !empty.map);
        }
        c.x=start.x;c.y=start.y;c.z=start.z+48*65536;c.sector=(int16_t)ssa_map_sector(&m,c.x,c.y);
        for(unsigned frame=0;frame<256;++frame) {
            c.heading=(uint16_t)(start.heading+frame*257);
            c.z=start.z+(int32_t)(48+(frame%64))*65536;
            CHECK(ssa_world_visibility_build(v,&c,(uint8_t)frame)==1);check_queue(v);++frames;
        }
        GUARD(ssa_world_visibility_build(v,NULL,0)==-1);
        c.sector=-1;GUARD(ssa_world_visibility_build(v,&c,0)==-1);c.sector=0;
        GUARD(ssa_visibility_segment(v,m.segment_count,&projection)==-1);
        GUARD(ssa_visibility_segment(v,0,NULL)==-1);
        GUARD(ssa_visibility_box(v,NULL)==-1);
        GUARD(ssa_visibility_clip(v,-1,3,0)==-1);
        GUARD(ssa_visibility_clip(v,3,121,0)==-1);
        GUARD(ssa_visibility_clip(v,3,2,1)==-1);
        v->updating=1;
        GUARD(ssa_world_visibility_build(v,&c,0)==-1);
        GUARD(ssa_visibility_reset(v,&c,0)==-1);
        GUARD(ssa_visibility_segment(v,0,&projection)==-1);
        GUARD(ssa_visibility_clip(v,0,1,0)==-1);
        GUARD(ssa_world_visibility_free(v)==-1);v->updating=0;
        CHECK(ssa_visibility_reset(v,&c,0)==1);
        v->range_count=SSA_VISIBILITY_RANGE_LIMIT;
        GUARD(ssa_visibility_clip(v,1,2,1)==-1 && v->solid_count==2);
        CHECK(ssa_visibility_reset(v,&c,0)==1);
        v->solid_count=0;GUARD(ssa_visibility_clip(v,1,2,1)==-1);
        CHECK(ssa_visibility_reset(v,&c,0)==1);
        /* Invalid BSP cycles fail without recursion or an infinite traversal. */
        {
            uint8_t *node=m.data+m.nodes+(m.node_count-1)*32;
            uint16_t a=rd(node+24),b=rd(node+26);
            wr(node+24,(uint16_t)(m.node_count-1));wr(node+26,(uint16_t)(m.node_count-1));
            GUARD(ssa_world_visibility_build(v,&c,0)==-1 && !v->updating);
            wr(node+24,a);wr(node+26,b);
        }
        {
            uint8_t *seg=m.data+m.segments;uint16_t vertex=rd(seg);
            c.heading=(uint16_t)(rd(seg+6)-0x4000);CHECK(ssa_visibility_reset(v,&c,0)==1);
            wr(seg,65535);GUARD(ssa_visibility_segment(v,0,&projection)==-1);wr(seg,vertex);
        }
        /* Explicit clipping exhaustion is transactional for this primitive. */
        for(unsigned j=0;j<128;++j) {
            unsigned a=random32()%120,b=random32()%121;
            if(a>b){unsigned t=a;a=b;b=t;}
            CHECK(ssa_visibility_reset(v,&c,0)==1);
            CHECK(ssa_visibility_clip(v,(int)a,(int)b,1)>=0);
            CHECK(ssa_visibility_clip(v,(int)a,(int)b,0)==0);
        }
        if(level==0) {
            SsaMapAssets synthetic=m;SsaWorldVisibility *overflow=calloc(1,sizeof(*overflow));
            SsaPlanePool other;SsaRenderCamera eye={0};uint8_t *node,*line,*side,*sector,*vertices,*leaf;
            synthetic.data=malloc(m.size);CHECK(synthetic.data && overflow);memcpy(synthetic.data,m.data,m.size);
            synthetic.node_count=synthetic.leaf_count=synthetic.line_count=synthetic.side_count=synthetic.sector_count=1;
            synthetic.segment_count=101;synthetic.vertex_count=2;synthetic.data[synthetic.reject]=0;
            node=synthetic.data+synthetic.nodes;memset(node,0,32);wr(node+4,1);wr(node+24,0x8000);wr(node+26,0x8000);
            leaf=synthetic.data+synthetic.leaves;memset(leaf,0,8);wr(leaf,101);
            line=synthetic.data+synthetic.lines;memset(line,0,40);wr(line+26,4);
            side=synthetic.data+synthetic.sides;memset(side,0,12);
            sector=synthetic.data+synthetic.sectors;memset(sector,0,20);wr(sector+2,128);sector[10]=1;sector[11]=2;
            vertices=synthetic.data+synthetic.vertices;wr(vertices,(uint16_t)-64);wr(vertices+2,128);wr(vertices+4,64);wr(vertices+6,128);
            for(unsigned j=0;j<101;++j) {
                uint8_t *seg=synthetic.data+synthetic.segments+j*16;memset(seg,0,16);wr(seg+2,1);wr(seg+6,0x4000);
            }
            eye.z=48*65536;CHECK(ssa_plane_pool_init(&other,0)==1);
            CHECK(ssa_world_visibility_init(overflow,&synthetic,math,tables,&other)==1);
            GUARD(ssa_world_visibility_build(overflow,&eye,0)==-1 && overflow->wall_count==100 &&
                  overflow->range_count==101 && !overflow->updating);
            check_queue(overflow);
            other.capacity=1;other.count=0;
            GUARD(ssa_world_visibility_build(overflow,&eye,0)==-1 && overflow->wall_count==0 &&
                  other.count==1 && !overflow->updating);
            CHECK(ssa_world_visibility_free(overflow)==1);free(overflow);free(synthetic.data);
        }
        CHECK(ssa_world_visibility_free(v)==1);free(v);ssa_map_assets_free(&m);
    }
    free(rom);free(math);free(tables);
    printf("{\"assertions\":%u,\"native_frames\":%u,\"native_guards\":%u}\n",checks,frames,guards);
    return 0;
}
