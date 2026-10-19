#include "ssa/classic_hud.h"
#include "ssa/map_assets.h"
#include <string.h>
#include "classic_time_strike.inc"
#include "classic_tommy_ammo.inc"

static int image_palette(const SsaSpriteAssets *a,unsigned id,int left,int top,uint8_t *out,const uint8_t *swap)
{
    uint8_t pixels[2048];const SsaSpriteImage *im;unsigned x,y;
    if(id>=SSA_SPRITE_IMAGE_COUNT)return -1;
    im=&a->images[id];
    if(!im->width || !im->height || (size_t)im->width*im->height>sizeof pixels ||
       ssa_sprite_image_indices(im,pixels,im->width,sizeof pixels)!=1)return -1;
    for(y=0;y<im->height;++y)for(x=0;x<im->width;++x){int xx=left+(int)x,yy=top+(int)y;
        unsigned color=pixels[y*im->width+x];
        if(color && xx>=0 && xx<240 && yy>=0 && yy<160)
            out[yy*240+xx]=swap?swap[color]:(uint8_t)color;}
    return 1;
}
static int image(const SsaSpriteAssets *a,unsigned id,int left,int top,uint8_t *out)
{return image_palette(a,id,left,top,out,NULL);}
static unsigned frame(const SsaWeaponAnimations *a,unsigned animation)
{return a->words[a->offsets[animation]]&0x3fff;}
/* Blit the supplied framed PNG pixel for pixel. Recolor only: source alpha and
 * layout remain intact, with no world-sprite lookup, crop, fit or resampling. */
static int time_strike_icon(const SsaSpriteAssets *a,int left,int top,uint8_t *out)
{
    unsigned x,y,i,n,found=0;uint8_t shades[32]={0};unsigned errors[32];
    for(n=0;n<32;++n)errors[n]=UINT32_MAX;
    for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        if(c&0x7c1f)continue;
        found=1;
        for(n=0;n<32;++n){unsigned delta=g>n?g-n:n-g;
            if(delta<errors[n]){errors[n]=delta;shades[n]=(uint8_t)i;}}
    }
    if(!found)return -1;
    for(y=0;y<24;++y)for(x=0;x<16;++x){unsigned c=classic_time_strike_rgb555[y][x];int xx=left+(int)x,yy=top+(int)y;
        if(!(c&0x8000) && xx>=0 && xx<240 && yy>=0 && yy<160){
            unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31,light=r>g?r:g;
            if(b>light)light=b;
            if(g && !r && !b)light=31; /* Match the other HUD frames. */
            out[yy*240+xx]=shades[light];}
    }
    return 1;
}
static int number(const SsaSpriteAssets *a,const SsaWeaponAnimations *anim,
    uint32_t value,unsigned digits,unsigned base,int x,int y,unsigned spacing,unsigned padded,const uint8_t *swap,uint8_t *out)
{
    unsigned i;uint32_t divisor=1;
    for(i=1;i<digits;++i)divisor*=10;
    value%=divisor*10;
    for(i=0;i<digits;++i){unsigned digit=value/divisor%10;
        if((padded || i+1==digits || value>=divisor) &&
           image_palette(a,frame(anim,base+digit),x+(int)(i*spacing),y,out,swap)!=1)return -1;
        divisor/=10;}
    return 1;
}
static void fill(uint8_t *out,int x,int y,unsigned width,unsigned height,unsigned color)
{
    unsigned xx,yy;for(yy=0;yy<height;++yy)for(xx=0;xx<width;++xx)
        out[(y+(int)yy)*240+x+(int)xx]=(uint8_t)color;
}
static void plate(uint8_t *background,int x,int y,unsigned width,unsigned height)
{if(background)fill(background,x,y,width,height,1);}
/* Seven-pixel glyphs fit inside the retained panel's left square, leaving
 * its frame/divider intact. Light from above/left uses the retained green ramp. */
