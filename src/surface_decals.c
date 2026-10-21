#include "ssa/surface_decals.h"
#include "ssa/plane_renderer.h"
#include "ssa/render_view.h"
#include "ssa/water.h"
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdatomic.h>
#endif

static uint32_t hash(uint32_t n)
{n^=n>>16;n*=UINT32_C(0x7feb352d);n^=n>>15;n*=UINT32_C(0x846ca68b);return n^(n>>16);}
static unsigned smooth(unsigned t)
{return t*t*(768-2*t)/65536;}
/* Periodic noise gives connected ports identical detail on opposite edges. */
static unsigned noise(uint32_t seed,unsigned x,unsigned y,unsigned spacing)
{
    unsigned u=x*128*256/127,v=y*128*256/127,wrap=128/spacing-1;
    unsigned ix=(u/(spacing*256))&wrap,iy=(v/(spacing*256))&wrap;
    unsigned fx=smooth(u%(spacing*256)/spacing),fy=smooth(v%(spacing*256)/spacing);
    unsigned a=hash(seed+ix+iy*67)&4095,b=hash(seed+((ix+1)&wrap)+iy*67)&4095;
    unsigned c=hash(seed+ix+((iy+1)&wrap)*67)&4095,d=hash(seed+((ix+1)&wrap)+((iy+1)&wrap)*67)&4095;
    a=(a*(256-fx)+b*fx)/256;c=(c*(256-fx)+d*fx)/256;
    return (a*(256-fy)+c*fy)/256;
}
static unsigned error(uint16_t color,unsigned r,unsigned g,unsigned b)
{
    int dr=(int)(color&31)*1000-(int)r,dg=(int)((color>>5)&31)*1000-(int)g,db=(int)((color>>10)&31)*1000-(int)b;
    return (unsigned)(dr*dr)+(unsigned)(dg*dg)+(unsigned)(db*db);
}
static unsigned brightness(uint16_t color)
{return (color&31)*3+((color>>5)&31)*6+((color>>10)&31);}
static unsigned occupied(unsigned x,unsigned y,uint32_t seed,unsigned ceiling)
{
    /* Fixed tile-cluster stamps: random placement/orientation selects pieces,
     * never a generated ellipse or a continuously deformed texture. */
    static const uint16_t stamps[]={0x0020,0x0020,0x0060,0x0260,0x0270,0x0272,
        0x0777,0x0eee,0x0e66,0x0f66,0x0776,0x0eec};
    uint32_t n=hash((x/4)*UINT32_C(0x9e3779b9)^(y/4)*UINT32_C(0x85ebca6b)^seed);
    unsigned lx=x&3,ly=y&3,i;
    if(n%100>=(ceiling?95u:100u))return 0;
    if(n&0x100)lx=3-lx;
    for(i=0;i<((n>>12)&3);++i){unsigned swap=lx;lx=3-ly;ly=swap;}
    return (stamps[(n>>16)%(sizeof stamps/sizeof stamps[0])]>>(ly*4+lx))&1;
}
static unsigned profile(int distance,unsigned radius)
{
    int value=(int)radius-distance;
    return value<=0?0:value>=14000?255:smooth((unsigned)value*256/14000);
}
/* Texture variations share their edge pixels. Fade their interior differences
 * in over16 texels so the selected tile remains compatible with every bank. */
