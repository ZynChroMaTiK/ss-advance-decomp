/* Render-only shoreline geometry. Index expanded line bounds once per level;
 * read live sector floors/colors/ceilings when sampling so movers stay correct.
 * All sampling is read-only and safe in the hardware renderer's row jobs. */
#include "water_foam.inc"
enum { WATER_EDGE_CELLS=4096, WATER_EDGE_CELL_SIZE=128, WATER_EDGE_WIDTH=32 };
typedef struct WaterEdge {
    double x,y,dx,dy,inverse_length2,inverse_length;
    uint16_t sector[2];
    int left,right,top,bottom;
} WaterEdge;
typedef struct WaterEdgeRef {uint32_t edge,next;int x,y;} WaterEdgeRef;
typedef struct SsaWaterEdges {
    const SsaMapAssets *map;
    WaterEdge *lines;WaterEdgeRef *refs;
    uint32_t heads[WATER_EDGE_CELLS];
} SsaWaterEdges;
static unsigned edge_u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int edge_s16(const uint8_t *p){unsigned v=edge_u16(p);return v<32768?(int)v:(int)v-65536;}
static unsigned edge_bucket(int x,int y){return hash((uint32_t)x*UINT32_C(0x9e3779b9)^(uint32_t)y*UINT32_C(0x85ebca6b))&(WATER_EDGE_CELLS-1);}
static int edge_cell(double x){return (int)floor(x/WATER_EDGE_CELL_SIZE);}
static void edges_free(SsaWaterEdges *e){if(e){free(e->lines);free(e->refs);free(e);}}
static int edge_range(const SsaMapAssets *m,size_t offset,size_t count,size_t stride){return offset<=m->size && count<=(m->size-offset)/stride;}
static SsaWaterEdges *edges_init(const SsaMapAssets *m){
    SsaWaterEdges *e;size_t count=0,at=0;unsigned i;
    if(!edge_range(m,m->lines,m->line_count,40) || !edge_range(m,m->vertices,m->vertex_count,4) ||
       !edge_range(m,m->sectors,m->sector_count,20))return NULL;
    e=calloc(1,sizeof(*e));if(!e)return NULL;e->map=m;
    e->lines=calloc(m->line_count?m->line_count:1,sizeof(*e->lines));if(!e->lines){edges_free(e);return NULL;}
    for(i=0;i<WATER_EDGE_CELLS;++i)e->heads[i]=UINT32_MAX;
    for(i=0;i<m->line_count;++i){const uint8_t *line=m->data+m->lines+(size_t)i*40,*a,*b;WaterEdge *l=&e->lines[i];
        unsigned va=edge_u16(line),vb=edge_u16(line+2),front=edge_u16(line+12),back=edge_u16(line+14);
        if(va>=m->vertex_count || vb>=m->vertex_count || front>=m->sector_count){edges_free(e);return NULL;}
        if(!(edge_u16(line+26)&4))back=UINT16_MAX;
        else if(back>=m->sector_count){edges_free(e);return NULL;}
        l->sector[0]=(uint16_t)front;l->sector[1]=(uint16_t)back;
        if(front==back)continue;
        a=m->data+m->vertices+(size_t)va*4;b=m->data+m->vertices+(size_t)vb*4;
        l->x=edge_s16(a);l->y=edge_s16(a+2);l->dx=edge_s16(b)-l->x;l->dy=edge_s16(b+2)-l->y;
        if(!l->dx && !l->dy)continue;
        l->inverse_length2=1/(l->dx*l->dx+l->dy*l->dy);
        l->inverse_length=sqrt(l->inverse_length2);
        l->left=edge_cell(fmin(l->x,l->x+l->dx)-WATER_EDGE_WIDTH);
        l->right=edge_cell(fmax(l->x,l->x+l->dx)+WATER_EDGE_WIDTH);
        l->top=edge_cell(fmin(l->y,l->y+l->dy)-WATER_EDGE_WIDTH);
        l->bottom=edge_cell(fmax(l->y,l->y+l->dy)+WATER_EDGE_WIDTH);
        {size_t cells=(size_t)(l->right-l->left+1)*(size_t)(l->bottom-l->top+1);
            if(cells>UINT32_MAX-count || cells>SIZE_MAX/sizeof(*e->refs)-count){edges_free(e);return NULL;}
            count+=cells;}
    }
    e->refs=malloc((count?count:1)*sizeof(*e->refs));if(!e->refs){edges_free(e);return NULL;}
    for(i=0;i<m->line_count;++i){WaterEdge *l=&e->lines[i];int x,y;if(!l->inverse_length2)continue;
        for(y=l->top;y<=l->bottom;++y)for(x=l->left;x<=l->right;++x){unsigned bucket=edge_bucket(x,y);
            e->refs[at]=(WaterEdgeRef){i,e->heads[bucket],x,y};e->heads[bucket]=(uint32_t)at++;}}
    return e;
}
int ssa_water_edge_decal(const SsaWaterScene *scene,const SsaSurfaceDecals *d,uint8_t color,
    double x,double y,double height,double distance,uint32_t frame){
    const SsaWaterEdges *e;int cx,cy,tx,ty,result,texture_x,tile;uint32_t at;unsigned phase,u,v,variant;
    double nearest2=WATER_EDGE_WIDTH*WATER_EDGE_WIDTH,along=0,shore_offset=0;
    if(!scene || !(e=scene->edges) || !d || !d->loaded || !ssa_surface_decals_water(d,color,height) ||
       !(x>-65536 && x<65536 && y>-65536 && y<65536 && distance>=0 && distance<1536))return 0;
    /* World-locked2-unit texels match wall/decal density in every renderer. */
    tx=(int)floor(x/2);ty=(int)floor(y/2);x=tx*2.0+1;y=ty*2.0+1;
    cx=edge_cell(x);cy=edge_cell(y);
    for(at=e->heads[edge_bucket(cx,cy)];at!=UINT32_MAX;at=e->refs[at].next){
        const WaterEdgeRef *ref=&e->refs[at];const WaterEdge *l;const uint8_t *sector;unsigned side,other;
        double px,py,t,dx,dy,dist2;
        if(ref->x!=cx || ref->y!=cy)continue;
        l=&e->lines[ref->edge];
        px=x-l->x;py=y-l->y;side=l->dx*py-l->dy*px>0;
        if(l->sector[side]==UINT16_MAX)continue;
        sector=e->map->data+e->map->sectors+(size_t)l->sector[side]*20;
        if(edge_s16(sector)!=height || sector[10]!=color)continue;
        other=l->sector[!side];
        if(other!=UINT16_MAX){sector=e->map->data+e->map->sectors+(size_t)other*20;
            if(edge_s16(sector)==height && edge_s16(sector+2)>height && ssa_surface_decals_water(d,sector[10],height))continue;}
        t=(px*l->dx+py*l->dy)*l->inverse_length2;if(t<0)t=0;if(t>1)t=1;
        dx=px-t*l->dx;dy=py-t*l->dy;dist2=dx*dx+dy*dy;
        if(dist2<nearest2){nearest2=dist2;along=(x*l->dx+y*l->dy)*l->inverse_length;
            shore_offset=(l->y*l->dx-l->x*l->dy)*l->inverse_length;
            /* World-origin UVs stay continuous across collinear line splits,
             * including reversed vertex order and either water-facing side. */
            if(l->dx<0 || (!l->dx && l->dy<0)){along=-along;shore_offset=-shore_offset;}}
    }
    if(nearest2>=WATER_EDGE_WIDTH*WATER_EDGE_WIDTH)return 0;
    texture_x=(int)floor(along/2)+(int)(d->seed&63);u=(uint32_t)texture_x&63;
    tile=(int)floor(texture_x/64.0);
    /* Stable per-world-tile art, independent of line index and frame. Common
     * atlas borders preserve joins even when adjacent variants differ. */
    variant=hash((uint32_t)tile*UINT32_C(0x517cc1b7)^
        (uint32_t)(int)floor(shore_offset/2)*UINT32_C(0x6c8e9cf5)^d->seed)&(WATER_FOAM_VARIANTS-1);
    v=(unsigned)(sqrt(nearest2)/2)*2+(u>>5);u&=31;
    phase=(frame/WATER_FRAME_TICKS+(d->seed&31))&31;
    result=(int)((water_foam[variant][phase][v]>>u)&1) |
        (int)(((water_foam_surface[variant][phase][v]>>u)&1)<<1);
    if(!result)return 0;
    if(distance>768){double fade=(1536-distance)/768;
        uint32_t noise=hash((uint32_t)tx*UINT32_C(0x517cc1b7)^(uint32_t)ty*UINT32_C(0x6c8e9cf5)^d->seed);
        if((noise&65535)>=fade*fade*(3-2*fade)*65536)return 0;}
    return result;
}