static void bar_icon(const SsaSpriteAssets *a,uint8_t *out,int x,int y,unsigned color,unsigned boss)
{
    static const char icons[2][7][8]={
        {"0033300","0032100","0032100","3332100","3232133","3232121","1111111"},
        {"0333320","3322221","3002001","3002001","0322210","0030100","0011100"}
    };
    unsigned xx,yy,n,i,peak=(a->palette[color]>>5)&31;uint8_t shades[4]={0,0,0,(uint8_t)color};
    for(n=1;n<3;++n){unsigned error=UINT32_MAX,target=peak*n/3;
        for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31,d;
            if(c&0x7c1f)continue;
            d=g>target?g-target:target-g;
            if(d<error){error=d;shades[n]=(uint8_t)i;}}}
    for(yy=0;yy<7;++yy)for(xx=0;xx<7;++xx){unsigned shade=(unsigned)(icons[boss][yy][xx]-'0');
        if(shade)out[(y+1+(int)yy)*240+x+1+(int)xx]=shades[shade];}
}
uint16_t ssa_classic_hud_darken555(uint16_t color)
{
    unsigned r=((color&31)*45+50)/100,g=(((color>>5)&31)*45+50)/100,b=(((color>>10)&31)*45+50)/100;
    return (uint16_t)(r|(g<<5)|(b<<10));
}
uint32_t ssa_classic_hud_darken888(uint32_t color)
{
    unsigned r=(((color>>16)&255)*45+50)/100,g=(((color>>8)&255)*45+50)/100,b=((color&255)*45+50)/100;
    return (color&UINT32_C(0xff000000))|(r<<16)|(g<<8)|b;
}
unsigned ssa_classic_hud_alpha(unsigned x,unsigned y,unsigned ticks,unsigned boss)
{
    unsigned top=boss?40:28;
    if(ticks>180 && ticks<=300 && x>=100 && x<140 && y>=top && y<top+16)
        return ((300-ticks)/15)%2?0:255;
    return 255;
}
uint32_t ssa_classic_hud_blend888(uint32_t foreground,uint32_t behind,unsigned alpha)
{
    unsigned r=(((foreground>>16)&255)*alpha+((behind>>16)&255)*(255-alpha)+127)/255;
    unsigned g=(((foreground>>8)&255)*alpha+((behind>>8)&255)*(255-alpha)+127)/255;
    unsigned b=((foreground&255)*alpha+(behind&255)*(255-alpha)+127)/255;
    return (behind&UINT32_C(0xff000000))|(r<<16)|(g<<8)|b;
}
uint16_t ssa_classic_hud_blend555(uint16_t foreground,uint16_t behind,unsigned alpha)
{
    unsigned r=((foreground&31)*alpha+(behind&31)*(255-alpha)+127)/255;
    unsigned g=(((foreground>>5)&31)*alpha+((behind>>5)&31)*(255-alpha)+127)/255;
    unsigned b=(((foreground>>10)&31)*alpha+((behind>>10)&31)*(255-alpha)+127)/255;
    return (uint16_t)(r|(g<<5)|(b<<10));
}
static unsigned green(const SsaSpriteAssets *a)
{
    unsigned i,best=1,score=0;
    /* Choose a bright green already in the original OBJ palette. */
    for(i=1;i<256;++i){unsigned c=a->palette[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        unsigned n=g*2>r+b?g*2-r-b:0;if(n>score){score=n;best=i;}}
    return best;
}
static unsigned neutral(const SsaSpriteAssets *a,unsigned target)
{
    unsigned i,best=1,error=UINT32_MAX;
    for(i=1;i<256;++i){unsigned c=a->palette[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        unsigned dr=target>r?target-r:r-target,dg=target>g?target-g:g-target,db=target>b?target-b:b-target;
        unsigned n=dr*dr+dg*dg+db*db;if(n<error){error=n;best=i;}}
    return best;
}
/* Swap the green ramp to matching neutral shades, leaving all other indices,
 * black and transparency alone. Never flatten a shaded glyph to one color. */
static int white_palette(const SsaSpriteAssets *a,uint8_t swap[256])
{
    unsigned i,n,errors[32],found=0;uint8_t gray[32]={0};
    for(n=0;n<32;++n)errors[n]=UINT32_MAX;
    for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        if(r!=g || g!=b)continue;
        found=1;
        for(n=0;n<32;++n){unsigned d=g>n?g-n:n-g;
            if(d<errors[n]){errors[n]=d;gray[n]=(uint8_t)i;}}
    }
    if(!found)return -1;
    for(i=0;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        swap[i]=(i && g && !(c&0x7c1f))?gray[g]:(uint8_t)i;}
    return 1;
}
static int green_image(const SsaSpriteAssets *a,unsigned id,int left,int top,unsigned all_colors,uint8_t *out)
{
    uint8_t pixels[2048],swap[256];unsigned i,j,peak=0,target=(a->palette[green(a)]>>5)&31;
    const SsaSpriteImage *im=&a->images[id];size_t count=(size_t)im->width*im->height;
    if(!count || count>sizeof pixels || ssa_sprite_image_indices(im,pixels,im->width,sizeof pixels)!=1)return -1;
    for(i=0;i<count;++i)if(pixels[i]){unsigned c=a->palette[pixels[i]]&0x7fff,g=(c>>5)&31;
        if(all_colors){if((c&31)>g)g=c&31;if((c>>10)>g)g=c>>10;}
        if((all_colors || !(c&0x7c1f)) && g>peak)peak=g;}
    if(!peak)return -1;
    /* Match the HUD's brightest green while retaining the lettering's shading,
     * black pixels and transparency. Use also converts its olive source ramp;
     * Analyzing keeps its existing green-only mapping. */
    for(i=0;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        swap[i]=(uint8_t)i;
        if(all_colors){if((c&31)>g)g=c&31;if((c>>10)>g)g=c>>10;}
        if(i && g && (all_colors || !(c&0x7c1f))){unsigned light=(g*target+peak/2)/peak,error=UINT32_MAX;
            for(j=1;j<256;++j){unsigned shade=a->palette[j]&0x7fff,n=(shade>>5)&31,d;
                if(shade&0x7c1f)continue;
                d=n>light?n-light:light-n;
                if(d<error){error=d;swap[i]=(uint8_t)j;}}}}
    return image_palette(a,id,left,top,out,swap);
}
int ssa_classic_hud_use(const SsaSpriteAssets *a,uint8_t *out,size_t capacity)
{
    if(!a || a->loaded!=1 || !out || capacity<38400)return -1;
    return green_image(a,77,112,92,1,out);
}
static int analyzing(const SsaSpriteAssets *a,int top,uint8_t *out)
{return green_image(a,79,100,top,0,out);}
/* Recolor only the green shading ramp, retaining black, transparency and every
 * sprite pixel. Red alerts use the nearest retained red tones; low armor uses
 * TFE's light-gray tint (0xBF, about23/31), distinct from the white flash. */
static int alert_palette(const SsaSpriteAssets *a,uint8_t swap[256],unsigned gray)
{
    uint8_t shades[32];unsigned n,i;
    for(n=0;n<32;++n){unsigned target=gray?(n*23+15)/31:n,best=0,error=UINT32_MAX;
        for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,r=c&31,g=(c>>5)&31,b=(c>>10)&31;
            unsigned dr=target>r?target-r:r-target,dg,db,d;
            if(gray && (r!=g || g!=b))continue;
            dg=gray?(target>g?target-g:g-target):g;
            db=gray?(target>b?target-b:b-target):b;
            d=dr*dr+dg*dg+db*db;
            if(d<error){error=d;best=i;}}
        if(!best)return -1;
        shades[n]=(uint8_t)best;
    }
    for(i=0;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        swap[i]=(i && g && !(c&0x7c1f))?shades[g]:(uint8_t)i;}
    return 1;
}
int ssa_classic_hud_inbox_position(unsigned ticks,unsigned boss,int *left,int *top,unsigned *attention)
{
    unsigned ratio;
    if(ticks>300 || boss>1 || !left || !top || !attention)return -1;
    /* Snap inward as analysis ends, hold two seconds, then snap back. */
    ratio=ticks>60 && ticks<=180?30:0;
    *left=215-(int)((107*ratio+15)/30);
    *top=2+(int)(((boss?38:26)*ratio+15)/30);*attention=ratio;return 1;
}
int ssa_classic_hud_netricsa(const SsaSpriteAssets *a,const SsaWeaponAnimations *anim,
    unsigned unread,unsigned ticks,unsigned boss,uint8_t *out,uint8_t *background,size_t capacity)
{
    uint8_t swap[256];unsigned white,attention;int left,top;
    if(!a || a->loaded!=1 || !anim || anim->loaded!=1 || !out || capacity<38400 ||
       ssa_classic_hud_inbox_position(ticks,boss,&left,&top,&attention)!=1)return -1;
    if(!unread)return 1;
    white=ticks && ((300-ticks)/15)%2==0;
    if(white_palette(a,swap)!=1)return -1;
    plate(background,left,top,23,10);
    /* Keep the green inbox's exact pixels in both blink phases. */
    if(image_palette(a,65,left,top,out,white?swap:NULL)!=1 ||
       /* Two8px glyphs at5px spacing fit the panel's13px count area. */
       number(a,anim,unread>99?99:unread,2,0x2e,left+10,top+2,5,0,white?swap:NULL,out)!=1)return -1;
    if(ticks>180){
        /* Retain the ROM lettering at the existing upper-center position. */
        if(analyzing(a,boss?40:28,out)!=1)return -1;
    }
    return 1;
}
int ssa_classic_hud_text_load(SsaClassicHudText *out,const uint8_t *rom,size_t size)
{
    SsaClassicHudText next={0};unsigned i;
    if(!out || !rom || size!=0x800000 ||
       !ssa_lz77_decode(rom+0xb7a60,size-0xb7a60,next.pixels,sizeof next.pixels,NULL))return -1;
    memcpy(next.glyph,rom+0x44c264,sizeof next.glyph);
    memcpy(next.metrics,rom+0xb7ea8,sizeof next.metrics);
    for(i=0;i<16;++i){unsigned at=0xa8168+2*(4*16+i);
        next.colors[i]=(uint16_t)(rom[at]|(unsigned)rom[at+1]<<8);}
    next.loaded=1;*out=next;return 1;
}
int ssa_classic_hud_text(const SsaClassicHudText *font,const SsaSpriteAssets *a,
    const char *text,int top,uint8_t *out,uint8_t *background,size_t capacity)
{
    uint8_t shades[32]={0},colors[16];unsigned errors[32],i,n;
    if(!font || font->loaded!=1 || !a || a->loaded!=1 || !text || !out || capacity<38400)return -1;
    for(n=0;n<32;++n)errors[n]=UINT32_MAX;
    for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        if((c&31)!=g || ((c>>10)&31)!=g)continue;
        for(n=0;n<32;++n){unsigned delta=g>n?g-n:n-g;
            if(delta<errors[n]){errors[n]=delta;shades[n]=(uint8_t)i;}}}
    for(i=0;i<16;++i){unsigned c=font->colors[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31;
        colors[i]=shades[r>g?(r>b?r:b):(g>b?g:b)];}
    while(*text){const char *end=text;unsigned width=0;int left,x;
        for(;*end && *end!='\n';++end){unsigned ch=(unsigned char)*end,glyph=font->glyph[ch];
            if(ch==32)width+=4;
            else if(ch>32){if(glyph>=126)return -1;
                width+=(font->metrics[glyph*2+1]==0xcd?1:font->metrics[glyph*2+1])+1;}}
        left=120-(int)(width/2);x=left;
        if(background && width){int l=left-2,r=left+(int)width+2,t=top-1,b=top+9;
            if(l<0)l=0;
            if(r>240)r=240;
            if(t<0)t=0;
            if(b>160)b=160;
            if(r>l && b>t)plate(background,l,t,(unsigned)(r-l),(unsigned)(b-t));}
        for(;text<end;++text){unsigned ch=(unsigned char)*text,glyph=font->glyph[ch],xx,yy;
            if(ch==32){x+=4;continue;}if(ch<=32)continue;
            for(yy=0;yy<8;++yy)for(xx=0;xx<8;++xx){int dx=x-font->metrics[glyph*2]+(int)xx,dy=top+(int)yy;
                unsigned color=(font->pixels[glyph*32+yy*4+xx/2]>>((xx&1)*4))&15;
                if(color && dx>=0 && dx<240 && dy>=0 && dy<160)out[dy*240+dx]=colors[color];}
            x+=(font->metrics[glyph*2+1]==0xcd?1:font->metrics[glyph*2+1])+1;
        }
        if(*text)++text;
        top+=8;
    }
    return 1;
}
int ssa_classic_hud_weapon_wheel(const SsaSpriteAssets *a,const SsaInventory *inventory,
    unsigned selected,
    uint8_t *out,uint8_t *background,size_t capacity)
{
    static const unsigned images[10]={68,67,69,70,71,72,74,73,0,75};
    unsigned weapons[10],count=0,i,x,y;uint8_t swap[256],pixels[2048];
    int left;const int top=103; /* Clear the armor row, even with ten entries. */
    if(!a || a->loaded!=1 || !inventory || !out || capacity<38400 ||
       selected<1 || selected>10 || white_palette(a,swap)!=1)return -1;
    for(i=0;i<10;++i)if((inventory->weapons&(1u<<i)) && inventory->ammo[i])weapons[count++]=i+1;
    if(!count)return 1;
    left=(240-(int)(count*17-1))/2;
    for(i=0;i<count;++i){unsigned weapon=weapons[i],active=weapon==selected;
        int at=left+(int)i*17;
        plate(background,at,top,16,17);
        if(weapon==9){if(time_strike_icon(a,at,top,out)!=1)return -1;}
        else{const SsaSpriteImage *im=&a->images[images[weapon-1]];
            if(im->width<16 || im->height<17 || (size_t)im->width*im->height>sizeof pixels ||
               ssa_sprite_image_indices(im,pixels,im->width,sizeof pixels)!=1)return -1;
            /* Match the visible17px status icon, including its original frame. */
            for(y=0;y<17;++y)for(x=0;x<16;++x){unsigned c=pixels[y*im->width+x];
                if(c)out[(top+y)*240+(unsigned)at+x]=(uint8_t)c;}
        }
        if(active)for(y=0;y<17;++y)for(x=0;x<16;++x){unsigned p=(top+y)*240+(unsigned)at+x;
            out[p]=swap[out[p]];}
    }
    return 1;
}
/* Always use retained green meters89..98, palette-swapped during the flash.
 * Green has no4px image: compose its original3px+1px pieces. Empty space
 * leaves the underlying ammo icon intact. */
