#include "ssa/automap.h"
#include <string.h>
static int32_t bits(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static int32_t sub(int32_t a,int32_t b){return bits((uint32_t)a-(uint32_t)b);}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t asr(int32_t a,unsigned n){return bits(((uint32_t)a>>n)|(a<0?UINT32_MAX<<(32-n):0));}
static int32_t mul(int32_t a,int32_t b){return bits((uint32_t)((uint64_t)((int64_t)a*b)>>16));}
static unsigned u16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
static int range(const SsaMapAssets *m,uint32_t at,size_t count,size_t stride){return at<=m->size && count<=(m->size-at)/stride;}
static unsigned outcode(int32_t x,int32_t y){return (x<0?1u:x>239?2u:0u)|(y<0?4u:y>159?8u:0u);}
static int32_t divide_product(int32_t a,int32_t b,int32_t c){return bits((uint32_t)a*(uint32_t)b)/c;}
int ssa_automap_line(uint8_t *pixels,size_t size,int32_t x,int32_t y,int32_t end_x,int32_t end_y,unsigned color)
{
    int32_t dx,dy,t,error,ax,ay;unsigned code,attempts=0;int direction;
    if(!pixels || size<38400 || color>255 || x< -65536 || x>65536 || y< -65536 || y>65536 ||
       end_x< -65536 || end_x>65536 || end_y< -65536 || end_y>65536)return -1;
    if(x>end_x){t=x;x=end_x;end_x=t;t=y;y=end_y;end_y=t;}
    dx=end_x-x;dy=end_y-y;
    if(end_x<0 || x>239 || (dy>=0?(end_y<0 || y>159):(y<0 || end_y>159)))return 1;
    if(!dy){if(x<0)x=0;if(end_x>239)end_x=239;}
    else if(!dx){if(dy<0){t=y;y=end_y;end_y=t;}if(y<0)y=0;if(end_y>159)end_y=159;}
    else{
        while((code=outcode(x,y)|(outcode(end_x,end_y)<<4))!=0){
            if((code&15)&(code>>4))return 1;
            if(++attempts>16)return -1;
            dx=end_x-x;dy=end_y-y;t=dy<0?-dy:dy;
            if(code&1){y=add(y,divide_product(-x,dy,dx));x=0;}
            else if(code&32){end_y=sub(end_y,divide_product(end_x-239,dy,dx));end_x=239;}
            else if(code&4){x=add(x,divide_product(-y,dx,t));y=0;}
            else if(code&8){x=add(x,divide_product(y-159,dx,t));y=159;}
            else if(code&64){end_x=sub(end_x,divide_product(-end_y,dx,t));end_y=0;}
            else if(code&128){end_x=sub(end_x,divide_product(end_y-159,dx,t));end_y=159;}
        }
    }
    dx=end_x-x;dy=end_y-y;
    if(x<0 || x>239 || y<0 || y>159 || end_x<0 || end_x>239 || end_y<0 || end_y>159)return -1;
    if(!dx && !dy){pixels[(size_t)y*240+(unsigned)(x^1)]|=(uint8_t)color;return 1;}
    if(!dy){for(;x<=end_x;++x)pixels[(size_t)y*240+(unsigned)x]|=(uint8_t)color;return 1;}
    if(!dx){if(dy<0){y=end_y;dy=-dy;}for(t=0;t<=dy;++t)pixels[(size_t)(y+t)*240+(unsigned)x]|=(uint8_t)color;return 1;}
    ax=2*dx;ay=2*(dy<0?-dy:dy);
    if(ax>ay){
        error=ay-dx;direction=dy>0?1:-1;
        for(t=0;t<dx;++t){pixels[(size_t)y*240+(unsigned)x]|=(uint8_t)color;
            if(error>=0){y+=direction;error-=ax;}++x;error+=ay;}
    }else{
        error=ax-ay/2;
        if(dy<0){y=end_y;x=end_x;dy=-dy;dx=-dx;}
        direction=dx>0?1:-1;
        for(t=0;t<dy;++t){pixels[(size_t)y*240+(unsigned)x]|=(uint8_t)color;
            if(error>=0){x+=direction;error-=ay;}++y;error+=ax;}
    }
    return 1;
}
static int project(SsaAutomapLine emit,void *context,const SsaAutomapView *v,
    int32_t x,int32_t y,int32_t ex,int32_t ey,unsigned color)
{
    return emit(context,120+asr(mul(sub(x,v->x),(int32_t)v->zoom),16),80-asr(mul(sub(y,v->y),(int32_t)v->zoom),16),
        120+asr(mul(sub(ex,v->x),(int32_t)v->zoom),16),80-asr(mul(sub(ey,v->y),(int32_t)v->zoom),16),color);
}
static int wall(const SsaMapAssets *m,const SsaAutomapView *v,const uint8_t *line,unsigned color,SsaAutomapLine emit,void *ctx)
{
    const uint8_t *a=m->data+m->vertices+4*u16(line),*b=m->data+m->vertices+4*u16(line+2);
    return project(emit,ctx,v,bits(u16(a)<<16),bits(u16(a+2)<<16),bits(u16(b)<<16),bits(u16(b+2)<<16),color);
}
int ssa_automap_scene(const SsaMapAssets *m,const SsaMathTables *math,const SsaAutomapView *v,SsaAutomapLine emit,void *ctx)
{
    size_t i;int32_t sine,cosine,ex,ey,x[3],y[3];
    if(!m || !m->data || !math || !v || !emit || v->zoom<4096 || v->zoom>65536 ||
       v->entity_count>100 || (v->entity_count && !v->entities) || v->contact_count>m->line_count ||
       (v->contact_count && !v->contacts) || !range(m,m->lines,m->line_count,40) ||
       !range(m,m->vertices,m->vertex_count,4) || !range(m,m->sectors,m->sector_count,20))return -1;
    for(i=0;i<m->line_count;++i){const uint8_t *l=m->data+m->lines+i*40;
        if(u16(l)>=m->vertex_count || u16(l+2)>=m->vertex_count ||
           ((u16(l+26)&4) && (u16(l+12)>=m->sector_count || u16(l+14)>=m->sector_count)))return -1;}
    for(i=0;i<v->contact_count;++i)if(v->contacts[i]>=m->line_count)return -1;
    for(i=0;i<m->line_count;++i){const uint8_t *l=m->data+m->lines+i*40;
        if((u16(l+26)&4) && u16(m->data+m->sectors+20*u16(l+12))==u16(m->data+m->sectors+20*u16(l+14)))continue;
        if(wall(m,v,l,3,emit,ctx)<0)return -1;}
    for(i=0;i<v->contact_count;++i)if(wall(m,v,m->data+m->lines+40*v->contacts[i],120,emit,ctx)<0)return -1;
    ssa_sincos(math,(uint16_t)((unsigned)v->facing<<8),&sine,&cosine);
    ex=bits((uint32_t)v->player_x+(uint32_t)mul(mul(64*65536,sine),(int32_t)v->zoom));
    ey=bits((uint32_t)v->player_y+(uint32_t)mul(mul(64*65536,cosine),(int32_t)v->zoom));
    if(emit(ctx,120+asr(sub(v->player_x,v->x),16),80-asr(sub(v->player_y,v->y),16),
        120+asr(sub(ex,v->x),16),80-asr(sub(ey,v->y),16),8)<0)return -1;
    for(i=0;i<3;++i){static const unsigned angle[]={0,0x6000,0xa000};
        ssa_sincos(math,(uint16_t)(v->heading+angle[i]),&sine,&cosine);
        x[i]=120+asr(mul(bits(((uint32_t)sine<<5)+0xc000),(int32_t)v->zoom),16);
        y[i]=80-asr(mul(bits(((uint32_t)cosine<<5)+0xc000),(int32_t)v->zoom),16);}
    for(i=0;i<3;++i)if(emit(ctx,x[i],y[i],x[(i+1)%3],y[(i+1)%3],1)<0)return -1;
    for(i=0;i<v->entity_count;++i){int32_t px=120+asr(mul(sub(v->entities[i].x,v->x),(int32_t)v->zoom),16),
        py=80-asr(mul(sub(v->entities[i].y,v->y),(int32_t)v->zoom),16);
        if(emit(ctx,px-4,py-4,px+4,py+4,8)<0 || emit(ctx,px-4,py+4,px+4,py-4,8)<0)return -1;}
    return 1;
}
static int draw_line(void *ctx,int32_t x,int32_t y,int32_t ex,int32_t ey,unsigned color)
{return ssa_automap_line(ctx,38400,x,y,ex,ey,color);}
int ssa_automap_draw(uint8_t *out,size_t size,const SsaMapAssets *m,const SsaMathTables *math,const SsaAutomapView *v)
{
    unsigned i,row;static const uint8_t colors[]={14,12,10,8,6};
    if(!out || size<38400 || !v)return -1;
    memset(out,0,38400);
    if(ssa_automap_scene(m,math,v,draw_line,out)<0)return -1;
    row=v->scanline;
    for(i=0;i<5;++i){if(row<160)memset(out+row*240,colors[i],240);row=(row+2)&255;}
    return 1;
}
