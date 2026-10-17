#include "ssa/world_visibility.h"
#include "ssa/enemy_targeting.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0]|(unsigned)p[1]<<8); }
static int16_t si16(uint16_t u) { return u<=INT16_MAX?(int16_t)u:(int16_t)(-1-(int)(uint16_t)~u); }
static int16_t rds(const uint8_t *p) { return si16(rd16(p)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p)|(uint32_t)rd16(p+2)<<16; }
static int32_t bits(uint32_t u) { return u<=INT32_MAX?(int32_t)u:-1-(int32_t)~u; }
static int32_t high(int32_t x) { return si16((uint16_t)((uint32_t)x>>16)); }
static int32_t fx(int32_t a,int32_t b) {
    return bits((uint32_t)((uint64_t)((int64_t)a*b)>>16));
}
static int section(const SsaMapAssets *m,uint32_t offset,size_t n,size_t stride) {
    return offset<=m->size && n<=(m->size-offset)/stride;
}
static int valid_map(const SsaMapAssets *m) {
    return m && m->data && m->sector_count && m->sector_count<=32767 &&
        m->node_count<=32767 && m->leaf_count<=32767 && m->segment_count<=32767 &&
        section(m,m->nodes,m->node_count,32) && section(m,m->leaves,m->leaf_count,8) &&
        section(m,m->segments,m->segment_count,16) && section(m,m->vertices,m->vertex_count,4) &&
        section(m,m->lines,m->line_count,40) && section(m,m->sides,m->side_count,12) &&
        section(m,m->sectors,m->sector_count,20) &&
        section(m,m->reject,((size_t)m->sector_count*m->sector_count+7)/8,1);
}
int ssa_visibility_tables_load(SsaVisibilityTables *t,const void *data,size_t size) {
    const uint8_t *rom=data;unsigned i;
    if (!t || !rom || size<0x44aed8) return -1;
    for(i=0;i<65536;++i)t->reciprocal[i]=rd32(rom+0x34934+i*4);
    for(i=0;i<16384;++i)t->tangent[i]=bits(rd32(rom+0x7ca28+i*4));
    memcpy(t->angle_x,rom+0x74934,sizeof(t->angle_x));
    memcpy(t->bbox_corners,rom+0x44aeac,sizeof(t->bbox_corners));
    t->loaded=1;return 1;
}
int ssa_world_visibility_init(SsaWorldVisibility *v,const SsaMapAssets *m,
    const SsaMathTables *math,const SsaVisibilityTables *t,SsaPlanePool *p) {
    SsaWorldVisibility n;
    if (!v || v->updating || v->map || v->stack || v->vertex_angles ||
        v->vertex_valid || v->sector_visited || !valid_map(m) || !math || !t ||
        !t->loaded || !p || !p->capacity || p->capacity>SSA_PLANE_LIMIT) return -1;
    memset(&n,0,sizeof(n));n.map=m;n.math=math;n.tables=t;n.planes=p;
    n.vertex_words=((size_t)m->vertex_count+15)/16;
    n.sector_words=((size_t)m->sector_count+15)/16;
    n.stack_capacity=(size_t)m->node_count+m->leaf_count+2;
    n.vertex_angles=calloc(m->vertex_count?m->vertex_count:1,2);
    n.vertex_valid=calloc(n.vertex_words?n.vertex_words:1,2);
    n.sector_visited=calloc(n.sector_words,2);n.stack=calloc(n.stack_capacity,2);
    if (!n.vertex_angles || !n.vertex_valid || !n.sector_visited || !n.stack) {
        free(n.vertex_angles);free(n.vertex_valid);free(n.sector_visited);free(n.stack);return -1;
    }
    n.head=n.tail=65535;*v=n;return 1;
}
int ssa_world_visibility_free(SsaWorldVisibility *v) {
    if(!v || v->updating)return -1;
    free(v->vertex_angles);free(v->vertex_valid);free(v->sector_visited);free(v->stack);
    memset(v,0,sizeof(*v));return 1;
}
static int valid(const SsaWorldVisibility *v) {
    return v && valid_map(v->map) && v->math && v->tables && v->tables->loaded &&
        v->planes && v->vertex_angles && v->vertex_valid && v->sector_visited && v->stack &&
        v->vertex_words==((size_t)v->map->vertex_count+15)/16 &&
        v->sector_words==((size_t)v->map->sector_count+15)/16 &&
        v->stack_capacity==(size_t)v->map->node_count+v->map->leaf_count+2;
}
static int reset(SsaWorldVisibility *v,const SsaRenderCamera *c,uint8_t counter) {
    if (!valid(v) || !c || c->sector<0 || c->sector>=v->map->sector_count ||
        ssa_plane_pool_reset(v->planes)<0) return -1;
    v->camera=*c;v->game_counter=counter;
    memset(v->vertex_valid,0,v->vertex_words*2);memset(v->sector_visited,0,v->sector_words*2);
    if(v->wall_count>v->high_water)v->high_water=v->wall_count;
    v->wall_count=v->range_count=0;v->head=v->tail=65535;v->solid_count=2;
    v->solid[0].left=-32767;v->solid[0].right=-1;
    v->solid[1].left=120;v->solid[1].right=32767;return 1;
}
int ssa_visibility_reset(SsaWorldVisibility *v,const SsaRenderCamera *c,uint8_t counter) {
    return !v || v->updating?-1:reset(v,c,counter);
}
static uint16_t direction(const SsaMathTables *t,int32_t x,int32_t y) {
    int32_t ax=x<0?-x:x,ay=y<0?-y:y,sum=ax+ay,index;uint16_t a;
    if(!sum)return 0;
    index=(ax*16384)/sum;index=x<0?16384-index:16384+index;
    a=t->direction[index];if(y>0)a=(uint16_t)-a;return (uint16_t)(a+16384);
}
static int frustum(const SsaWorldVisibility *v,uint16_t a,uint16_t b,uint16_t span,
                    SsaSegmentProjection *out) {
    uint16_t ca=a,cb=b,d=(uint16_t)(0x18e4-a);unsigned l,r;
    if(d>0x31c8) {if((uint16_t)(d-0x31c8)>=span)return 0;ca=0xe71c;}
    d=(uint16_t)(b+0x18e4);
    if(d>0x31c8) {if((uint16_t)(d-0x31c8)>=span)return 0;cb=0x18e4;}
    l=(uint16_t)(ca+0x4000);r=(uint16_t)(cb+0x4000);
    if(l>=32768 || r>=32768)return -1;
    l=v->tables->angle_x[l];r=v->tables->angle_x[r];
    if(l==r)return 0;
    if(l>r || r>120)return -1;
    out->left=(uint16_t)l;out->right=(uint16_t)r;out->original_left=a;
    out->original_right=b;out->angle_left=ca;out->angle_right=cb;return 1;
}
static int uncovered(const SsaWorldVisibility *v,int l,int r) {
    unsigned i;
    for(i=0;i<v->solid_count;++i)if(v->solid[i].right>=r)
        return l<v->solid[i].left || r>v->solid[i].right;
    return -1;
}
static int box(SsaWorldVisibility *v,const int16_t b[4]) {
    int x=high(v->camera.x),y=high(v->camera.y),xc,yc,index;uint16_t a,c,span;
    const uint8_t *p;SsaSegmentProjection out;int result;
    xc=x<=b[0]?0:x>=b[2]?2:1;yc=y<=b[1]?0:y>=b[3]?2:1;index=yc*4+xc;
    if(index==5)return 1;
    p=v->tables->bbox_corners+index*4;
    if(p[0]>3 || p[1]>3 || p[2]>3 || p[3]>3)return -1;
    a=(uint16_t)(direction(v->math,b[p[0]]-x,b[p[1]]-y)-v->camera.heading);
    c=(uint16_t)(direction(v->math,b[p[2]]-x,b[p[3]]-y)-v->camera.heading);
    span=(uint16_t)(c-a);if(span&0x8000)return 1;
    result=frustum(v,a,c,span,&out);if(result!=1)return result;
    return uncovered(v,out.left,(int)out.right-1);
}
int ssa_visibility_box(SsaWorldVisibility *v,const int16_t b[4]) {
    if(!valid(v) || v->updating || !b || v->solid_count<2 || v->solid_count>SSA_VISIBILITY_SOLID_LIMIT)return -1;
    return box(v,b);
}
static uint16_t vertex(SsaWorldVisibility *v,unsigned i) {
    if(!(v->vertex_valid[i/16]&(1u<<(i&15)))) {
        const uint8_t *p=v->map->data+v->map->vertices+i*4;
        v->vertex_angles[i]=direction(v->math,rds(p)-high(v->camera.x),rds(p+2)-high(v->camera.y));
        v->vertex_valid[i/16]|=(uint16_t)(1u<<(i&15));
    }
    return v->vertex_angles[i];
}
static int segment(SsaWorldVisibility *v,unsigned i,SsaSegmentProjection *out) {
    const uint8_t *p;uint16_t a,b,span,h,vi,vj;
    if(i>=v->map->segment_count)return -1;
    p=v->map->data+v->map->segments+i*16;h=rd16(p+6);
    if((uint16_t)(h-(uint16_t)(v->camera.heading-0x4000)+0xe71c)>0xce38)return 0;
    vi=rd16(p);vj=rd16(p+2);if(vi>=v->map->vertex_count || vj>=v->map->vertex_count)return -1;
    a=vertex(v,vi);b=vertex(v,vj);span=(uint16_t)(b-a);if(span&0x8000)return 0;
    return frustum(v,(uint16_t)(a-v->camera.heading),(uint16_t)(b-v->camera.heading),span,out);
}
int ssa_visibility_segment(SsaWorldVisibility *v,unsigned i,SsaSegmentProjection *out) {
    if(!valid(v) || v->updating || !out)return -1;
    return segment(v,i,out);
}
static int clip(SsaWorldVisibility *v,int left,int right,int solid) {
    unsigned i,start,end,count=0;int cursor=left,newleft=left,newright=right;
    SsaVisibilityRange spans[SSA_VISIBILITY_SOLID_LIMIT];
    if(left<0 || left>right || right>120 || v->solid_count<2 ||
        v->solid_count>SSA_VISIBILITY_SOLID_LIMIT || v->range_count>SSA_VISIBILITY_RANGE_LIMIT)return -1;
    for(i=0;i<v->solid_count && cursor<=right;++i) {
        int l=v->solid[i].left,r=v->solid[i].right;
        if(r<cursor)continue;
        if(l>cursor) {spans[count].left=(int16_t)cursor;spans[count++].right=(int16_t)(right<l?right:l-1);}
        if(r>=right)break;
        cursor=r+1;
    }
    if(v->range_count+count>SSA_VISIBILITY_RANGE_LIMIT)return -1;
    if(solid) {
        for(start=0;start<v->solid_count && v->solid[start].right<left-1;++start){}
        end=start;
        while(end<v->solid_count && v->solid[end].left<=right+1) {
            if(v->solid[end].left<newleft)newleft=v->solid[end].left;
            if(v->solid[end].right>newright)newright=v->solid[end].right;
            ++end;
            if(v->solid[end-1].right>=right)break;
        }
        /* AC8's compaction copies the word AT the old end pointer too. The
         * resulting extra trailing entry is retained, beyond the sentinel. */
        unsigned extra=end-start>1?1u:0u;
        if(v->solid_count+extra>=SSA_VISIBILITY_SOLID_LIMIT ||
            v->solid_count-(end-start)+1+extra>SSA_VISIBILITY_SOLID_LIMIT)return -1;
        memmove(v->solid+start+1,v->solid+end,(v->solid_count-end+extra)*sizeof(*v->solid));
        v->solid[start].left=(int16_t)newleft;v->solid[start].right=(int16_t)newright;
        v->solid_count=(uint16_t)(v->solid_count-(end-start)+1+extra);
    }
    memcpy(v->ranges+v->range_count,spans,count*sizeof(*spans));v->range_count=(uint16_t)(v->range_count+count);
    return (int)count;
}
int ssa_visibility_clip(SsaWorldVisibility *v,int left,int right,int solid) {
    if(!valid(v) || v->updating)return -1;
    return clip(v,left,right,solid);
}
static int32_t tangent(const SsaVisibilityTables *t,uint16_t a) {
    if(a<=0x3fff)return t->tangent[a];
    return bits(0u-(uint32_t)t->tangent[(uint16_t)-a&0x3fff]);
}
static int32_t depth(SsaWorldVisibility *v,uint16_t angle,uint16_t normal,int32_t distance) {
    int32_t a,b,num,den;uint32_t reciprocal;
    ssa_sincos(v->math,(uint16_t)(0x4000-angle),&a,NULL);
    ssa_sincos(v->math,(uint16_t)(angle-normal+0x4000),&b,NULL);
    if(a<0)a=-a;
    if(b<0)b=-b;
    num=fx(0x55b05a,b);den=fx(a,distance);
    reciprocal=(uint32_t)den<=65535?v->tables->reciprocal[(uint32_t)den]>>8:0;
    return (int32_t)((uint32_t)fx(num,bits(reciprocal))>>8);
}
static int enqueue(SsaWorldVisibility *v,unsigned index,const SsaSegmentProjection *projection) {
    const SsaMapAssets *m=v->map;const uint8_t *seg=m->data+m->segments+index*16,*line,*side,*front,*back=NULL,*vert;
    unsigned li=rd16(seg+4),sd=rd16(seg+8),vi=rd16(seg),slot;int si,fronti,backi=-1,count,mode;
    int32_t sine,cosine,dx,dy,distance;uint16_t normal,relative;uint32_t origin;
    SsaVisibleWall *w;
    if(li>=m->line_count || sd>1 || vi>=m->vertex_count)return -1;
    line=m->data+m->lines+li*40;si=rds(line+8+sd*2);if(si<0)return 1;
    if(si>=m->side_count)return -1;
    side=m->data+m->sides+(unsigned)si*12;
    fronti=rds(line+12+sd*2);if(fronti<0 || fronti>=m->sector_count)return -1;
    front=m->data+m->sectors+(unsigned)fronti*20;v->front_sector=(int16_t)fronti;
    v->front_floor=si16((uint16_t)(rd16(front)-high(v->camera.z)));
    v->front_ceiling=si16((uint16_t)(rd16(front+2)-high(v->camera.z)));
    if(v->front_floor!=v->front_ceiling) {
        v->floor=v->front_floor<0?ssa_plane_find(v->planes,rds(front),front[10]):NULL;
        if(v->front_floor<0 && !v->floor)return -1;
        v->ceiling=v->front_ceiling>0?ssa_plane_find(v->planes,rds(front+2),front[11]):NULL;
        if(v->front_ceiling>0 && !v->ceiling)return -1;
    }
    if(rd16(line+26)&4) {
        backi=rds(line+12+(sd^1)*2);if(backi<0 || backi>=m->sector_count)return -1;
        back=m->data+m->sectors+(unsigned)backi*20;
        v->back_floor=si16((uint16_t)(rd16(back)-high(v->camera.z)));
        v->back_ceiling=si16((uint16_t)(rd16(back+2)-high(v->camera.z)));
    }
    v->back_sector=(int16_t)backi;
    if(!back)mode=0;
    else if(v->front_floor!=v->back_floor || v->front_ceiling!=v->back_ceiling)mode=1;
    else {
        if(rds(side+6)<0 && rd16(front+10)==rd16(back+10))return 1;
        mode=2;
    }
    count=clip(v,projection->left,projection->right,mode==0);if(count<0)return -1;if(!count)return 1;
    if(v->wall_count>=SSA_VISIBILITY_WALL_LIMIT)return -1;
    slot=v->wall_count++;w=v->walls+slot;
    origin=(uint32_t)rd16(seg+10)<<16;if(!origin)origin=(uint32_t)rd16(side)<<16;
    if(rd16(line+26)&0x200)origin+=(uint32_t)v->game_counter<<16;
    normal=(uint16_t)(rd16(seg+6)-0x4000);ssa_sincos(v->math,normal,&sine,&cosine);
    vert=m->data+m->vertices+vi*4;dx=rds(vert)-high(v->camera.x);dy=rds(vert+2)-high(v->camera.y);
    distance=bits((uint32_t)((uint64_t)((int64_t)sine*dx+(int64_t)cosine*dy)>>16));
    ssa_sincos(v->math,(uint16_t)(rd16(seg+6)+0x8000),&sine,&cosine);
    origin+=(uint32_t)sine*(uint32_t)dx+(uint32_t)cosine*(uint32_t)dy;
    relative=(uint16_t)(normal-v->camera.heading);
    w->u_left=bits((uint32_t)tangent(v->tables,(uint16_t)(projection->angle_left-relative))*(uint32_t)distance+origin);
    w->u_right=bits((uint32_t)tangent(v->tables,(uint16_t)(projection->angle_right-relative))*(uint32_t)distance+origin-0x1000);
    w->depth_left=depth(v,projection->angle_left,relative,distance);
    w->depth_right=depth(v,projection->angle_right,relative,distance);
    if(w->depth_left>0x80000 || w->depth_right>0x80000)return 1;
    w->left=(uint8_t)projection->left;w->right=(uint8_t)projection->right;
    w->segment_flags=(uint16_t)(index|(mode==0?0x8000:0));w->range_count=(uint16_t)count;
    w->range_start=(uint16_t)(v->range_count-count);w->front_sector=(uint16_t)fronti;
    w->ceiling=v->ceiling;w->floor=v->floor;w->distance=distance;
    w->previous=v->tail;w->next=65535;
    if(v->tail!=65535)v->walls[v->tail].next=(uint16_t)slot;else v->head=(uint16_t)slot;
    v->tail=(uint16_t)slot;return 1;
}
static int leaf(SsaWorldVisibility *v,unsigned index) {
    const SsaMapAssets *m=v->map;const uint8_t *l;unsigned sectorid,count,first,i;int result;
    if(index>=m->leaf_count)return -1;
    l=m->data+m->leaves+index*8;
    sectorid=rd16(l+4);if(sectorid>=m->sector_count)return -1;
    result=ssa_sector_reject(m,v->camera.sector,(int)sectorid);if(result<0)return -1;if(result)return 1;
    v->sector_visited[sectorid/16]|=(uint16_t)(1u<<(sectorid&15));count=rd16(l);first=rd16(l+2);
    if(first>m->segment_count || count>m->segment_count-first)return -1;
    for(i=0;i<count;++i) {
        SsaSegmentProjection p;result=segment(v,first+i,&p);
        if(result<0 || (result && enqueue(v,first+i,&p)<0))return -1;
    }
    return 1;
}
int ssa_world_visibility_build(SsaWorldVisibility *v,const SsaRenderCamera *camera,uint8_t counter) {
    const SsaMapAssets *m;size_t top=0,steps=0;unsigned index;int result=-1;
    if(!v || v->updating || reset(v,camera,counter)<0)return -1;
    v->updating=1;m=v->map;
    if(!m->node_count){result=1;goto done;}
    v->stack[top++]=(uint16_t)(m->node_count-1);
    while(top) {
        const uint8_t *node;unsigned near,far,child;int16_t bounds[4];int visible;
        index=v->stack[--top];if(++steps>(size_t)m->node_count+m->leaf_count+1)goto done;
        if(index==65535)break;
        if(index&0x8000){if(leaf(v,index&0x7fff)<0)goto done;continue;}
        if(index>=m->node_count)goto done;
        node=m->data+m->nodes+index*32;
        near=(unsigned)ssa_bsp_side(camera->x,camera->y,rds(node),rds(node+2),rds(node+4),rds(node+6));far=near^1;
        for(unsigned j=0;j<4;++j)bounds[j]=rds(node+8+far*8+j*2);
        visible=box(v,bounds);if(visible<0)goto done;
        if(visible) {
            child=rd16(node+24+far*2);if(!(child&0x8000) && child>=index)goto done;
            if(top>=v->stack_capacity)goto done;
        v->stack[top++]=(uint16_t)child;
        }
        child=rd16(node+24+near*2);if(!(child&0x8000) && child>=index)goto done;
        if(top>=v->stack_capacity)goto done;
        v->stack[top++]=(uint16_t)child;
    }
    result=1;
done:v->updating=0;return result;
}


