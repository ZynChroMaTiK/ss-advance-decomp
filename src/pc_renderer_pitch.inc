/* PC camera rotation. World-space polygons, near clipping and reciprocal-depth
 * interpolation: pitch is not a horizon offset or a warped old framebuffer. */
typedef SsaPcVertex PitchVertex;
typedef struct PitchPoint {double x,y;} PitchPoint;
typedef struct PitchPlane {PitchPoint p[3];unsigned sector;} PitchPlane;
typedef struct HardwareRowStats {unsigned actors,walls;} HardwareRowStats;
#include "pc_actor_clip.inc"
typedef struct ReflectionSpan {unsigned key,first,last;int next;} ReflectionSpan;
typedef struct SsaPcGeometry {
    ActorClipScratch actor_clip;
    ReflectionSpan *reflection_spans;size_t reflection_capacity;
    PitchPlane *planes;size_t count,capacity;
    uint64_t signature;
    uint32_t *sky_axes;unsigned sky_width,sky_height;
    HardwareRowStats *row_stats;unsigned row_capacity;
    SsaGodrayPalette godray_palette;
    uint8_t godray_nearest[32768],godray_valid[4096];
} SsaPcGeometry;
static void geometry_free(SsaPcRenderer *r)
{if(r->geometry){actor_clip_free(&r->geometry->actor_clip);free(r->geometry->planes);free(r->geometry->reflection_spans);free(r->geometry->sky_axes);free(r->geometry->row_stats);free(r->geometry);r->geometry=NULL;}}
typedef SsaPcMaterial PitchMaterial;
static PitchVertex pitch_vertex(const Camera *c,double x,double y,double z,double u,double v)
{
    double dx=x-c->x,dy=y-c->y,d=dx*c->sine+dy*c->cosine,h=z-c->z;
    PitchVertex p={dx*c->cosine-dy*c->sine,h*c->pc-d*c->ps,d*c->pc+h*c->ps,u,v};return p;
}
static double orient(double ax,double ay,double bx,double by,double x,double y)
{return (bx-ax)*(y-ay)-(by-ay)*(x-ax);}
static void pitch_triangle(SsaPcRenderer *r,const Camera *c,PitchVertex a,PitchVertex b,PitchVertex d,
    const PitchMaterial *m)
{
    if(r->hardware){PitchVertex vertices[3]={a,b,d};
        if(!r->hardware_error && r->hardware->triangle(r->hardware->context,vertices,m)<0)r->hardware_error=1;
        return;
    }
    double ax=r->width/2.0+a.x*c->focal/a.z,ay=r->height/2.0-a.y*c->focal/a.z;
    double bx=r->width/2.0+b.x*c->focal/b.z,by=r->height/2.0-b.y*c->focal/b.z;
    double dx=r->width/2.0+d.x*c->focal/d.z,dy=r->height/2.0-d.y*c->focal/d.z;
    double area=orient(ax,ay,bx,by,dx,dy),minx=ax,maxx=ax,miny=ay,maxy=ay;
    double aq=1/a.z,bq=1/b.z,dq=1/d.z,qx,ux,vx,qy,uy,vy;
    double au=a.u*aq,bu=b.u*bq,du=d.u*dq,av=a.v*aq,bv=b.v*bq,dv=d.v*dq;
    double xx[3]={ax,bx,dx},yy[3]={ay,by,dy};int top,bottom,y;unsigned i;
    if(area>-1e-10 && area<1e-10)return;
    if(bx<minx)minx=bx;
    if(dx<minx)minx=dx;
    if(bx>maxx)maxx=bx;
    if(dx>maxx)maxx=dx;
    if(by<miny)miny=by;
    if(dy<miny)miny=dy;
    if(by>maxy)maxy=by;
    if(dy>maxy)maxy=dy;
    if(maxx<=0 || minx>=r->width)return;
    top=edge(miny,r->height);bottom=edge(maxy,r->height);
    if(top<(int)c->top)top=(int)c->top;
    if(bottom>(int)c->bottom)bottom=(int)c->bottom;
    if(top>=bottom)return;
    qx=((by-dy)*(aq-dq)+(dy-ay)*(bq-dq))/area;
    ux=((by-dy)*(au-du)+(dy-ay)*(bu-du))/area;
    vx=((by-dy)*(av-dv)+(dy-ay)*(bv-dv))/area;
    qy=((dx-bx)*(aq-dq)+(ax-dx)*(bq-dq))/area;
    uy=((dx-bx)*(au-du)+(ax-dx)*(bu-du))/area;
    vy=((dx-bx)*(av-dv)+(ax-dx)*(bv-dv))/area;
    for(y=top;y<bottom;++y){
        double py=y+0.5,lo=DBL_MAX,hi=-DBL_MAX,wa,wb,q,uq,vq;int left,right,x;
        for(i=0;i<3;++i){unsigned j=(i+1)%3;
            if((yy[i]<=py && py<yy[j]) || (yy[j]<=py && py<yy[i])){
                double crossing=xx[i]+(xx[j]-xx[i])*(py-yy[i])/(yy[j]-yy[i]);
                if(crossing<lo)lo=crossing;
                if(crossing>hi)hi=crossing;
            }}
        if(lo>hi)continue;
        left=edge(lo,r->width);right=edge(hi,r->width);
        wa=orient(bx,by,dx,dy,left+0.5,py)/area;wb=orient(dx,dy,ax,ay,left+0.5,py)/area;
        q=dq+wa*(aq-dq)+wb*(bq-dq);uq=du+wa*(au-du)+wb*(bu-du);vq=dv+wa*(av-dv)+wb*(bv-dv);
        for(x=left;x<right;++x,q+=qx,uq+=ux,vq+=vx){
        double z;size_t at=(size_t)y*r->width+(unsigned)x;unsigned color=m->color;
        if(q<=0 || (!m->actor && q*((double)r->depth[at]+0.001)<1))continue;
        z=1/q;
        if(m->pixels){double u=uq*z,v=vq*z;
            if(m->transparent && (u<0 || v<0 || u>=m->width || v>=m->height))continue;
            {const uint8_t *pixels=m->pixels;unsigned width=m->width,height=m->height;
                if(m->mip_pixels[0]){double dux=(ux-u*qx)*z,dvx=(vx-v*qx)*z,duy=(uy-u*qy)*z,dvy=(vy-v*qy)*z;
                    double footprint2=fmax(dux*dux+dvx*dvx,duy*duy+dvy*dvy);
                    unsigned level=footprint2>=16?2:footprint2>=4?1:0;
                    if(level){pixels=m->mip_pixels[level-1];width=m->mip_width[level-1];height=m->mip_height[level-1];
                        u*= (double)width/m->width;v*= (double)height/m->height;}}
                color=pixels[(size_t)wrap(u,width)*height+wrap(v,height)];}
            if(m->transparent && !color)continue;
        }
        if(c->water && at<c->water->capacity)c->water->mask[at]=0;
        if(m->decals==1 && c->water && ssa_surface_decals_water(c->water->decals,color,m->height_z))
            r->pixels[at]=rgb(ssa_water_rgb_cached(c->water,c->samples,(uint8_t)color,uq*z,vq*z,m->height_z,z,at));
        else {if(m->decals && c->decals && !(m->decals==1 && ssa_surface_decals_water(c->decals,color,m->height_z)))
                color=ssa_surface_decals_sample_cached(c->decals,c->samples,m->decals-1,(uint8_t)color,uq*z,vq*z,m->height_z,z);
            if(!m->sky)r->pixels[at]=c->palette[color];}
        r->depth[at]=(float)z;
        if(m->actor)++r->actor_pixels;else if(m->pixels)++r->wall_pixels;
    }}
}
static void pitch_polygon(SsaPcRenderer *r,const Camera *c,const PitchVertex *v,unsigned n,const PitchMaterial *m)
{
    PitchVertex clipped[10],above[10];unsigned i,count=0;
    if(c->clip){
        for(i=0;i<n;++i){PitchVertex a=v[i],b=v[(i+1)%n];
            double da=c->z+a.y*c->pc+a.z*c->ps-c->clip_z,db=c->z+b.y*c->pc+b.z*c->ps-c->clip_z;
            int ai=da>=0,bi=db>=0;
            if(ai)above[count++]=a;
            if(ai!=bi){double t=da/(da-db);
                above[count++]=(PitchVertex){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,
                    a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};}}
        v=above;n=count;count=0;
    }
    /* Input is a triangle or quad. Clipping before division also covers walls
     * and actors crossing the camera, and pitch views past the old horizon. */
    for(i=0;i<n;++i){PitchVertex a=v[i],b=v[(i+1)%n];int ai=a.z>=0.05,bi=b.z>=0.05;
        if(ai)clipped[count++]=a;
        if(ai!=bi){double t=(0.05-a.z)/(b.z-a.z);
            clipped[count++]=(PitchVertex){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,0.05,
                a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};}}
    for(i=1;i+1<count;++i)pitch_triangle(r,c,clipped[0],clipped[i],clipped[i+1],m);
}
static int flat_sky(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c)
{
    SsaPcGeometry *cache=r->geometry;
    unsigned x,y,top=c->top,bottom=c->bottom;
    const uint8_t *sky=g->sky.pixels[ssa_texture_assets_family(g->level)];
    if(cache->sky_width!=r->width || cache->sky_height!=r->height){
        size_t count=(size_t)r->width+r->height;uint32_t *axes;
        if(count>SIZE_MAX/sizeof(*axes))return -1;
        axes=malloc(count*sizeof(*axes));if(!axes)return -1;
        free(cache->sky_axes);cache->sky_axes=axes;cache->sky_width=r->width;cache->sky_height=r->height;
        ++r->sky_builds;
    }
    /* Independent axes keep texture rows/columns straight at every pitch.
     * Refresh both lookups without reallocating as yaw/pitch change. */
    for(x=0;x<r->width;++x)cache->sky_axes[x]=sky_column(g,r,x);
    for(y=top;y<bottom;++y)cache->sky_axes[r->width+y]=sky_row(r,y,c->sky_shift)*768;
    for(y=top;y<bottom;++y){const uint8_t *row=sky+cache->sky_axes[r->width+y];
        uint32_t *out=r->pixels+(size_t)y*r->width;
        /* Upscaled/clamped sky rows are identical within this draw. Copy
         * their resolved colors; never retain them across palette changes. */
        if(y>top && cache->sky_axes[r->width+y]==cache->sky_axes[r->width+y-1])
            memcpy(out,out-r->width,(size_t)r->width*sizeof(*out));
        else for(x=0;x<r->width;++x)out[x]=c->palette[row[cache->sky_axes[x]]];}
    if(r->hardware){r->hardware_error=0;
        if(r->hardware->begin(r->hardware->context,r,c->palette,c->focal,c->top,c->bottom)<0)return -1;}
    return 1;
}
static unsigned pitch_clip(PitchPoint *out,const PitchPoint *p,unsigned n,
    double ox,double oy,double dx,double dy,int positive)
{
    unsigned i,count=0;
    for(i=0;i<n;++i){PitchPoint a=p[i],b=p[(i+1)%n];
        double da=dx*(a.y-oy)-dy*(a.x-ox),db=dx*(b.y-oy)-dy*(b.x-ox);
        int ai=positive?da>=0:da<=0,bi=positive?db>=0:db<=0;
        if(ai)out[count++]=a;
        if(ai!=bi){double t=da/(da-db);out[count++]=(PitchPoint){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};}}
    return count;
}
static int pitched_leaf(SsaPcGeometry *cache,const SsaNativeGame *g,
    unsigned index,const PitchPoint *points,unsigned count)
{
    const SsaMapAssets *m=&g->map;const uint8_t *leaf;PitchPoint *a,*b,*swap;
    unsigned first,n,i,capacity;
    if(index>=m->leaf_count)return -1;
    leaf=m->data+m->leaves+(size_t)index*8;first=u16(leaf+2);n=u16(leaf);
    if(u16(leaf+4)>=m->sector_count || first+n>m->segment_count)return -1;
    capacity=count+n+2;a=malloc((size_t)capacity*2*sizeof(*a));if(!a)return -1;b=a+capacity;
    memcpy(a,points,(size_t)count*sizeof(*a));
    /* BSP cuts close implicit subsector edges. Actual wall halfplanes close
     * the map exterior; segment endpoint hulls alone leave missing floor wedges. */
    for(i=0;i<n && count>=3;++i){const uint8_t *seg=m->data+m->segments+(size_t)(first+i)*16;
        const uint8_t *line,*v,*w;unsigned li=u16(seg+4),side=u16(seg+8);
        if(li>=m->line_count || side>1){free(a<b?a:b);return -1;}
        line=m->data+m->lines+(size_t)li*40;v=m->data+m->vertices+(size_t)u16(line)*4;
        w=m->data+m->vertices+(size_t)u16(line+2)*4;
        count=pitch_clip(b,a,count,s16(v),s16(v+2),s16(w)-s16(v),s16(w+2)-s16(v+2),(int)side);
        swap=a;a=b;b=swap;
    }
    for(i=1;i+1<count;++i){
        if(cache->count==cache->capacity){size_t cap=cache->capacity?cache->capacity*2:256;PitchPlane *p;
            if(cap>SIZE_MAX/sizeof(*p)){free(a<b?a:b);return -1;}
            p=realloc(cache->planes,cap*sizeof(*p));if(!p){free(a<b?a:b);return -1;}
            cache->planes=p;cache->capacity=cap;}
        cache->planes[cache->count++]=(PitchPlane){{a[0],a[i],a[i+1]},u16(leaf+4)};
    }
    free(a<b?a:b);return 1;
}
static int pitched_bsp(SsaPcGeometry *cache,const SsaNativeGame *g,
    unsigned index,const PitchPoint *points,unsigned count,unsigned remaining)
{
    const uint8_t *node;PitchPoint *scratch;unsigned side,n;int result=1;
    if(count<3)return 1;
    if(index&0x8000)return pitched_leaf(cache,g,index&0x7fff,points,count);
    if(!remaining || index>=g->map.node_count)return -1;
    node=g->map.data+g->map.nodes+(size_t)index*32;
    scratch=malloc((size_t)(count+2)*sizeof(*scratch));if(!scratch)return -1;
    for(side=0;side<2 && result>0;++side){
        n=pitch_clip(scratch,points,count,s16(node),s16(node+2),s16(node+4),s16(node+6),(int)side);
        result=pitched_bsp(cache,g,u16(node+24+side*2),scratch,n,remaining-1);}
    free(scratch);return result;
}
static uint64_t geometry_signature(const SsaMapAssets *m)
{
    uint64_t hash=UINT64_C(14695981039346656037);size_t i;unsigned j;
    const uint32_t offsets[]={m->nodes,m->leaves,m->segments,m->vertices};
    const size_t sizes[]={(size_t)m->node_count*32,(size_t)m->leaf_count*8,(size_t)m->segment_count*16,(size_t)m->vertex_count*4};
    for(j=0;j<4;++j){hash=(hash^sizes[j])*UINT64_C(1099511628211);
        for(i=0;i<sizes[j];++i)hash=(hash^m->data[offsets[j]+i])*UINT64_C(1099511628211);}
    for(j=0;j<m->line_count;++j)for(i=0;i<4;++i)
        hash=(hash^m->data[m->lines+(size_t)j*40+i])*UINT64_C(1099511628211);
    return hash;
}
static int pitched_geometry(SsaPcRenderer *r,const SsaNativeGame *g)
{
    const SsaMapAssets *m=&g->map;uint64_t signature=geometry_signature(m);SsaPcGeometry *cache;
    const PitchPoint bounds[4]={{-32768,-32768},{32767,-32768},{32767,32767},{-32768,32767}};
    if(r->geometry && r->geometry->signature==signature)return 1;
    cache=calloc(1,sizeof(*cache));if(!cache)return -1;
    if(pitched_bsp(cache,g,m->node_count?m->node_count-1u:0x8000u,bounds,4,m->node_count+1)<0){free(cache->planes);free(cache);return -1;}
    cache->signature=signature;geometry_free(r);r->geometry=cache;++r->geometry_builds;return 1;
}
static void pitched_planes(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c)
{
    size_t i;unsigned j,k;PitchMaterial material={0};
    for(i=0;i<r->geometry->count;++i){const PitchPlane *p=&r->geometry->planes[i];
        const uint8_t *sector=g->map.data+g->map.sectors+(size_t)p->sector*20;
        for(j=0;j<2;++j){int height=s16(sector+j*2);PitchVertex tri[3];
            if(c->clip && height<=c->clip_z)continue;
            if((!j && c->z<height) || (j && c->z>height))continue;
            material.color=sector[10+j];material.sky=!material.color;
            material.decals=(c->decals || c->water) && material.color?j+1:0;material.height_z=height;
            for(k=0;k<3;++k)tri[k]=pitch_vertex(c,p->p[k].x,p->p[k].y,height,p->p[k].x,p->p[k].y);
            pitch_polygon(r,c,tri,3,&material);
        }}
}
static void pitched_wall(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,
    double ax,double ay,double bx,double by,double low,double high,double origin,double along,
    double len,unsigned side,unsigned layer)
{
    const SsaTextureImage *im=texture(g,side,layer);PitchMaterial material={0};PitchVertex q[4];
    if(high<=low)return;
    if(im){unsigned level;material.pixels=im->pixels;material.width=im->width;material.height=im->height;
        if(g->mipmaps || g->gba_view)for(level=1;level<3;++level){const SsaTextureImage *lower=texture_level(g,side,layer,level);
            if(!lower)lower=im;
            material.mip_pixels[level-1]=lower->pixels;material.mip_width[level-1]=lower->width;material.mip_height[level-1]=lower->height;}}
    q[0]=pitch_vertex(c,ax,ay,low,along/2,(origin-low)/2);
    q[1]=pitch_vertex(c,bx,by,low,(along+len)/2,(origin-low)/2);
    q[2]=pitch_vertex(c,bx,by,high,(along+len)/2,(origin-high)/2);
    q[3]=pitch_vertex(c,ax,ay,high,along/2,(origin-high)/2);
    pitch_polygon(r,c,q,4,&material);
}
static int pitched_world(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c)
{
    const SsaMapAssets *m=&g->map;unsigned i;
    if(flat_sky(r,g,c)<0)return -1;
    pitched_planes(r,g,c);
    for(i=0;i<m->line_count;++i){const uint8_t *l=m->data+m->lines+(size_t)i*40,*a,*b,*s,*side,*back;
        unsigned direction,si,sector,flags=u16(l+26);double ax,ay,bx,by,len,along,origin;int ff,fc,bf,bc;
        a=m->data+m->vertices+(size_t)u16(l)*4;b=m->data+m->vertices+(size_t)u16(l+2)*4;
        ax=s16(a);ay=s16(a+2);bx=s16(b);by=s16(b+2);
        direction=orient(ax,ay,bx,by,c->x,c->y)>0;
        if(direction && !(flags&4))continue;
        si=u16(l+8+direction*2);sector=u16(l+12+direction*2);if(si>=m->side_count || sector>=m->sector_count)continue;
        if(direction){double t=ax;ax=bx;bx=t;t=ay;ay=by;by=t;}
        side=m->data+m->sides+(size_t)si*12;s=m->data+m->sectors+(size_t)sector*20;
        ff=s16(s);fc=s16(s+2);len=length(bx-ax,by-ay);along=s16(side);
        if(!(flags&4)){const SsaTextureImage *im=texture(g,si,SSA_MAP_SIDE_MIDDLE);
            origin=(flags&16) && im?ff+im->height*2.0:fc;
            pitched_wall(r,g,c,ax,ay,bx,by,ff,fc,origin+s16(side+2),along,len,si,SSA_MAP_SIDE_MIDDLE);continue;}
        sector=u16(l+12+(direction^1u)*2);if(sector>=m->sector_count)continue;
        back=m->data+m->sectors+(size_t)sector*20;bf=s16(back);bc=s16(back+2);
        if(bc<fc){const SsaTextureImage *im=texture(g,si,SSA_MAP_SIDE_UPPER);
            origin=(flags&8) || !u16(l+36)?fc:bc+(im?im->height*2.0:0);
            pitched_wall(r,g,c,ax,ay,bx,by,bc>ff?bc:ff,fc,origin+s16(side+2),along,len,si,SSA_MAP_SIDE_UPPER);}
        if(bf>ff)pitched_wall(r,g,c,ax,ay,bx,by,ff,bf<fc?bf:fc,bf+s16(side+2),along,len,si,SSA_MAP_SIDE_LOWER);
    }
    return 1; /* This path rasterizes geometry; it does not cast column rays. */
}
/* Camera transforms and live portal heights are shared by all sprite layers
 * in this view. Actor origin/height gates remain in actor_silhouette. */