/* Dry edge grime shares the boundary spatial index, not shoreline artwork or
 * animation. All tile choices are deterministic and independent of line IDs. */
#include "surface_edge_tiles.inc"
static unsigned surface_edge_present(int tile,int offset,unsigned seed)
{return hash((uint32_t)tile*UINT32_C(0x517cc1b7)^(uint32_t)offset*UINT32_C(0x6c8e9cf5)^seed)%100<62;}
int ssa_surface_edge_decal(const SsaWaterScene *scene,const SsaSurfaceDecals *d,unsigned ceiling,
    uint8_t color,double x,double y,double height,double distance)
{
    const SsaWaterEdges *e;int cx,cy,tx,ty;uint32_t at;unsigned seed;
    if(!scene || !(e=scene->edges) || !d || !d->loaded || ceiling>1 || !color ||
       (!ceiling && ssa_surface_decals_water(d,color,height)) ||
       !(x>-65536 && x<65536 && y>-65536 && y<65536 && distance>=0 && distance<1536))return 0;
    tx=(int)floor(x/2);ty=(int)floor(y/2);x=tx*2.0+1;y=ty*2.0+1;
    cx=edge_cell(x);cy=edge_cell(y);seed=d->seed^(ceiling?UINT32_C(0xa511e9b3):UINT32_C(0x63d83595));
    if(distance>768){double fade=(1536-distance)/768;
        if((hash((uint32_t)tx^(uint32_t)ty*197^seed)&65535)>=fade*fade*(3-2*fade)*65536)return 0;}
    for(at=e->heads[edge_bucket(cx,cy)];at!=UINT32_MAX;at=e->refs[at].next){
        const WaterEdgeRef *ref=&e->refs[at];const WaterEdge *l;const uint8_t *sector;
        unsigned side,other,joins,bank,u,v;int texture_x,tile,offset;
        double px,py,t,dx,dy,dist2,along,normal;
        if(ref->x!=cx || ref->y!=cy)continue;
        l=&e->lines[ref->edge];px=x-l->x;py=y-l->y;side=l->dx*py-l->dy*px>0;
        if(l->sector[side]==UINT16_MAX)continue;
        sector=e->map->data+e->map->sectors+(size_t)l->sector[side]*20;
        if(edge_s16(sector+ceiling*2)!=height || sector[10+ceiling]!=color)continue;
        other=l->sector[!side];
        if(other!=UINT16_MAX){sector=e->map->data+e->map->sectors+(size_t)other*20;
            if(edge_s16(sector+ceiling*2)==height &&
               (ceiling?edge_s16(sector)<height:edge_s16(sector+2)>height))continue;}
        t=(px*l->dx+py*l->dy)*l->inverse_length2;if(t<0 || t>1)continue;
        dx=px-t*l->dx;dy=py-t*l->dy;dist2=dx*dx+dy*dy;if(dist2>=32*32)continue;
        along=(x*l->dx+y*l->dy)*l->inverse_length;
        normal=(l->y*l->dx-l->x*l->dy)*l->inverse_length;
        if(l->dx<0 || (!l->dx && l->dy<0)){along=-along;normal=-normal;}
        texture_x=(int)floor(along/2)+(int)(seed&63);tile=(int)floor(texture_x/64.0);
        offset=(int)floor(normal/2);if(!surface_edge_present(tile,offset,seed))continue;
        joins=surface_edge_present(tile-1,offset,seed)|(surface_edge_present(tile+1,offset,seed)<<1);
        bank=hash((uint32_t)tile*UINT32_C(0x27d4eb2d)^(uint32_t)offset^seed)&7;
        u=(unsigned)texture_x&63;v=(unsigned)(sqrt(dist2)/2);
        /* Union contributions at corners instead of cutting both tiles at
         * a nearest-wall seam. Neighbor variants have identical port texels. */
        if((surface_edge_tiles[ceiling][bank][joins][v]>>u)&1)return 1;
    }
    return 0;
}
