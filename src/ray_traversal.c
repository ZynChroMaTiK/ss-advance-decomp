#include "ssa/ray_traversal.h"
#include <limits.h>

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
static int valid(const SsaRayTraversal *s) {
    return s && !s->updating && s->count<=s->capacity &&
           (!s->capacity || s->intercepts);
}
static int traverse(SsaRayTraversal *s,int32_t maximum,SsaRayPredicate predicate,void *context) {
    size_t i,j;
    for (i=0;i<s->count;++i) {
        size_t best=s->count;
        int32_t fraction=INT32_MAX;
        int result;
        for (j=0;j<s->count;++j) {
            if (s->intercepts[j].fraction<fraction) {
                best=j; fraction=s->intercepts[j].fraction;
            }
        }
        if (fraction>maximum) return 1;
        /* Original dereferences a stale/null best pointer when every entry is
         * INT32_MAX and maximum is INT32_MAX. Reject that undefined input. */
        if (best==s->count) return -1;
        result=predicate(context,&s->intercepts[best]);
        if (result<0) return -1;
        if (!result) { s->blocking_fraction=fraction; return 0; }
        s->intercepts[best].fraction=INT32_MAX;
    }
    return 1;
}
int ssa_ray_traverse_intercepts(SsaRayTraversal *s,int32_t maximum,
                               SsaRayPredicate predicate,void *context) {
    int result;
    if (!valid(s) || !predicate) return -1;
    s->updating=1;
    result=traverse(s,maximum,predicate,context);
    s->updating=0;
    return result;
}
int ssa_ray_trace(SsaRayTraversal *s,int32_t ox,int32_t oy,int32_t x,int32_t y,
                  int32_t ex,int32_t ey,SsaRayCollectCell collect,
                  SsaRayPredicate predicate,void *context) {
    int32_t rx,ry,rex,rey,bx,by,tx,ty,xstep,ystep,partial,yslope,xslope,xi,yi;
    unsigned i;
    int result=1;
    if (!valid(s) || !collect || !predicate) return -1;
    s->updating=1; s->count=0;
    if (!((uint32_t)sub(x,ox)&0x7fffff)) x=add(x,65536);
    if (!((uint32_t)sub(y,oy)&0x7fffff)) y=add(y,65536);
    s->ray.x=x; s->ray.y=y; s->ray.dx=sub(ex,x); s->ray.dy=sub(ey,y);
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
        result=collect(context,s,bx,by);
        if (result<=0) { if (result<0) result=-1; goto done; }
        if (s->count>s->capacity) { result=-1; goto done; }
        if (bx==tx && by==ty) break;
        if (asr(yi,16)==by) { yi=add(yi,yslope); bx=add(bx,xstep); }
        else if (asr(xi,16)==bx) { xi=add(xi,xslope); by=add(by,ystep); }
    }
    result=traverse(s,65536,predicate,context);
done:
    s->updating=0;
    return result;
}