static int ammo_meter(const SsaSpriteAssets *a,unsigned amount,unsigned limit,
    const uint8_t *swap,int x,int y,uint8_t *out)
{
    unsigned height;int top;
    if(!limit)return 1;
    if(amount>limit)amount=limit;
    height=(amount*11+limit-1)/limit;
    if(!height)return 1;
    top=y+13-(int)height;
    if(height==4)return image_palette(a,91,x+10,top,out,swap)==1?
        image_palette(a,89,x+10,top+3,out,swap):-1;
    return image_palette(a,(height<=3?88:87)+height,x+10,top,out,swap);
}
/* Adapt the actual shaded circle (image88), keeping its frame and expanding the
 * circular interior to match the larger UMT badge.
 * Lift its green face within the original palette for legible dark engravings.
 * Biohazard coverage comes from the public-domain standard geometry saved in
 * assets/classic-hud: 8x8 area samples per9x9 pixel, rounded to0..9. Contrast
 * hinting retains its open crescents, central hole and curved tips at HUD size.
 * These native UI variants are new artwork, not recovered ROM sprites. */
static int ammo_symbol(const SsaSpriteAssets *a,unsigned kind,int left,int top,uint8_t *out)
{
    static const char symbols[2][9][10]={
        {"000202000",
         "004000400",
         "017242710",
         "049515940",
         "356737653",
         "305161503",
         "102585201",
         "011595110",
         "013303310"},
        {"000090000",
         "000090000",
         "000090000",
         "000090000",
         "900099909",
         "000000000",
         "000000000",
         "000000000",
         "000090000"}
    };
    unsigned x,y,i,n,errors[32],black=neutral(a,0);uint8_t shades[32]={0},circle[121];
    if(image(a,88,left,top,out)!=1)return -1;
    for(y=0;y<11;++y)for(x=0;x<11;++x)
        circle[y*11+x]=out[(top+2+(int)y)*240+left+2+(int)x];
    /* Sample pixel centers so the green face grows symmetrically from9x9 to
     * 11x11, leaving the original outer frame at0/14 intact. */
    for(y=0;y<13;++y)for(x=0;x<13;++x){unsigned sx=(2*x+1)*11/26,sy=(2*y+1)*11/26;
        unsigned index=circle[sy*11+sx];
        if(index)out[(top+1+(int)y)*240+left+1+(int)x]=(uint8_t)index;}
    for(n=0;n<32;++n)errors[n]=UINT32_MAX;
    for(i=1;i<256;++i){unsigned c=a->palette[i]&0x7fff,g=(c>>5)&31;
        if(c&0x7c1f)continue;
        for(n=0;n<32;++n){unsigned d=g>n?g-n:n-g;
            if(d<errors[n]){errors[n]=d;shades[n]=(uint8_t)i;}}
    }
    for(y=2;y<13;++y)for(x=2;x<13;++x){unsigned at=(top+(int)y)*240+left+(int)x;
        unsigned index=out[at],c=a->palette[index]&0x7fff,g=(c>>5)&31;
        if(index && g && !(c&0x7c1f))out[at]=shades[(g+31)/2];}
    for(y=0;y<9;++y)for(x=0;x<9;++x){unsigned coverage=(unsigned)(symbols[kind][y][x]-'0');
        unsigned at=(top+3+(int)y)*240+left+3+(int)x,c=a->palette[out[at]]&0x7fff,g=(c>>5)&31;
        coverage=coverage>=4?9:coverage>=2?4:0;
        if(coverage && g && !(c&0x7c1f))out[at]=coverage==9?(uint8_t)black:shades[(g*(9-coverage)+4)/9];}
    return 1;
}
/* User-supplied badge, imported pixel for pixel; the meter is drawn later. */
static int tommy_ammo(const SsaSpriteAssets *a,int left,int top,uint8_t *out)
{
    unsigned x,y;
    if(!a || a->loaded!=1)return -1;
    for(y=0;y<16;++y)for(x=0;x<16;++x){unsigned index=classic_tommy_ammo_indices[y][x];
        int xx=left+(int)x,yy=top+(int)y;
        if(index && xx>=0 && xx<240 && yy>=0 && yy<160)out[yy*240+xx]=(uint8_t)index;}
    return 1;
}
int ssa_classic_hud_replaces(unsigned type)
{return type==4 || (type>=5 && type<=17);}
int ssa_classic_hud_draw(const SsaSpriteAssets *a,const SsaWeaponAnimations *anim,
    const SsaHudSpriteState *state,const SsaInventoryRules *rules,
    unsigned ammo_flash,unsigned health_flash,uint32_t high_score,uint8_t *out,uint8_t *background,size_t capacity)
{
    /* Prototype green icons have no animation bindings in the retail table.
     * These explicit associations cover all ten retail weapons. */
    static const unsigned weapons[10]={68,67,69,70,71,72,74,73,0,75};
    static const unsigned ammunition[8]={84,83,83,85,0,86,0,88};
    SsaHudSpriteState local;SsaWeaponObject digit={0};uint8_t flash_colors[256],red_colors[256],gray_colors[256];
    const uint8_t *health_colors,*armor_colors,*ammo_colors;
    unsigned i,color,health,armor,weapon,ammo,seconds,ammo_count=0,ammo_index=0;
    int weapon_x;
    if(!a || a->loaded!=1 || !anim || anim->loaded!=1 || !state || !rules || !out || capacity<38400)return -1;
    memset(out,0,38400);if(background)memset(background,0,38400);
    if(state->local_flags&8)return 1;
    local=*state;color=green(a);health=state->health>0?(unsigned)state->health:0;
    weapon=state->current_weapon>=1 && state->current_weapon<=10?state->current_weapon:1;
    if(white_palette(a,flash_colors)!=1 || alert_palette(a,red_colors,0)!=1 ||
       alert_palette(a,gray_colors,1)!=1)return -1;
    armor=state->armor>0?(unsigned)state->armor:0;local.health=(int16_t)health;
    ammo=state->ammo[weapon-1]>0?(unsigned)state->ammo[weapon-1]:0;
    /* Common/HUD.cpp: health/armor normalize to100. The selected ammo count
     * uses a15% low threshold; the little bars independently use25%.
     * Preserve Classic green outside the requested red/gray alert zones. */
    health_colors=health_flash?flash_colors:health<25?red_colors:NULL;
    armor_colors=armor<25?gray_colors:NULL;
    ammo_colors=(ammo_flash&(1u<<(weapon-1)))?flash_colors:
        rules->ammo_capacity[weapon-1] && ammo*100<rules->ammo_capacity[weapon-1]*15u?red_colors:NULL;
#define IMAGE(id,x,y) do{if(image(a,id,x,y,out)!=1)return -1;}while(0)
#define NUMBER(v,n,b,x,y,s) do{if(number(a,anim,v,n,b,x,y,s,0,NULL,out)!=1)return -1;}while(0)
    /* TFE uses max(saved record,current run). Retain the panel's horizontal
     * placement, inset it2px from above, and put the clock immediately below. */
    plate(background,2,2,59,10);IMAGE(63,2,2);NUMBER(state->score,8,0x2e,14,4,5);
    plate(background,88,2,59,10);IMAGE(64,88,2);
    bar_icon(a,out,88,2,color,0);
    NUMBER(high_score>state->score?high_score:state->score,8,0x2e,100,4,5);
    plate(background,104,13,26,7);seconds=state->time/60;
    NUMBER(seconds/60%100,2,0x2e,105,14,5);
    if(number(a,anim,seconds%60,2,0x2e,120,14,5,1,NULL,out)!=1)return -1;
    fill(out,117,15,1,1,color);fill(out,117,17,1,1,color);
    /* Bottom status: armor/health, selected weapon/ammo, eight finite ammo
     * slots in the original weapon order3..10. Their pools are independent,
     * even when older artwork shares a bullet icon. */
    if(armor){plate(background,2,121,45,17);IMAGE(80,2,121);IMAGE(82,18,120);
        if(number(a,anim,armor,3,0x24,21,123,8,0,armor_colors,out)!=1)return -1;}
    plate(background,2,141,45,17);
    IMAGE(81,2,141);IMAGE(82,18,140);
    digit.type=5;
    for(i=0;i<3;++i){digit.subtype=(uint16_t)i;
        if((i==0 && health%1000<100) || (i==1 && health%1000<10))continue;
        if(ssa_hud_sprite_step(&digit,&local)!=1)return -1;
        if(image_palette(a,frame(anim,digit.animation),21+(int)i*8,143,out,health_colors)!=1)return -1;}
    for(i=2;i<10;++i)if(state->ammo[i]>0)++ammo_count;
    /* Reference anchors175/640 for finite weapons,205/640 for infinite.
     * Eight full-size GBA ammo badges need8px more clearance than the
     * reference's scaled badges; shift only the finite weapon group to fit. */
    weapon_x=weapon>2?66:77;
    if(weapon>2 && ammo_count==8)weapon_x=58;
    plate(background,weapon_x,141,16,17);
    if(weapons[weapon-1]){IMAGE(weapons[weapon-1],weapon_x,141);}
    else if(time_strike_icon(a,weapon_x,141,out)!=1)return -1;
    if(weapon>2){plate(background,weapon_x+18,141,29,17);IMAGE(82,weapon_x+18,140);
        if(number(a,anim,ammo,3,0x24,weapon_x+21,143,8,0,ammo_colors,out)!=1)return -1;}
    for(i=0;i<8;++i){unsigned amount=state->ammo[i+2]>0?(unsigned)state->ammo[i+2]:0;
        int x=239-(int)ammo_count*16+(int)ammo_index*16;
        if(amount){
            ++ammo_index;
            plate(background,x,142,15,16);
            if(i==1){if(tommy_ammo(a,x,142,out)!=1)return -1;}
            else if(ammunition[i]){IMAGE(ammunition[i],x,142);}
            else if(ammo_symbol(a,i==4?0:1,x,142,out)!=1)return -1;
            /* Highlight the selected ammo art inside the green frame. Draw
             * the meter afterward so its separate consumption cue remains. */
            if(weapon==i+3){unsigned xx,yy;
                for(yy=1;yy<14;++yy)for(xx=1;xx<14;++xx){unsigned at=(142+yy)*240+(unsigned)x+xx;
                    out[at]=flash_colors[out[at]];}}
            if(ammo_meter(a,amount,rules->ammo_capacity[i+2],(ammo_flash&(1u<<(i+2)))?flash_colors:
                rules->ammo_capacity[i+2] && amount*4<rules->ammo_capacity[i+2]?red_colors:NULL,x,142,out)!=1)return -1;
        }
    }
    if(state->boss_present && state->boss_health>0){unsigned percent=(unsigned)state->boss_health/30;
        plate(background,91,26,59,10);
        IMAGE(64,91,26);bar_icon(a,out,91,26,color,1);
        fill(out,101,29,(percent>100?100:percent)*47/100,4,color);}
#undef IMAGE
#undef NUMBER
    return 1;
}
