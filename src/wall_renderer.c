#include "ssa/wall_renderer.h"
#include "ssa/world_raster.h"
#include <limits.h>
#include <string.h>

static uint16_t u16(const uint8_t *p)
{return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p)
{return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16;}
static int32_t bits(uint32_t v)
{return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t hword(uint16_t v)
{return v<=INT16_MAX?(int32_t)v:-1-(int32_t)(uint16_t)~v;}
static int32_t add(int32_t a,int32_t b)
{return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b)
{return bits((uint32_t)a-(uint32_t)b);}
static int32_t asr(int32_t a,unsigned n)
{return n?bits(((uint32_t)a>>n)|(a<0?UINT32_MAX<<(32-n):0)):a;}
static int32_t mul(int32_t a,int32_t b)
{
    int64_t p=(int64_t)a*b,q=p/65536;
    if(p<0 && p%65536)--q;
    return bits((uint32_t)q);
}
static int32_t times(int32_t a,int32_t b)
{return bits((uint32_t)a*(uint32_t)b);}
static int32_t scale3(int32_t a)
{return add(a,asr(a,1));}
static uint32_t reciprocal(const SsaWallRenderTables *t,uint32_t n)
{return n<=65535u?t->reciprocal[n]:0;}
static int32_t height_fixed(uint16_t h)
{return bits((uint32_t)h<<16);}
static int span(size_t capacity,size_t at,size_t length)
{return at<=capacity && length<=capacity-at;}

int ssa_wall_render_tables_load(SsaWallRenderTables *t,const void *data,size_t size)
{
    const uint8_t *rom=(const uint8_t *)data;
    unsigned i;
    if(!t || !rom || size<0x44AEF4u)return -1;
    for(i=0;i<65536;++i)t->reciprocal[i]=u32(rom+0x34934u+i*4u);
    memcpy(t->mip_near,rom+0x44AEDCu,16);
    memcpy(t->mip_far,rom+0x44AEECu,8);
    memcpy(t->closed_top,rom+0x44A9FCu,120);
    memcpy(t->closed_bottom,rom+0x44AA74u,120);
    t->loaded=1;
    return 1;
}

static unsigned mip_level(const SsaWallRenderTables *t,int32_t depth)
{
    if(depth<=4999)return 2;
    if(depth<=5031)return t->mip_far[(unsigned)(depth-5000)/4u];
    if(depth<=9999)return 1;
    if(depth<=10063)return t->mip_near[(unsigned)(depth-10000)/4u];
    return 0;
}
static unsigned sample_shift(unsigned height)
{
    switch(height) {
    case 4:return 30;case 8:return 29;case 16:return 28;
    case 32:return 27;case 64:return 26;case 128:return 25;
    default:return 32;
    }
}
static unsigned phase_shift(unsigned height)
{
    switch(height) {
    case 16:return 11;case 32:return 10;case 64:return 9;case 128:return 8;
    default:return 0;
    }
}

static int draw_layer(SsaWallRenderer *r,unsigned texture_index,unsigned mip,
    unsigned height,unsigned column,unsigned phase,uint32_t inverse,
    int32_t origin,int32_t x,int32_t top,int32_t bottom,int middle)
{
    const SsaTextureImage *image;
    int32_t count=bottom-top;
    uint32_t step,fraction;
    if(count<=119)++count;
    if(count<=0)return 0;
    image=ssa_map_texture_image(r->bindings,texture_index,mip);
    if(!image || !image->pixels || !height)return -1;
    step=phase>=3?inverse<<(phase-3u):0;
    fraction=(uint32_t)add(times(top-60,asr(bits(inverse),3)),origin)<<phase;
    if(middle && fraction>UINT32_C(0xf0000000))fraction=0;
    if(top<0 || top>=120 || count>120 || top+count>121 ||
       (size_t)column*height+height>(size_t)image->width*image->height)return -1;
    if(top+count==121)
        return ssa_world_raster_column_clip_bottom(r->framebuffer,r->framebuffer_offset+14400u,
            r->framebuffer_offset+(size_t)top*120u+(unsigned)x,image->pixels+(size_t)column*height,
            height,fraction,step,(uint32_t)count,sample_shift(height));
    return ssa_world_raster_column(r->framebuffer,r->capacity,
        r->framebuffer_offset+(size_t)top*120u+(unsigned)x,image->pixels+(size_t)column*height,
        height,fraction,step,(uint32_t)count,sample_shift(height));
}

static int render(SsaWallRenderer *r,const SsaWallRenderCommand *c,
                  SsaWallRenderResult *out,int solid_only)
{
    const SsaMapAssets *m;
    const SsaMapTextureRecord *textures[3]={NULL,NULL,NULL},*chosen=NULL;
    const uint8_t *segment,*line,*side,*sector,*back=NULL;
    unsigned line_index,side_index,sector_index,direction,phase=0;
    unsigned wall_flags=0,plane_flags=0;
    int32_t depth,depth_step,ceiling,ceiling_step,floor,floor_step,u,u_step;
    int32_t relative_ceiling,relative_floor,eye_integer,one_over_width,x;
    int32_t upper=0,upper_step=0,lower=0,lower_step=0,origin[3]={0,0,0};
    uint16_t flags;
    size_t i;
    SsaWallRenderResult result;
    if(!r || !c || !out || r->updating || !r->bindings || !r->tables ||
       !r->tables->loaded || !r->columns || !r->framebuffer || !span(r->capacity,r->framebuffer_offset,14400) ||
       !(m=r->bindings->map) || !m->data || c->left>c->right || c->right>120 ||
       (c->segment&0x7fffu)>=m->segment_count ||
       (c->range_count && !c->ranges) || c->range_count>120 ||
       ((c->ceiling || c->floor) && !r->planes))return -1;
    if(!span(m->size,m->segments+(size_t)(c->segment&0x7fffu)*16u,16))return -1;
    segment=m->data+m->segments+(size_t)(c->segment&0x7fffu)*16u;
    line_index=u16(segment+4);direction=u16(segment+8);
    if(line_index>=m->line_count || direction>1 ||
       !span(m->size,m->lines+(size_t)line_index*40u,40))return -1;
    line=m->data+m->lines+(size_t)line_index*40u;flags=u16(line+26);
    if(solid_only && (flags&4u))return -2;
    side_index=u16(line+8+direction*2);sector_index=u16(line+12+direction*2);
    if(side_index>=m->side_count || sector_index>=m->sector_count ||
       !span(m->size,m->sides+(size_t)side_index*12u,12) ||
       !span(m->size,m->sectors+(size_t)sector_index*20u,20))return -1;
    side=m->data+m->sides+(size_t)side_index*12u;
    sector=m->data+m->sectors+(size_t)sector_index*20u;
    if(flags&4u) {
        unsigned opposite=u16(line+12+(direction^1u)*2u);
        if(opposite>=m->sector_count ||
           !span(m->size,m->sectors+(size_t)opposite*20u,20))return -1;
        back=m->data+m->sectors+(size_t)opposite*20u;
    }
    for(i=0;i<3;++i)
        if(ssa_map_side_texture(r->bindings,side_index,(unsigned)i,&textures[i])<0)return -1;
    for(i=0;i<c->range_count;++i) {
        if(c->ranges[i].left<c->left || c->ranges[i].right>c->right || c->ranges[i].right>=120 ||
           c->ranges[i].left>c->ranges[i].right ||
           (i && c->ranges[i].left<=c->ranges[i-1].right))return -1;
    }
    for(i=0;i<120;++i)if(r->columns[i].top < -1 || r->columns[i].top>120 ||
                           r->columns[i].bottom < -1 || r->columns[i].bottom>120)return -1;
    eye_integer=hword((uint16_t)((uint32_t)r->eye_z>>16));
    relative_ceiling=height_fixed((uint16_t)(u16(sector+2)-eye_integer));
    relative_floor=height_fixed((uint16_t)(u16(sector)-eye_integer));
    depth=c->depth_left;
    one_over_width=(int32_t)(r->tables->reciprocal[(unsigned)c->right-c->left+1u]>>16);
    depth_step=mul(sub(c->depth_right,depth),one_over_width);
    result=*out;result.sprite_clip_flags=0;
    result.top_clip=NULL;result.bottom_clip=NULL;result.top_clip_x=0;result.bottom_clip_x=0;
    result.ceiling=c->ceiling;result.floor=c->floor;
    if(back) {
        int32_t ff=hword(u16(sector)),fc=hword(u16(sector+2));
        int32_t bf=hword(u16(back)),bc=hword(u16(back+2));
        if(ff>bf) {result.sprite_clip_flags=1;result.bottom_height=(int16_t)ff;}
        else if(bf>eye_integer) {result.sprite_clip_flags=1;result.bottom_height=INT16_MAX;}
        if(fc<bc) {result.sprite_clip_flags|=2;result.top_height=(int16_t)fc;}
        else if(bc<eye_integer) {result.sprite_clip_flags|=2;result.top_height=INT16_MIN;}
        if(bc<=ff) {
            result.bottom_clip=r->tables->closed_bottom;
            result.sprite_clip_flags|=1;result.bottom_height=INT16_MAX;
        }
        if(bf>=fc) {
            result.top_clip=r->tables->closed_top;
            result.sprite_clip_flags|=2;result.top_height=INT16_MIN;
        }
        upper=height_fixed((uint16_t)(u16(back+2)-eye_integer));
        lower=height_fixed((uint16_t)(u16(back)-eye_integer));
        if(upper!=relative_ceiling || back[11]!=sector[11])plane_flags|=1;
        if(lower!=relative_floor || back[10]!=sector[10])plane_flags|=2;
        if(bc<=ff || bf>=fc)plane_flags=3;
        if(upper<relative_ceiling && textures[0]) {
            wall_flags|=1;
            upper_step=scale3(sub(0,mul(depth_step,upper)));
            upper=sub(60*65536,scale3(mul(upper,depth)));
            if((flags&8u) || !u16(line+36))origin[0]=sub(height_fixed(u16(sector+2)),r->eye_z);
            else origin[0]=sub(add(height_fixed(u16(back+2)),
                bits((uint32_t)textures[0]->height<<17)),r->eye_z);
            origin[0]=add(origin[0],height_fixed(u16(side+2)));
        }
        if(lower>relative_floor && textures[2]) {
            wall_flags|=4;
            lower_step=scale3(sub(0,mul(depth_step,lower)));
            lower=sub(60*65536,scale3(mul(lower,depth)));
            origin[2]=add(sub(height_fixed(u16(back)),r->eye_z),height_fixed(u16(side+2)));
        }
        chosen=(wall_flags&4u)?textures[2]:textures[0];
    } else {
        if(!textures[1])return -1;
        wall_flags=2;plane_flags=3;chosen=textures[1];
        origin[1]=(flags&16u)?sub(add(height_fixed(u16(sector)),
            bits((uint32_t)chosen->height<<17)),r->eye_z):relative_ceiling;
        origin[1]=add(origin[1],height_fixed(u16(side+2)));
        result.sprite_clip_flags=3;result.bottom_height=INT16_MAX;result.top_height=INT16_MIN;
        result.top_clip=r->tables->closed_top;result.bottom_clip=r->tables->closed_bottom;
    }
    if(wall_flags) {
        if(!chosen || !chosen->width || !chosen->height)return -1;
        phase=phase_shift(chosen->height);
    }
    ceiling=sub(60*65536,scale3(mul(relative_ceiling,depth)));
    ceiling_step=scale3(sub(0,mul(depth_step,relative_ceiling)));
    floor=sub(60*65536,scale3(mul(relative_floor,depth)));
    floor_step=scale3(sub(0,mul(depth_step,relative_floor)));
    u=mul(depth,add(c->u_left,0x2000));
    u_step=mul(sub(mul(c->depth_right,sub(c->u_right,0x2000)),u),one_over_width);
    x=c->left;r->updating=1;
    for(i=0;i<c->range_count;++i) {
        int32_t skip=c->ranges[i].left-x;
        depth=add(depth,times(depth_step,skip));ceiling=add(ceiling,times(ceiling_step,skip));
        floor=add(floor,times(floor_step,skip));u=add(u,times(u_step,skip));
        if(wall_flags&1u)upper=add(upper,times(upper_step,skip));
        if(wall_flags&4u)lower=add(lower,times(lower_step,skip));
        x=c->ranges[i].left;
        if(plane_flags&2u) {
            if(!result.floor)plane_flags&=~2u;
            else {result.floor=ssa_plane_check(r->planes,result.floor,x,c->ranges[i].right);if(!result.floor)goto failure;}
        }
        if(plane_flags&1u) {
            if(!result.ceiling)plane_flags&=~1u;
            else {result.ceiling=ssa_plane_check(r->planes,result.ceiling,x,c->ranges[i].right);if(!result.ceiling)goto failure;}
        }
        for(;x<=c->ranges[i].right;++x) {
            int32_t top=r->columns[x].top,bottom=r->columns[x].bottom;
            int32_t draw_top=asr(add(ceiling,65535),16),draw_bottom=asr(floor,16);
            if(draw_top<top+1)draw_top=top+1;
            if(plane_flags&1u) {
                int32_t end=hword((uint16_t)(draw_top-1));
                if(end>=bottom)end=bottom-1;
                if(top+1<=end) {
                    result.ceiling->top[x+1]=(uint8_t)(top+1);result.ceiling->bottom[x+1]=(uint8_t)end;top=end;
                }
            }
            if(draw_bottom>=bottom)draw_bottom=bottom-1;
            if(plane_flags&2u) {
                int32_t start=hword((uint16_t)(draw_bottom+1));
                if(start<=top)start=top+1;
                if(start<=bottom-1) {
                    result.floor->top[x+1]=(uint8_t)start;result.floor->bottom[x+1]=(uint8_t)(bottom-1);bottom=start;
                }
            }
            if(wall_flags) {
                unsigned mip=mip_level(r->tables,depth),height,width,column;
                uint32_t index,inv;
                if(mip>2)goto failure;
                height=chosen->height>>mip;width=chosen->width>>mip;
                if(!height || !width)goto failure;
                index=(uint32_t)depth>>3;inv=reciprocal(r->tables,index);
                column=((uint32_t)mul(u,asr(asr(bits(inv),3),mip))>>17)&(width-1u);
                inv=reciprocal(r->tables,index+(index>>1));
                if(wall_flags&2u) {
                    top=120;bottom=-1;
                    if(draw_layer(r,u16(side+6),mip,height,column,phase,inv,origin[1],x,draw_top,draw_bottom,1))goto failure;
                } else {
                    if(wall_flags&1u) {
                        int32_t end=asr(upper,16);upper=add(upper,upper_step);
                        if(end>bottom)end=bottom-1;
                        if(end>=draw_top) {
                            top=end;
                            if(draw_layer(r,u16(side+4),mip,height,column,phase,inv,origin[0],x,draw_top,end,0))goto failure;
                        } else if(plane_flags&1u)top=draw_top-1;
                    } else if(plane_flags&1u)top=draw_top-1;
                    if(wall_flags&4u) {
                        int32_t start=asr(lower,16);lower=add(lower,lower_step);
                        if(start<=top)start=top+1;
                        if(start<=draw_bottom) {
                            bottom=start;
                            if(draw_layer(r,u16(side+8),mip,height,column,phase,inv,origin[2],x,start,draw_bottom,0))goto failure;
                        } else if(plane_flags&2u)bottom=draw_bottom+1;
                    } else if(plane_flags&2u)bottom=draw_bottom+1;
                }
            }
            r->columns[x].top=(int16_t)hword((uint16_t)top);r->columns[x].bottom=(int16_t)hword((uint16_t)bottom);
            depth=add(depth,depth_step);ceiling=add(ceiling,ceiling_step);floor=add(floor,floor_step);u=add(u,u_step);
        }
    }
    for(i=0;i<2;++i) {
        const uint8_t **clip=i?&result.bottom_clip:&result.top_clip;
        if((result.sprite_clip_flags&(i?1u:2u)) && !*clip) {
            size_t n=(size_t)c->right-c->left,k,at;
            if(!r->planes || !r->openings || !span(r->opening_capacity,r->planes->opening_cursor,n))goto failure;
            at=r->planes->opening_cursor;
            for(k=0;k<n;++k)r->openings[at+k]=(uint8_t)(i?r->columns[c->left+k].bottom:r->columns[c->left+k].top);
            *clip=r->openings+at;
            if(i)result.bottom_clip_x=c->left;else result.top_clip_x=c->left;
            r->planes->opening_cursor+=n;
        }
    }
    *out=result;r->updating=0;return 0;
failure:
    *out=result;r->updating=0;return -1;
}
int ssa_wall_render(SsaWallRenderer *r,const SsaWallRenderCommand *c,SsaWallRenderResult *out)
{return render(r,c,out,0);}
int ssa_wall_render_solid(SsaWallRenderer *r,const SsaWallRenderCommand *c,SsaWallRenderResult *out)
{return render(r,c,out,1);}