static int actor_clip_prepare(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c)
{
    const SsaMapAssets *map=&g->map;ActorClipScratch *scratch=&r->geometry->actor_clip;unsigned i;
    if(actor_clip_reserve(scratch,map->line_count)<0)return -1;
    scratch->view_count=0;
    for(i=0;i<map->line_count;++i){const uint8_t *line=map->data+map->lines+(size_t)i*40,*a,*b,*sector;
        ActorViewWall w={0};unsigned direction,si;double t;
        a=map->data+map->vertices+(size_t)u16(line)*4;b=map->data+map->vertices+(size_t)u16(line+2)*4;
        w.ax=s16(a);w.ay=s16(a+2);w.bx=s16(b);w.by=s16(b+2);w.flags=u16(line+26);
        w.cam_side=orient(w.ax,w.ay,w.bx,w.by,c->x,c->y);direction=w.cam_side>0;
        if(direction && !(w.flags&4))continue;
        w.ad=(w.ax-c->x)*c->sine+(w.ay-c->y)*c->cosine;
        w.bd=(w.bx-c->x)*c->sine+(w.by-c->y)*c->cosine;
        if(fmax(w.ad,w.bd)<=0.05)continue;
        si=u16(line+12+direction*2);if(si>=map->sector_count)continue;
        sector=map->data+map->sectors+(size_t)si*20;w.ff=s16(sector);w.fc=s16(sector+2);
        if(w.flags&4){si=u16(line+12+(direction^1u)*2);if(si>=map->sector_count)continue;
            sector=map->data+map->sectors+(size_t)si*20;w.bf=s16(sector);w.bc=s16(sector+2);}
        w.al=(w.ax-c->x)*c->cosine-(w.ay-c->y)*c->sine;
        w.bl=(w.bx-c->x)*c->cosine-(w.by-c->y)*c->sine;
        w.near_ad=w.ad;w.near_bd=w.bd;
        if(w.near_ad<0.05){t=(0.05-w.near_ad)/(w.near_bd-w.near_ad);w.al+=(w.bl-w.al)*t;w.near_ad=0.05;}
        if(w.near_bd<0.05){t=(0.05-w.near_bd)/(w.near_ad-w.near_bd);w.bl+=(w.al-w.bl)*t;w.near_bd=0.05;}
        scratch->view[scratch->view_count++]=w;
    }
    return 1;
}
/* Each projected wall contributes a linear top/bottom bound over its X
 * interval. Split at interval endpoints, intersect bounds, then submit only
 * visible triangles. No screen-resolution mask or per-pixel world-depth bias. */
