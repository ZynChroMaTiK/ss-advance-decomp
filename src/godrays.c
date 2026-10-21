#include "ssa/godrays.h"
#include <limits.h>
#include <string.h>

typedef struct Vertex {int64_t x,y,z,v,u;} Vertex;
typedef struct Projected {int64_t x,y,q,vq,uq;} Projected;
static unsigned rd(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int sd(const uint8_t *p){unsigned n=rd(p);return n<32768?(int)n:(int)n-65536;}
static int64_t min64(int64_t a,int64_t b){return a<b?a:b;}
static int64_t max64(int64_t a,int64_t b){return a>b?a:b;}
static int64_t ratio16(int64_t n,int64_t d){return (n/d)*65536+(n%d)*65536/d;}
int ssa_godray_palette(SsaGodrayPalette *p,const uint16_t colors[256]){
    unsigned i,j,s,k,light[3]={0};
    if(!p || !colors)return -1;
    if(p->loaded && !memcmp(p->colors,colors,sizeof p->colors))return 1;
    memcpy(p->colors,colors,sizeof p->colors);
    for(i=0;i<256;++i)for(k=0;k<3;++k){unsigned c=(colors[i]>>(5*k))&31;if(c>light[k])light[k]=c;}
    light[2]=light[2]*7/8;
    for(i=0;i<256;++i){p->blend[0][i]=(uint8_t)i;
        for(s=1;s<=SSA_GODRAY_STEPS;++s){unsigned target[3],best=i,error=UINT_MAX;
            for(k=0;k<3;++k){unsigned b=(colors[i]>>(5*k))&31;
                target[k]=b+(light[k]>b?((light[k]-b)*42*s+800)/1600:0);}
            for(j=0;j<256;++j){unsigned e=0;for(k=0;k<3;++k){int d=(int)((colors[j]>>(5*k))&31)-(int)target[k];e+=(unsigned)(d*d);}
                if(e<error || (e==error && j==i)){best=j;error=e;}}
            p->blend[s][i]=(uint8_t)best;
        }}
    p->loaded=1;return 1;
}
unsigned ssa_godray_sample(int64_t q,int64_t vq,uint32_t *depth){
    int64_t level;
    if(q<=0){if(depth)*depth=UINT32_MAX;return 0;}
    if(depth)*depth=(uint32_t)(UINT64_C(4294967296)/(uint64_t)q);
    level=(vq*SSA_GODRAY_STEPS+q/2)/q;
    return (unsigned)max64(0,min64(SSA_GODRAY_STEPS,level));
}
static int64_t floor_ratio(int64_t n,int64_t d){int64_t a=n/d;return a-(n%d<0);}
unsigned ssa_godray_dither(int64_t q,int64_t vq,int64_t uq,int bottom,int top){
    /* Ordered Bayer texture: a true checkerboard at half coverage, with
     * finer density steps over each8x8 tile. World texels are2 units. */
    static const uint8_t ranks[64]={
        0,48,12,60,3,51,15,63, 32,16,44,28,35,19,47,31,
        8,56,4,52,11,59,7,55, 40,24,36,20,43,27,39,23,
        2,50,14,62,1,49,13,61, 34,18,46,30,33,17,45,29,
        10,58,6,54,9,57,5,53, 42,26,38,22,41,25,37,21};
    /* Pair gradient rows: each checkerboard transition spans12 world units
     * on a full-height shaft, instead of6. The64-rank texture tiles in both
     * axes at two world units/texel; its mean retains the linear gradient. */
    enum { SHADE_STRIDE=2 };
    int64_t row,height,gradient,scaled,remainder;unsigned level,x,y,threshold;
    if(q<=0 || vq<=0 || top<=bottom)return 0;
    if(vq>=q)return SSA_GODRAY_STEPS;
    height=(int64_t)top-bottom;
    row=floor_ratio((int64_t)bottom*q+height*vq,2*q);
    /* Sample both coverage and checkerboard at the same world texel. Using
     * continuous vq for coverage cut thin color strips through texel rows. */
    gradient=max64(0,min64(height,row*2+1-bottom));
    scaled=gradient*(SSA_GODRAY_STEPS/SHADE_STRIDE);
    level=(unsigned)(scaled/height)*SHADE_STRIDE;remainder=scaled%height;
    x=(unsigned)floor_ratio(uq,q)&7;y=(unsigned)row&7;
    threshold=ranks[y*8+x];
    return level+SHADE_STRIDE*(remainder*128>height*(2*threshold+1));
}
static Vertex vertex(const SsaGodrayCamera *c,int64_t x,int64_t y,int z,int v,int64_t u){
    int64_t dx=x-c->x,dy=y-c->y,h=(int64_t)z*256-c->z;
    int64_t d=(dx*c->sine+dy*c->cosine)/65536;
    Vertex p={(dx*c->cosine-dy*c->sine)/65536,
        (h*c->pitch_cosine-d*c->pitch_sine)/65536,
        (d*c->pitch_cosine+h*c->pitch_sine)/65536,v,u};return p;
}
static int64_t distance(const SsaGodrayCamera *c,Vertex p,unsigned plane){
    switch(plane){
    case 0:return p.z-16;
    case 1:return (p.z*((int64_t)c->width*128)+p.x*c->focal_x)/256;
    case 2:return (p.z*((int64_t)c->width*128)-p.x*c->focal_x)/256;
    case 3:return (p.z*((int64_t)c->height*128)+p.y*c->focal_y)/256;
    default:return (p.z*((int64_t)c->height*128)-p.y*c->focal_y)/256;
    }
}
static int64_t orient(Projected a,Projected b,int64_t x,int64_t y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static int triangle(const SsaGodrayCamera *c,const SsaGodrayOps *ops,Vertex a,Vertex b,Vertex d,unsigned line,int bottom,int top){
    Vertex p[3]={a,b,d};Projected t[3];unsigned i;int y,first,last;int64_t area;
    for(i=0;i<3;++i){t[i].x=(int64_t)c->width*8+p[i].x*c->focal_x/(p[i].z*16);
        t[i].y=(int64_t)c->height*8-p[i].y*c->focal_y/(p[i].z*16);
        t[i].q=INT64_C(4294967296)/p[i].z;t[i].vq=t[i].q*p[i].v/65536;t[i].uq=t[i].q*p[i].u/65536;}
    area=orient(t[0],t[1],t[2].x,t[2].y);if(!area)return 0;
    if(area<0){Projected temp=t[1];t[1]=t[2];t[2]=temp;area=-area;}
    first=(int)max64(0,(min64(t[0].y,min64(t[1].y,t[2].y))+7)/16);
    last=(int)min64(c->height,(max64(t[0].y,max64(t[1].y,t[2].y))+7)/16);
    for(y=first;y<last;++y){int64_t py=(int64_t)y*16+8,lo=INT64_MAX,hi=INT64_MIN;int left,right;
        for(i=0;i<3;++i){Projected a0=t[i],b0=t[(i+1)%3];
            if(a0.y>b0.y){Projected swap=a0;a0=b0;b0=swap;}
            if((a0.y<=py && py<b0.y) || (b0.y<=py && py<a0.y)){
                int64_t x=a0.x+(b0.x-a0.x)*(py-a0.y)/(b0.y-a0.y);
                lo=min64(lo,x);hi=max64(hi,x);}}
        if(lo>hi)continue;
        left=(int)max64(0,(lo+7)/16);right=(int)min64(c->width,(hi+7)/16);
        if(left<right){int64_t px=(int64_t)left*16+8,wa=orient(t[1],t[2],px,py),wb=orient(t[2],t[0],px,py);
            int64_t da=-(t[2].y-t[1].y)*16,db=-(t[0].y-t[2].y)*16;
            SsaGodraySpan span={(unsigned)left,(unsigned)y,(unsigned)(right-left),line,bottom,top,0,0,0,0,0,0};
            span.q=t[2].q+(ratio16(wa,area)*(t[0].q-t[2].q)+ratio16(wb,area)*(t[1].q-t[2].q))/65536;
            span.vq=t[2].vq+(ratio16(wa,area)*(t[0].vq-t[2].vq)+ratio16(wb,area)*(t[1].vq-t[2].vq))/65536;
            span.uq=t[2].uq+(ratio16(wa,area)*(t[0].uq-t[2].uq)+ratio16(wb,area)*(t[1].uq-t[2].uq))/65536;
            /* Extra16 fractional bits keep long, shallow spans stable. */
            span.q*=65536;span.vq*=65536;span.uq*=65536;
            span.dq=ratio16(da*(t[0].q-t[2].q)+db*(t[1].q-t[2].q),area);
            span.dvq=ratio16(da*(t[0].vq-t[2].vq)+db*(t[1].vq-t[2].vq),area);
            span.duq=ratio16(da*(t[0].uq-t[2].uq)+db*(t[1].uq-t[2].uq),area);
            ops->span(ops->context,&span);}}
    return 1;
}
int ssa_godrays_draw(const SsaMapAssets *m,const SsaGodrayCamera *c,const SsaGodrayOps *ops){
    unsigned line;int triangles=0;
    if(!m || !m->data || !c || !ops || !ops->span || !c->width || !c->height ||
       c->width>16384 || c->height>16384 || c->focal_x<=0 || c->focal_y<=0 ||
       c->focal_x>16777216 || c->focal_y>16777216)return -1;
    if(m->lines>m->size || m->line_count>(m->size-m->lines)/40 ||
       m->vertices>m->size || m->vertex_count>(m->size-m->vertices)/4 ||
       m->sectors>m->size || m->sector_count>(m->size-m->sectors)/20)return -1;
    for(line=0;line<m->line_count;++line){const uint8_t *l=m->data+m->lines+line*40,*s,*roof,*a,*b;
        unsigned front=rd(l+12),back=rd(l+14),i,plane,n=4;int bottom,top,draw_bottom,vbottom;int64_t ax,ay,bx,by,dx,dy,scale,ua,ub;
        Vertex polygon[12],out[12];
        if(!(rd(l+26)&4) || front==back || front>=m->sector_count || back>=m->sector_count)continue;
        s=m->data+m->sectors+front*20;roof=m->data+m->sectors+back*20;
        if(s[11] && !roof[11]){const uint8_t *swap=s;s=roof;roof=swap;}
        else if(s[11] || !roof[11])continue;
        bottom=sd(s);top=sd(roof+2);
        if(top<=bottom+SSA_GODRAY_GAP || sd(s+2)<=top || sd(roof)>=top)continue;
        bottom=(int)max64(bottom+SSA_GODRAY_GAP,top-SSA_GODRAY_HEIGHT);
        draw_bottom=c->clip?(int)max64(bottom,c->clip_z):bottom;
        if(draw_bottom>=top)continue;
        vbottom=(int)((int64_t)(draw_bottom-bottom)*65536/(top-bottom));
        if(rd(l)>=m->vertex_count || rd(l+2)>=m->vertex_count)return -1;
        a=m->data+m->vertices+rd(l)*4;b=m->data+m->vertices+rd(l+2)*4;
        ax=sd(a)*256;ay=sd(a+2)*256;bx=sd(b)*256;by=sd(b+2)*256;
        /* Canonical diagonal also keeps fixed-point interpolation identical
         * when the map describes this boundary in the opposite direction. */
        if(ax>bx || (ax==bx && ay>by)){int64_t swap=ax;ax=bx;bx=swap;swap=ay;ay=by;by=swap;}
        dx=bx-ax;dy=by-ay;
        scale=max64(dx<0?-dx:dx,dy<0?-dy:dy);if(scale<8*256)continue;
        /* Use the actual shared vertices. Per-edge normal offsets separate
         * corners or overlap them, producing dark cracks or double-lit lines. */
        /* Dominant world axis keeps texture density stable on either wall
         * orientation and phase consistent across collinear edge splits. */
        if((dx<0?-dx:dx)>=(dy<0?-dy:dy)){ua=ax*128;ub=bx*128;}else{ua=ay*128;ub=by*128;}
        polygon[0]=vertex(c,ax,ay,draw_bottom,vbottom,ua);polygon[1]=vertex(c,bx,by,draw_bottom,vbottom,ub);
        polygon[2]=vertex(c,bx,by,top,65536,ub);polygon[3]=vertex(c,ax,ay,top,65536,ua);
        for(plane=0;plane<5 && n>=3;++plane){unsigned count=0;
            for(i=0;i<n;++i){Vertex a0=polygon[i],b0=polygon[(i+1)%n];int64_t da=distance(c,a0,plane),db=distance(c,b0,plane);
                if(da>=0)out[count++]=a0;
                if((da>=0)!=(db>=0)){int64_t f=da*65536/(da-db);Vertex v={
                    a0.x+(b0.x-a0.x)*f/65536,a0.y+(b0.y-a0.y)*f/65536,
                    a0.z+(b0.z-a0.z)*f/65536,a0.v+(b0.v-a0.v)*f/65536,a0.u+(b0.u-a0.u)*f/65536};
                    if(!plane)v.z=16;
                    out[count++]=v;}}
            n=count;memcpy(polygon,out,n*sizeof(*out));}
        for(i=1;i+1<n;++i)triangles+=triangle(c,ops,polygon[0],polygon[i],polygon[i+1],line,bottom,top);
    }
    return triangles;
}
