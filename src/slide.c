#include "ssa/slide.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int32_t bits(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)~v; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t asr(int32_t a,unsigned n) {
    return bits(((uint32_t)a>>n)|(a<0 ? UINT32_MAX<<(32-n) : 0));
}
static int32_t fx(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;
    if (p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static int16_t s16(const uint8_t *p) {
    uint16_t u=u16(p);
    return u<=INT16_MAX ? (int16_t)u : (int16_t)(-1-(int32_t)(UINT16_MAX-u));
}
static const uint8_t *record(const SsaMapAssets *m,uint32_t off,size_t n,size_t stride) {
    if (!m || !m->data || off>m->size || stride>m->size-off || n>(m->size-off-stride)/stride) return NULL;
    return m->data+off+n*stride;
}

/* 0800970C + original 32-stage unsigned ARM division at ROM 08000430.
 * Keeping the stages also preserves divide-by-zero and overflowing quotients. */
int32_t ssa_trace_divide(int32_t a,int32_t b) {
    uint32_t aa=a<0 ? 0u-(uint32_t)a : (uint32_t)a;
    uint32_t bb=b<0 ? 0u-(uint32_t)b : (uint32_t)b;
    uint32_t low=aa<<16,high=aa>>16;
    unsigned i;
    for (i=0;i<32;++i) {
        high=(high<<1)|(low>>31);
        low<<=1;
        if (high>=bb) { high-=bb; low|=1; }
    }
    return bits((a<0)!=(b<0) ? 0u-low : low);
}
int ssa_trace_side(int32_t x,int32_t y,const SsaTraceVector *l) {
    int32_t px,py;
    uint32_t sign;
    if (!l->dx) return x<=l->x ? l->dy>0 : l->dy<0;
    if (!l->dy) return y<=l->y ? l->dx<0 : l->dx>0;
    px=sub(x,l->x); py=sub(y,l->y);
    sign=(uint32_t)l->dy^(uint32_t)px;
    if (((uint32_t)l->dx^(uint32_t)l->dy^(uint32_t)px^(uint32_t)py)>>31) return (int)(sign>>31);
    return fx(asr(py,8),asr(l->dx,8))>=fx(asr(l->dy,8),asr(px,8));
}
int32_t ssa_trace_intercept(const SsaTraceVector *t,const SsaTraceVector *w) {
    int32_t den=sub(fx(asr(w->dy,8),t->dx),fx(asr(w->dx,8),t->dy));
    int32_t num;
    if (!den) return 0;
    num=add(fx(asr(sub(w->x,t->x),8),w->dy),fx(asr(sub(t->y,w->y),8),w->dx));
    return ssa_trace_divide(num,den);
}
int ssa_slide_init(SsaSlide *s,const SsaMapAssets *m) {
    if (!s || !m || !m->data) return 0;
    memset(s,0,sizeof(*s));
    s->visited=calloc(m->line_count ? m->line_count : 1,1);
    s->intercepts=malloc((m->line_count ? m->line_count : 1)*sizeof(*s->intercepts));
    if (!s->visited || !s->intercepts) { ssa_slide_free(s); return 0; }
    s->map=m; s->capacity=m->line_count;
    return 1;
}
void ssa_slide_free(SsaSlide *s) {
    if (!s) return;
    free(s->visited); free(s->intercepts); memset(s,0,sizeof(*s));
}
/* 080098CC uses the SECOND endpoint, unlike 08003B0C's global first endpoint. */
static int wall_side(const SsaMapAssets *m,const uint8_t *l,int32_t x,int32_t y) {
    const uint8_t *v;
    if (u16(l+2)>=m->vertex_count || !(v=record(m,m->vertices,u16(l+2),4))) return -1;
    return ssa_line_side(x,y,s16(v),s16(v+2),s16(l+4),s16(l+6));
}
static int collect(SsaSlide *s,const SsaTraceVector *t,uint16_t index) {
    const SsaMapAssets *m=s->map;
    const uint8_t *l,*v1,*v2;
    int a,b;
    int32_t f;
    SsaTraceVector wall;
    if (index>=m->line_count || !(l=record(m,m->lines,index,40))) return -1;
    if (s->visited[index]) return 1;
    s->visited[index]=1;
    if (u16(l)>=m->vertex_count || u16(l+2)>=m->vertex_count ||
        !(v1=record(m,m->vertices,u16(l),4)) || !(v2=record(m,m->vertices,u16(l+2),4))) return -1;
    if (t->dx>0x100000 || t->dy>0x100000 || t->dx<-0x100000 || t->dy<-0x100000) {
        a=ssa_trace_side((int32_t)s16(v1)*65536,(int32_t)s16(v1+2)*65536,t);
        b=ssa_trace_side((int32_t)s16(v2)*65536,(int32_t)s16(v2+2)*65536,t);
    } else {
        a=wall_side(m,l,t->x,t->y);
        b=wall_side(m,l,add(t->x,t->dx),add(t->y,t->dy));
    }
    if (a==b) return 1;
    wall.x=(int32_t)s16(v1)*65536; wall.y=(int32_t)s16(v1+2)*65536;
    wall.dx=(int32_t)s16(l+4)*65536; wall.dy=(int32_t)s16(l+6)*65536;
    f=ssa_trace_intercept(t,&wall);
    if ((uint32_t)f<=65536) {
        if (s->count>=s->capacity) return -1;
        s->intercepts[s->count].fraction=f; s->intercepts[s->count++].line=index;
    }
    return 1;
}
static int cell(SsaSlide *s,const SsaTraceVector *t,const uint8_t *blocks,size_t words,int width,int height,int x,int y) {
    size_t cursor;
    if (x<0 || y<0 || x>=width || y>=height) return 1;
    cursor=(size_t)u16(blocks+2*(4+(size_t)y*(unsigned)width+(unsigned)x))+1;
    for (;;) {
        uint16_t line;
        if (cursor>=words) return -1;
        line=u16(blocks+2*cursor++);
        if (line==UINT16_MAX) return 1;
        if (collect(s,t,line)!=1) return -1;
    }
}
int ssa_slide_trace(SsaSlide *s,int32_t x,int32_t y,int32_t ex,int32_t ey) {
    const SsaMapAssets *m;
    const uint8_t *blocks;
    size_t words;
    int width,height,bx,by,tx,ty,xstep,ystep,i;
    int32_t ox,oy,rx,ry,rex,rey,partial,yslope,xslope,yi,xi;
    SsaTraceVector t;
    if (!s || !(m=s->map) || !s->visited || !s->intercepts || m->line_count>s->capacity || m->size<0x48 ||
        !(blocks=record(m,m->blockmap,0,8))) return -1;
    words=u16(m->data+0x1a);
    if (words<4 || words>(m->size-m->blockmap)/2) return -1;
    width=s16(blocks+4); height=s16(blocks+6);
    if (width<=0 || height<=0 || (size_t)width*(unsigned)height>words-4) return -1;
    s->count=0; memset(s->visited,0,m->line_count);
    ox=(int32_t)s16(blocks)*65536; oy=(int32_t)s16(blocks+2)*65536;
    if (!((uint32_t)sub(x,ox)&0x7fffff)) x=add(x,65536);
    if (!((uint32_t)sub(y,oy)&0x7fffff)) y=add(y,65536);
    t.x=x; t.y=y; t.dx=sub(ex,x); t.dy=sub(ey,y);
    rx=sub(x,ox); ry=sub(y,oy); rex=sub(ex,ox); rey=sub(ey,oy);
    bx=asr(rx,23); by=asr(ry,23); tx=asr(rex,23); ty=asr(rey,23);
    if (tx==bx) { xstep=0; partial=65536; yslope=0x1000000; }
    else {
        int32_t dx=sub(rex,rx);
        xstep=tx>bx ? 1 : -1;
        partial=asr(rx,7)&65535;
        if (xstep>0) partial=65536-partial;
        yslope=ssa_trace_divide(sub(rey,ry),dx<0 ? sub(0,dx) : dx);
    }
    yi=add(asr(ry,7),fx(partial,yslope));
    if (ty==by) { ystep=0; partial=65536; xslope=0x1000000; }
    else {
        int32_t dy=sub(rey,ry);
        ystep=ty>by ? 1 : -1;
        partial=asr(ry,7)&65535;
        if (ystep>0) partial=65536-partial;
        xslope=ssa_trace_divide(sub(rex,rx),dy<0 ? sub(0,dy) : dy);
    }
    xi=add(asr(rx,7),fx(partial,xslope));
    for (i=0;i<64;++i) {
        if (cell(s,&t,blocks,words,width,height,bx,by)!=1) return -1;
        if (bx==tx && by==ty) break;
        if (asr(yi,16)==by) { yi=add(yi,yslope); bx+=xstep; }
        else if (asr(xi,16)==bx) { xi=add(xi,xslope); by+=ystep; }
    }
    return 1;
}

int ssa_slide_project(const SsaMapAssets *m,uint16_t index,int32_t x,int32_t y,
                      const SsaMathTables *tables,int32_t *vx,int32_t *vy) {
    const uint8_t *l;
    int side;
    int32_t tangent,angle,delta,sine,cosine,length,amount,ax,ay,den;
    int16_t ix,iy;
    if (!m || index>=m->line_count || !tables || !vx || !vy || !(l=record(m,m->lines,index,40))) return -1;
    if (!u16(l+24)) { *vy=0; return 1; }
    if (u16(l+24)==1) { *vx=0; return 1; }
    side=wall_side(m,l,x,y);
    if (side<0) return -1;
    tangent=(65536-u16(l+28)+16384)&65535;
    if (side==1) tangent+=32768;
    ix=(int16_t)asr(*vx,16); iy=(int16_t)asr(*vy,16);
    angle=ssa_direction(tables,ix,iy);
    ax=ix<0 ? -(int32_t)ix : ix; ay=iy<0 ? -(int32_t)iy : iy;
    length=0;
    if (ax || ay) {
        ssa_sincos(tables,(uint16_t)angle,&sine,&cosine);
        den=ax>ay ? sine : cosine;
        if (den<0) den=-den;
        /* BIOS signed division sees a wrapped 32-bit shifted numerator. */
        length=den ? bits((uint32_t)(ax>ay ? ax : ay)<<16)/den : 0;
    }
    angle=(65536-angle+16384)&65535;
    length=bits((uint32_t)length<<16);
    delta=angle-tangent;
    if (delta>32768) delta+=32768;
    ssa_sincos(tables,(uint16_t)(65536-(delta-16384)),&sine,&cosine);
    amount=fx(length,sine);
    ssa_sincos(tables,(uint16_t)(65536-(tangent-16384)),&sine,&cosine);
    *vx=fx(amount,sine); *vy=fx(amount,cosine);
    return 1;
}
static int blocking(const SsaMapAssets *m,const SsaBody *b,uint16_t index) {
    const uint8_t *l=record(m,m->lines,index,40),*front,*back;
    int f,k;
    int32_t floor,ceiling;
    if (!l) return -1;
    if (!(u16(l+26)&4)) {
        int side=wall_side(m,l,b->x,b->y);
        return side<0 ? -1 : side==0;
    }
    f=s16(l+12); k=s16(l+14);
    if (f<0 || k<0 || f>=m->sector_count || k>=m->sector_count ||
        !(front=record(m,m->sectors,(size_t)f,20)) || !(back=record(m,m->sectors,(size_t)k,20))) return -1;
    floor=(int32_t)(s16(front)>s16(back) ? s16(front) : s16(back))*65536;
    ceiling=(int32_t)(s16(front+2)<s16(back+2) ? s16(front+2) : s16(back+2))*65536;
    return sub(ceiling,floor)<b->height || sub(ceiling,b->z)<b->height || sub(floor,b->z)>48*65536;
}
static int nearest(SsaSlide *s,const SsaBody *b,int32_t *fraction,uint16_t *line) {
    size_t i,j;
    for (i=0;i<s->count;++i) {
        size_t best=s->count;
        int result;
        for (j=0;j<s->count;++j) if (s->intercepts[j].fraction!=INT32_MAX &&
            (best==s->count || s->intercepts[j].fraction<s->intercepts[best].fraction)) best=j;
        if (best==s->count) break;
        result=blocking(s->map,b,s->intercepts[best].line);
        if (result<0) return -1;
        if (result) {
            if (s->intercepts[best].fraction<*fraction) {
                *fraction=s->intercepts[best].fraction; *line=s->intercepts[best].line;
            }
            break;
        }
        s->intercepts[best].fraction=INT32_MAX;
    }
    return 1;
}
static int fallback(SsaBody *b,SsaSlideTryMove move,void *context) {
    int result=move(context,b,b->x,add(b->y,b->velocity_y));
    return result!=0 ? result : move(context,b,add(b->x,b->velocity_x),b->y);
}
int ssa_slide_move(SsaSlide *s,SsaBody *b,int32_t radius,const SsaMathTables *tables,SsaSlideTryMove move,void *context) {
    unsigned retry;
    if (!s || !b || !tables || !move || radius<0) return -1;
    for (retry=0;retry<2;++retry) {
        int32_t leadx=b->velocity_x>0 ? add(b->x,radius) : sub(b->x,radius);
        int32_t trailx=b->velocity_x>0 ? sub(b->x,radius) : add(b->x,radius);
        int32_t leady=b->velocity_y>0 ? add(b->y,radius) : sub(b->y,radius);
        int32_t traily=b->velocity_y>0 ? sub(b->y,radius) : add(b->y,radius);
        int32_t fraction=65537,vx,vy;
        uint16_t line=0;
        int result;
        int32_t corners[3][2]={{leadx,leady},{trailx,leady},{leadx,traily}};
        unsigned i;
        for (i=0;i<3;++i) {
            if (ssa_slide_trace(s,corners[i][0],corners[i][1],add(corners[i][0],b->velocity_x),add(corners[i][1],b->velocity_y))!=1 ||
                nearest(s,b,&fraction,&line)!=1) return -1;
        }
        if (fraction==65537) return fallback(b,move,context);
        fraction-=2048;
        if (fraction>0) {
            result=move(context,b,add(b->x,fx(b->velocity_x,fraction)),add(b->y,fx(b->velocity_y,fraction)));
            if (result<0) return -1;
            if (!result) return fallback(b,move,context);
        }
        fraction=0xf800-fraction;
        if (fraction>65536) fraction=65536;
        if (fraction<=0) return 0;
        vx=fx(b->velocity_x,fraction); vy=fx(b->velocity_y,fraction);
        if (ssa_slide_project(s->map,line,b->x,b->y,tables,&vx,&vy)!=1) return -1;
        b->velocity_x=vx; b->velocity_y=vy;
        result=move(context,b,add(b->x,vx),add(b->y,vy));
        if (result!=0) return result;
    }
    return fallback(b,move,context);
}