static unsigned blend_weight(unsigned x,unsigned y)
{
    unsigned edge=x<127-x?x:127-x;
    if(y<edge)edge=y;
    if(127-y<edge)edge=127-y;
    return edge>=16?256:smooth(edge*16);
}
static unsigned tile_noise(uint32_t seed,unsigned bank,unsigned x,unsigned y,unsigned spacing,unsigned blend)
{
    unsigned shared=noise(seed,x,y,spacing);
    return bank?(shared*(256-blend)+noise(hash(seed+bank*1987),x,y,spacing)*blend)/256:shared;
}
static unsigned cell_index(int32_t x,int32_t y)
{
    return (((uint32_t)y>>7)&63)*64+(((uint32_t)x>>7)&63);
}
/* One immutable atlas per process; only palette/layout data belongs to a level. */
static SsaSurfaceDecalAtlas shared_atlas;
static void build_atlas(SsaSurfaceDecalAtlas *atlas,const SsaSurfaceDecals *out)
{
    unsigned surface,bank,variant,x,y;
    for(surface=0;surface<2;++surface)for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank){uint32_t seed=hash(surface*31+9);
        for(y=0;y<128;++y)for(x=0;x<128;++x){int dx=(int)x*2-127,dy=(int)y*2-127;
            unsigned blend=blend_weight(x,y);
            unsigned value=(3*tile_noise(seed,bank,x,y,64,blend)+tile_noise(seed+41,bank,x,y,32,blend)+tile_noise(seed+73,bank,x,y,16,blend))/5;
            /* Coarser, less aggressive erosion leaves fewer, larger openings
             * rather than shredding each stain into many small fragments. */
            unsigned wear=(3*tile_noise(seed+211,bank,x,y,32,blend)+tile_noise(seed+257,bank,x,y,16,blend))/4;
            unsigned remaining=wear<=600?0:wear>=2100?255:smooth((wear-600)*256/1500);
            /* Inset the curved shoulders farther from exposed tile corners.
             * Ports still share their profiles, including between banks. */
            int radius_noise=10000+((int)tile_noise(seed+107,bank,x,y,32,blend)-2048)*2;
            unsigned radius=(unsigned)(radius_noise>12300?12300:radius_noise);
            unsigned center=profile(dx*dx+dy*dy,radius),vertical=profile(dx*dx,radius),horizontal=profile(dy*dy,radius);
            unsigned quadrant=y<64?(x<64?1:2):(x<64?8:4);
            unsigned px=x==127?0:x,py=y==127?0:y;
            uint32_t point=px*UINT32_C(0x9e3779b9)^py*UINT32_C(0x85ebca6b)^seed;
            uint32_t speckle=hash(point+71);
            /* Unordered, tile-sized speckling avoids the old16x16 rank lattice
             * and its locally alternating checker pattern. Shared border
             * thresholds preserve exact joins between all texture banks. */
            if(bank && (hash(point+317)&255)<blend)speckle=hash(point+bank*UINT32_C(0x68e31da4));
            atlas->thresholds[surface][bank][y*128+x]=(uint16_t)speckle;
            if(x>1 && x<126 && y>1 && y<126 && hash(x+y*131+surface*1987+bank*613+47)%(surface?96u:64u)==0)
                atlas->masks[surface][bank][SSA_SURFACE_NOISE_TILE][y*128+x]=255;
            for(variant=1;variant<SSA_SURFACE_STAIN_TILES;++variant){unsigned connections=out->connections[variant],shape=center,strength;
                if(((connections&SSA_SURFACE_STAIN_NORTH) && y<64) || ((connections&SSA_SURFACE_STAIN_SOUTH) && y>=64))
                    if(vertical>shape)shape=vertical;
                if(((connections&SSA_SURFACE_STAIN_WEST) && x<64) || ((connections&SSA_SURFACE_STAIN_EAST) && x>=64))
                    if(horizontal>shape)shape=horizontal;
                if(out->corners[variant]&quadrant){
                    unsigned wx=smooth((unsigned)(dx<0?-dx:dx)*256/127);
                    unsigned wy=smooth((unsigned)(dy<0?-dy:dy)*256/127);
                    /* Blend diagonal support into the curved shoulders. A
                     * whole-quadrant fill made visible rectangular density
                     * steps along the tile's horizontal/vertical midlines. */
                    shape+=(255-shape)*wx*wy/65536;
                }
                strength=shape*(10*4095+value*5)/(15*4095);
                /* Negative noise reveals the underlying surface in irregular
                 * patches. Even full interior pieces remain porous stains. */
                strength=strength*remaining/255;
                if(surface)strength=strength*3/4;
                /* Bake the whole feather and mottled interior into two-color
                 * pixels. Do not saturate low coverage into solid fills. */
                atlas->masks[surface][bank][variant][y*128+x]=
                    atlas->thresholds[surface][bank][y*128+x]<strength*257?255:0;
            }
        }
    }
}
#ifdef _WIN32
static INIT_ONCE atlas_once=INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK initialize_atlas(PINIT_ONCE once,PVOID context,PVOID *unused)
{
    (void)once;(void)unused;build_atlas(&shared_atlas,context);return TRUE;
}
static void ensure_atlas(const SsaSurfaceDecals *d)
{InitOnceExecuteOnce(&atlas_once,initialize_atlas,(void *)d,NULL);}
#else
static atomic_uint atlas_state=ATOMIC_VAR_INIT(0);
static void ensure_atlas(const SsaSurfaceDecals *d)
{
    unsigned expected=0;
    if(atomic_load_explicit(&atlas_state,memory_order_acquire)==2)return;
    if(atomic_compare_exchange_strong_explicit(&atlas_state,&expected,1,memory_order_acq_rel,memory_order_acquire)){
        build_atlas(&shared_atlas,d);atomic_store_explicit(&atlas_state,2,memory_order_release);
    }else while(atomic_load_explicit(&atlas_state,memory_order_acquire)!=2){}
}
#endif
int ssa_surface_decals_init(SsaSurfaceDecals *out,const uint16_t palette[256],unsigned level)
{
    unsigned surface,x,y,c,n,i,count=1;
    uint8_t cells[64*64],lookup[256]={0};
    if(!out || !palette || level>=16)return -1;
    memset(out,0,sizeof(*out));out->seed=hash(level+87);out->level=level;
    for(n=0;n<16;++n){unsigned allowed=((n&9)==9?1:0)|((n&3)==3?2:0)|((n&6)==6?4:0)|((n&12)==12?8:0);
        for(i=0;i<16;++i)if(!(i&~allowed)){
            if(count>=SSA_SURFACE_STAIN_TILES)return -1;
            lookup[n|(i<<4)]=(uint8_t)count;out->connections[count]=(uint8_t)n;out->corners[count++]=(uint8_t)i;}}
    if(count!=SSA_SURFACE_STAIN_TILES)return -1;
    for(surface=0;surface<2;++surface){uint32_t seed=out->seed^(surface?UINT32_C(0x12343):0);
        for(y=0;y<64;++y)for(x=0;x<64;++x)cells[y*64+x]=(uint8_t)occupied(x,y,seed,surface);
        for(y=0;y<64;++y)for(x=0;x<64;++x)if(cells[y*64+x]){
            unsigned connections=(cells[((y-1)&63)*64+x]?SSA_SURFACE_STAIN_NORTH:0)|
                (cells[y*64+((x+1)&63)]?SSA_SURFACE_STAIN_EAST:0)|
                (cells[((y+1)&63)*64+x]?SSA_SURFACE_STAIN_SOUTH:0)|
                (cells[y*64+((x-1)&63)]?SSA_SURFACE_STAIN_WEST:0);
            unsigned corners=((connections&9)==9 && cells[((y-1)&63)*64+((x-1)&63)]?1:0)|
                ((connections&3)==3 && cells[((y-1)&63)*64+((x+1)&63)]?2:0)|
                ((connections&6)==6 && cells[((y+1)&63)*64+((x+1)&63)]?4:0)|
                ((connections&12)==12 && cells[((y+1)&63)*64+((x-1)&63)]?8:0);
            out->tiles[surface][y*64+x]=lookup[connections|(corners<<4)];
        }
        for(y=0;y<64;++y)for(x=0;x<64;++x)
            if(!cells[y*64+x] && hash(seed+x*613+y*1987)%4==0)
                out->tiles[surface][y*64+x]=SSA_SURFACE_NOISE_TILE;
        for(y=0;y<64;++y)for(x=0;x<64;++x)
            out->variations[surface][y*64+x]=(uint8_t)(hash(seed^x*UINT32_C(0x27d4eb2d)^y*UINT32_C(0x165667b1))%SSA_SURFACE_DECAL_VARIATIONS);
    }
    ensure_atlas(out);out->atlas=&shared_atlas;
    for(c=0;c<256;++c){unsigned color=palette[c]&0x7fff;
        /* One fixed darker shade per original surface color. Preserve
         * fractional targets until matching the chapter's original palette. */
        unsigned gain=940,r=(color&31)*gain;
        unsigned g=((color>>5)&31)*gain,b=((color>>10)&31)*gain;
        unsigned red=color&31,green=(color>>5)&31,blue=(color>>10)&31;
        unsigned best_error=UINT32_MAX,best=c;
        out->water_floor[c]=(uint8_t)(blue>=red+4 && blue>=green+4);
        /* Caesar's Palace: the U.M.T floor (sectors43/44/62) is blue stone,
         * not water. Keep the material dry in every shading path. */
        if(level==11 && c==36)out->water_floor[c]=0;
        /* The Forum of Mars blue floor (sector21) is also dry stone. */
        if(level==6 && c==130)out->water_floor[c]=0;
        /* A6% target can quantize back to the surface itself. Require real
         * contrast so dark ceilings and every solid floor receive the art. */
        if(c)for(i=1;i<256;++i)if(brightness(palette[i])<brightness((uint16_t)color)){
            unsigned d=error(palette[i],r,g,b);if(d<best_error){best_error=d;best=i;}}
        /* A black plane has no darker palette entry; use its closest distinct
         * opaque shade rather than silently losing all decal detail. */
        if(c && best==c)for(i=1;i<256;++i)if((palette[i]&0x7fff)!=color){
            unsigned d=error(palette[i],r,g,b);if(d<best_error){best_error=d;best=i;}}
        out->shades[c]=(uint8_t)best;
    }
    out->loaded=1;return 1;
}
uint8_t ssa_surface_decals_texel(const SsaSurfaceDecals *d,unsigned surface,int32_t x,int32_t y)
{
    unsigned cell,tile,bank;
    if(!d || d->loaded!=1 || surface>1)return 0;
    cell=cell_index(x,y);tile=d->tiles[surface][cell];bank=d->variations[surface][cell];
    return d->atlas->masks[surface][bank][tile][((unsigned)y&127)*128+((unsigned)x&127)];
}
uint8_t ssa_surface_decals_sample(const SsaSurfaceDecals *d,unsigned ceiling,uint8_t color,double wx,double wy,double distance)
{
    int32_t tx,ty;unsigned x,y,coverage,threshold;
    if(!d || d->loaded!=1 || ceiling>1 || !color || !(distance>=0 && distance<1536) ||
       !(wx>-2147483648.0 && wx<2147483648.0 && wy>-2147483648.0 && wy<2147483648.0))return color;
    if(!ceiling && d->water_floor[color])return color;
    wx/=2;wy/=2;tx=(int32_t)wx;ty=(int32_t)wy;if(wx<tx)--tx;if(wy<ty)--ty;
    x=(unsigned)tx&127;y=(unsigned)ty&127;
    if(!ssa_surface_decals_texel(d,ceiling,tx,ty))return color;
    coverage=65536;
    if(distance>768){double fade=(1536-distance)/768;coverage=(unsigned)(coverage*fade*fade*(3-2*fade));}
    if(coverage==65536)return d->shades[color];
    threshold=d->atlas->thresholds[ceiling][d->variations[ceiling][cell_index(tx,ty)]][y*128+x];
    return threshold<coverage?d->shades[color]:color;
}
uint8_t ssa_surface_decals_sample_plane(const SsaSurfaceDecals *d,unsigned ceiling,uint8_t color,
    double x,double y,double height,double distance)
{
    if(d && ssa_surface_edge_decal(d->edge_scene,d,ceiling,color,x,y,height,distance))return d->shades[color];
    return ssa_surface_decals_sample(d,ceiling,color,x,y,distance);
}
uint8_t ssa_surface_decals_sample_cached(const SsaSurfaceDecals *d,SsaSurfaceSampleCache *cache,
    unsigned ceiling,uint8_t color,double x,double y,double height,double distance)
{
    int hit;SsaSurfaceSample *entry=ssa_surface_sample_slot(cache,x,y,height,distance,ceiling,color,&hit);
    uint8_t result;if(hit)return (uint8_t)entry->value;
    result=ssa_surface_decals_sample_plane(d,ceiling,color,x,y,height,distance);
    if(entry)entry->value=result;
    return result;
}
int ssa_surface_decals_reference(const SsaSurfaceDecals *d,const SsaPlanePool *pool,const SsaRenderCamera *camera,uint8_t *fb,size_t capacity)
{
    unsigned i;double eye,sine,cosine,cx,cy;
    if(!d || !d->loaded || !pool || pool->count>SSA_PLANE_LIMIT || !camera || !fb || capacity<14400)return -1;
    eye=camera->z/65536.0;sine=-camera->sine/65536.0;cosine=camera->cosine/65536.0;
    cx=camera->x/65536.0;cy=camera->y/65536.0;
    for(i=0;i<pool->count;++i){const SsaPlane *p=&pool->records[i];int x,y;unsigned ceiling=p->height>eye;
        if(!p->color || p->min_x>p->max_x)continue;
        if(!ceiling && ssa_surface_decals_water(d,(uint8_t)p->color,p->height))continue;
        if(p->min_x<0 || p->max_x>=120)return -1;
        for(x=p->min_x;x<=p->max_x;++x){unsigned top=p->top[x+1],bottom=p->bottom[x+1];
            if(top==255 || top>bottom)continue;
            if(bottom>=120)return -1;
            for(y=(int)top;y<=(int)bottom;++y){double distance=(eye-p->height)*(0x55b05a/65536.0)*1.5/(y+0.5-60);
                double lateral=(x+0.5-60)/(0x55b05a/65536.0),wx=cx+(sine+cosine*lateral)*distance,wy=cy+(cosine-sine*lateral)*distance;
                fb[y*120+x]=ssa_surface_decals_sample_plane(d,ceiling,(uint8_t)p->color,wx,wy,p->height,distance);
            }
        }
    }
    return 1;
}