static void actor_silhouette(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,
    const SsaBody *body,double left,double right,double top,double bottom,
    unsigned flip,double unit,const PitchMaterial *material)
{
    ActorClipScratch *scratch=&r->geometry->actor_clip;
    double x=body->x/65536.0,y=body->y/65536.0;
    double depth=(x-c->x)*c->sine+(y-c->y)*c->cosine;
    double lateral=(x-c->x)*c->cosine-(y-c->y)*c->sine;
    unsigned i,j,count=0,cut_count=2;size_t stride=2*(scratch->capacity-1)+8;
    (void)g;
    if(depth<=0.05)return;
    left+=lateral;right+=lateral;
    scratch->cuts[0]=left;scratch->cuts[1]=right;
    for(i=0;i<scratch->view_count;++i){const ActorViewWall *v=&scratch->view[i];
        double ad=v->ad,bd=v->bd,al,bl,lo,hi,actor_side;unsigned clip=0;
        int ff=v->ff,fc=v->fc,bf=v->bf,bc=v->bc;double low=0,high=0;ActorClipWall wall={0};
        if(depth<fmin(ad,bd))continue;
        actor_side=orient(v->ax,v->ay,v->bx,v->by,x,y);
        if(depth<=fmax(ad,bd) && ((actor_side>0)==(v->cam_side>0)))continue;
        if(!(v->flags&4) || bc<=ff || bf>=fc)clip=4;
        else {
            if((ff>bf && body->z/65536.0<ff) || (ff<=bf && bf>c->z))clip|=1;
            if((fc<bc && floor((body->z+(double)body->height)/65536.0)>fc) || (fc>=bc && bc<c->z))clip|=2;
            low=fmax(ff,bf);high=fmin(fc,bc);
        }
        if(!clip)continue;
        ad=v->near_ad;bd=v->near_bd;al=v->al;bl=v->bl;
        al*=depth/ad;bl*=depth/bd;
        if(fabs(bl-al)<1e-9)continue;
        lo=fmax(left,fmin(al,bl));hi=fmin(right,fmax(al,bl));if(hi<=lo)continue;
        wall.left=lo;wall.right=hi;wall.flags=clip;
        wall.bottom_slope=(low-c->z)*depth*(1/bd-1/ad)/(bl-al);
        wall.bottom_offset=c->z+(low-c->z)*depth/ad-wall.bottom_slope*al;
        wall.top_slope=(high-c->z)*depth*(1/bd-1/ad)/(bl-al);
        wall.top_offset=c->z+(high-c->z)*depth/ad-wall.top_slope*al;
        scratch->walls[count++]=wall;scratch->cuts[cut_count++]=lo;scratch->cuts[cut_count++]=hi;
    }
    qsort(scratch->cuts,cut_count,sizeof(*scratch->cuts),actor_cut_compare);
    for(i=1;i<cut_count;++i){double lo=scratch->cuts[i-1],hi=scratch->cuts[i],mid=(lo+hi)*0.5;
        ActorClipPoint *p=scratch->points,*out=p+stride;unsigned n=4;
        if(hi-lo<1e-9)continue;
        p[0]=(ActorClipPoint){lo,top};p[1]=(ActorClipPoint){hi,top};
        p[2]=(ActorClipPoint){hi,bottom};p[3]=(ActorClipPoint){lo,bottom};
        for(j=0;j<count && n;++j){const ActorClipWall *w=&scratch->walls[j];ActorClipPoint *swap;
            if(mid<w->left || mid>w->right)continue;
            if(w->flags&4){n=0;break;}
            if(w->flags&1){n=actor_clip_height(p,n,out,w->bottom_slope,w->bottom_offset,1);swap=p;p=out;out=swap;}
            if(w->flags&2){n=actor_clip_height(p,n,out,w->top_slope,w->top_offset,0);swap=p;p=out;out=swap;}
        }
        for(j=1;j+1<n;++j){PitchVertex tri[3];unsigned k,indices[3]={0,j,j+1};
            for(k=0;k<3;++k){ActorClipPoint a=p[indices[k]];double u=(a.x-left)/unit;
                tri[k]=pitch_vertex(c,x+(a.x-lateral)*c->cosine,y-(a.x-lateral)*c->sine,a.z,
                    flip?material->width-u:u,(top-a.z)/unit);}
            pitch_polygon(r,c,tri,3,material);
        }
    }
}
static void pitched_actor(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,const SsaActorSource *s)
{
    const SsaActorResource *resource;const SsaActorImage *im;const SsaBody *body;
    unsigned dirs,flip=0,image,bit;int dir,cut=0;uint16_t facing;double unit,left,right,top,bottom;
    PitchMaterial material={0};
    if(!s->entity || !s->animation || s->animation>=SSA_ACTOR_RESOURCE_COUNT || s->frame>=65534 ||
       (s->flags&128) || ((s->flags&64) && (g->frame_game.flags&1)))return;
    body=&s->entity->body;resource=&g->actor_assets.resources[s->animation];
    if(!resource->directions || !resource->mip_flags)return;
    facing=(uint16_t)((unsigned)s->facing*256+((s->flags&16)?32768:0));
    dir=((unsigned)(uint16_t)(facing-ssa_direction(&g->math,(int16_t)(body->x/65536-(int)c->x),
        (int16_t)(body->y/65536-(int)c->y)))+0x800)>>12;dir&=15;
    if(dir>8){dir=16-dir;flip=1;}
    dirs=resource->directions-1u;if(!dirs){dir=0;flip=0;}else while(dirs<=7){dirs*=2;dir/=2;}
    image=(unsigned)s->frame+1+(unsigned)dir*resource->frames;
    {double depth=(body->x/65536.0-c->x)*c->sine+(body->y/65536.0-c->y)*c->cosine;
        bit=actor_mip(g,resource,c->focal/fmax(.05,depth),&image);}
    im=ssa_actor_assets_image(&g->actor_assets,s->animation,image);if(!im)return;
    unit=actor_unit(bit);
    if(body->step_adjust){cut=body->floor+body->step_adjust-body->z/65536;if(cut<0)cut=0;}
    if(cut>=(int)im->height)return;
    left=(flip?-(im->origin_x+(int)im->width):im->origin_x)*unit;right=left+im->width*unit;
    top=body->z/65536.0-im->origin_y*unit;bottom=top-(im->height-cut)*unit;
    material.pixels=im->pixels;material.width=im->width;material.height=im->height;material.transparent=material.actor=1;
    actor_silhouette(r,g,c,body,left,right,top,bottom,flip,unit,&material);
}

/* The shadow is an ordinary transparent billboard in its subject's painter
 * layer. It has no floor triangles or depth offset and is always drawn first. */
static void pitched_shadow(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c,const SsaActorSource *s,unsigned source)
{
    const SsaSpriteShadow *v;SsaBody ground;PitchMaterial material={0};double radius;
    if(!c->shadows || source>=SSA_SHADOW_LIMIT || c->shadows->indices[source]<0 || !s->entity ||
       (s->flags&128) || ((s->flags&64) && (g->frame_game.flags&1)))return;
    v=&c->shadows->sprites[c->shadows->indices[source]];radius=v->radius;
    ground=s->entity->body;ground.z=v->floor*65536;ground.height=0;
    material.pixels=v->pixels;material.width=SSA_SHADOW_WIDTH;material.height=SSA_SHADOW_HEIGHT;
    material.transparent=material.actor=1;
    actor_silhouette(r,g,c,&ground,-radius,radius,v->floor+radius*.25,v->floor-radius*.25,0,
        radius*2/SSA_SHADOW_WIDTH,&material);
}
