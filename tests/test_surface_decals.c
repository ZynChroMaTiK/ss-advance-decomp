#include "ssa/native_game.h"
#include "ssa/pc_renderer.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,draws,changed_pixels;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"decals line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void word(FILE *f,uint32_t n,unsigned bytes){while(bytes--){fputc(n&255,f);n>>=8;}}
static void snapshot(const char *dir,const char *name,unsigned level,const uint32_t *pixels,unsigned w,unsigned h){
    char path[1024];FILE *f;unsigned i;if(!dir)return;
    CHECK(snprintf(path,sizeof path,"%s/%s-%u.bmp",dir,name,level)<(int)sizeof path);
    f=fopen(path,"wb");CHECK(f);fputs("BM",f);word(f,54+w*h*4,4);word(f,0,4);word(f,54,4);word(f,40,4);
    word(f,w,4);word(f,0u-h,4);word(f,1,2);word(f,32,2);word(f,0,4);word(f,w*h*4,4);
    for(i=0;i<4;++i)word(f,0,4);
    CHECK(fwrite(pixels,4,(size_t)w*h,f)==(size_t)w*h && !fclose(f));
}
static void water_blend_checks(void){
    SsaWaterRender *r=calloc(1,sizeof(*r));uint16_t palette[256]={0};unsigned base,env;
    CHECK(r);r->palette=palette;
    for(base=0;base<32;++base){palette[1]=(uint16_t)(base|(base<<5)|(base<<10));
        {unsigned half=(base+1)/2;
            CHECK(ssa_water_blend(r,UINT32_C(0xff010000),0)==(half|(half<<5)|(half<<10)));}
        CHECK(ssa_water_blend(r,UINT32_C(0x00010000),0x7fff)==palette[1]);
        for(env=0;env<32;++env){unsigned channel;uint16_t result=ssa_water_blend(r,UINT32_C(0xff010000),(uint16_t)(env|(env<<5)|(env<<10)));
            for(channel=0;channel<15;channel+=5){unsigned value=(result>>channel)&31;
                CHECK(value==(base+env+1)/2);}
            CHECK(!(result&0x8000));
        }
    }
    /* Sparse GBA palettes retain weak highlights through stable two-color
     * dithering, while exact palette colors remain completely unchanged. */
    memset(palette,0,sizeof palette);palette[2]=0x7fff;
    memset(r->valid,0,sizeof r->valid);
    {unsigned x,y,sum=0;
        for(y=0;y<4;++y)for(x=0;x<4;++x){unsigned index=ssa_water_reflection_index(r,8|(8<<5)|(8<<10),x,y);
            CHECK(index && index<256);sum+=palette[index]&31;
            CHECK(ssa_water_reflection_index(r,0,x,y)==1);
            CHECK(ssa_water_reflection_index(r,0x7fff,x,y)==2);
        }
        CHECK(abs((int)sum-8*16)<=16);
    }
    free(r);
}
/* Asset coverage only: inspect campaign materials without running levels. */
static void materials(const uint8_t *rom){
    SsaSurfaceDecals *d=calloc(1,sizeof(*d));SsaPaletteEffect palette={0};SsaMapAssets map={0};
    unsigned family,surface,level,i,c,floors=0,ceilings=0,water=0;double wx[2],wy[2];
    CHECK(d);
    /* Live instances share only immutable texels; reinitializing or freeing
     * one level must not invalidate another level's atlas or palette. */
    {SsaSurfaceDecals *other=calloc(1,sizeof(*other));uint16_t colors[256]={0};
        const SsaSurfaceDecalAtlas *atlas;uint8_t tiles[sizeof d->tiles];
        CHECK(other && ssa_surface_decals_init(d,colors,0)==1);
        atlas=d->atlas;memcpy(tiles,d->tiles,sizeof tiles);
        CHECK(ssa_surface_decals_init(other,colors,6)==1 && other->atlas==atlas);
        CHECK(ssa_surface_decals_init(other,colors,3)==1 && other->atlas==atlas);
        free(other);CHECK(d->atlas==atlas && !memcmp(tiles,d->tiles,sizeof tiles));
        CHECK(sizeof(*d)<32*1024);
    }
    for(family=0;family<2;++family){
        CHECK(ssa_palette_effect_init(&palette,rom,0x800000,family*6)==1);
        CHECK(ssa_surface_decals_init(d,palette.colors,family*6)==1);
        for(surface=0;surface<2;++surface){unsigned cell,texel,bank,tile;
            for(cell=0;cell<4096;++cell)if(d->tiles[surface][cell] && d->tiles[surface][cell]<SSA_SURFACE_STAIN_TILES)break;
            CHECK(cell<4096);bank=d->variations[surface][cell];tile=d->tiles[surface][cell];
            for(texel=0;texel<128*128;++texel)if(d->atlas->masks[surface][bank][tile][texel])break;
            CHECK(texel<128*128);wx[surface]=(cell%64*128+texel%128)*2.0;wy[surface]=(cell/64*128+texel/128)*2.0;
        }
        CHECK(d->water_floor[20] && (family?!d->water_floor[130]:d->water_floor[19]));
        CHECK(!d->water_floor[33] && !d->water_floor[family?222:120]);
        for(c=1;c<256;++c){
            CHECK(d->shades[c] && (palette.colors[d->shades[c]]&0x7fff)!=(palette.colors[c]&0x7fff));
            CHECK(ssa_surface_decals_sample(d,0,(uint8_t)c,wx[0],wy[0],100)==(d->water_floor[c]?c:d->shades[c]));
            CHECK(ssa_surface_decals_sample(d,1,(uint8_t)c,wx[1],wy[1],100)==d->shades[c]);
        }
        for(level=family*6;level<family*6+6;++level){
            CHECK(ssa_map_assets_load(&map,rom,0x800000,level)==1);
            if(level==6){const uint8_t *s=map.data+map.sectors+21*20;
                CHECK(s[10]==130 && !s[0] && !s[1] && !ssa_surface_decals_water(d,130,0));}
            for(i=0;i<map.sector_count;++i)for(surface=0;surface<2;++surface){
                c=map.data[map.sectors+i*20+10+surface];
                if(!c){CHECK(!ssa_surface_decals_sample(d,surface,0,wx[surface],wy[surface],100));continue;}
                if(!surface && d->water_floor[c]){++water;
                    CHECK(ssa_surface_decals_sample(d,0,(uint8_t)c,wx[0],wy[0],100)==c);
                }else{if(surface)++ceilings;else ++floors;
                    CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,wx[surface],wy[surface],100)==d->shades[c]);}
            }
            ssa_map_assets_free(&map);
        }
    }
    CHECK(floors>100 && ceilings>100 && water>0);free(d);
}
static void water_checks(SsaNativeGame *g,SsaPcRenderer *r,const char *dir){
    unsigned water=1,x,y,f,pitch,found=0,changes=0;double gx=0,gy=0;uint32_t stamp=0;
    uint8_t *floor_colors=malloc(g->map.sector_count);uint32_t *base=malloc(480*320*4);float *depth=malloc(480*320*sizeof(float));
    uint32_t counter=g->frame_game.counter;SsaInventory inventory=g->players[0].inventory;
    CHECK(floor_colors && base && depth && !g->water_reflections);
    while(water<256 && !g->surface_decals.water_floor[water])++water;
    CHECK(water<256);
    /* Find an actual animated sprite pixel and prove it bypasses blending. */
    for(f=0;f<128 && !found;f+=3)for(y=0;y<128 && !found;++y)for(x=0;x<128;++x)
        if(ssa_water_glint(&g->surface_decals,x*2.0,y*2.0,100,f)){gx=x*2.0;gy=y*2.0;stamp=f;found=1;break;}
    CHECK(found && !ssa_water_glint(&g->surface_decals,gx,gy,1536,stamp));
    CHECK(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,0,0,64,stamp,1,1,1,1)==1);
    {uint16_t light=ssa_water_rgb(&r->water,(uint8_t)water,gx,gy,0,100,0);
        CHECK(light==(g->palette_effect.colors[g->water_scene.glints[water]]&0x7fff));
        CHECK(ssa_water_rgb(&r->water,(uint8_t)water,gx,gy,0,100,0)==light);
        CHECK(!r->water.mask[0] && !r->water.passes);}
    for(f=0;f<128;++f)changes+=!ssa_water_glint(&g->surface_decals,gx,gy,100,f);
    CHECK(changes>50);
    /* Non-glint decals keep water-colored shading, animate independently and
     * bypass reflection blending just like the separate bright glints. */
    {unsigned got=0,off=0;double rx=0,ry=0;uint32_t tick=0;uint16_t shade;
        for(f=0;f<256 && !got;f+=12)for(y=0;y<128 && !got;++y)for(x=0;x<128;++x)
            if(ssa_water_ripple(&g->surface_decals,x*2.0,y*2.0,100,f) &&
               !ssa_water_glint(&g->surface_decals,x*2.0,y*2.0,100,f)){rx=x*2.0;ry=y*2.0;tick=f;got=1;break;}
        CHECK(got && !ssa_water_ripple(&g->surface_decals,rx,ry,1536,tick));
        CHECK(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,0,0,64,tick,1,1,1,1)==1);
        shade=ssa_water_rgb(&r->water,(uint8_t)water,rx,ry,0,100,0);
        CHECK(shade==(g->palette_effect.colors[g->water_scene.ripples[water]]&0x7fff) &&
            shade!=(g->palette_effect.colors[water]&0x7fff) && !r->water.mask[0]);
        CHECK(shade!=(g->palette_effect.colors[g->water_scene.glints[water]]&0x7fff));
        CHECK(ssa_water_rgb(&r->water,(uint8_t)water,rx,ry,0,100,0)==shade);
        r->water.animated_decals=0;
        CHECK(ssa_water_rgb(&r->water,(uint8_t)water,rx,ry,0,100,0)==(g->palette_effect.colors[water]&0x7fff) && (r->water.mask[0]>>24));
        for(f=0;f<256;++f)off+=!ssa_water_ripple(&g->surface_decals,rx,ry,100,f);
        CHECK(off>180 && off<256);
        CHECK(!ssa_water_ripple(NULL,0,0,0,0) && !ssa_water_ripple(&g->surface_decals,65536,0,0,0));
        CHECK(ssa_water_ripple(&g->surface_decals,-127,-253,100,UINT32_MAX)==ssa_water_ripple(&g->surface_decals,-127,-253,100,UINT32_MAX));
    }
    /* Two visible water heights require two real scene passes. The isolated
     * stencil also checks zero-work frames and glint/dry-pixel exclusion. */
    {SsaWaterRender *w=calloc(1,sizeof(*w));SsaPcRenderer *mirror=NULL;uint32_t out[4]={0x123456,0x654321,0xabcdef,0xfedcba};
        CHECK(w && ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
            0,0,32767,0,0,1,2,2)==1);
        CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,out,NULL)==1 && !mirror && !w->passes);
        CHECK(ssa_water_rgb(w,(uint8_t)water,0,0,-16,100,0)==(g->palette_effect.colors[water]&0x7fff));
        CHECK(ssa_water_rgb(w,(uint8_t)water,0,0,16,100,1)==(g->palette_effect.colors[water]&0x7fff));
        CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,out,NULL)==1 && mirror && w->passes==2 && !mirror->rays);
        CHECK(mirror->width==2 && mirror->height==2);
        CHECK(out[2]==0xabcdef && out[3]==0xfedcba);
        {uint32_t odd[15];unsigned p;
            for(p=0;p<15;++p)odd[p]=0x123456;
            CHECK(ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
                0,0,32767,0,0,1,3,5)==1);
            (void)ssa_water_rgb(w,(uint8_t)water,0,0,16,100,0);
            (void)ssa_water_rgb(w,(uint8_t)water,0,0,16,100,14);
            CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,odd,NULL)==1 && w->passes==1);
            CHECK(mirror->width==3 && mirror->height==5);
            for(p=1;p<14;++p)CHECK(odd[p]==0x123456);
        }
        ssa_pc_renderer_free(mirror);free(mirror);ssa_water_free(w);free(w);}
    /* Reflection allocation follows native viewport growth and shrinkage.
     * Returning to the original size must reproduce the same scene. */
    {SsaWaterRender *w=calloc(1,sizeof(*w));SsaPcRenderer *mirror=NULL;
        uint32_t *out=malloc(640*320*4),*reference=malloc(240*160*4);unsigned view,p;
        static const unsigned sizes[4][2]={{240,160},{480,320},{640,320},{240,160}};
        CHECK(w && out && reference);
        for(view=0;view<4;++view){
            CHECK(ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
                0,0,32767,0,0,1,sizes[view][0],sizes[view][1])==1);
            for(p=0;p<sizes[view][0]*sizes[view][1];++p)w->mask[p]=UINT32_C(0xff000000)|(water<<16);
            CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,0,w,out,NULL)==1);
            CHECK(mirror->width==sizes[view][0] && mirror->height==sizes[view][1] && w->passes==1);
            if(!view)memcpy(reference,mirror->pixels,240*160*4);
            else if(view==3)CHECK(!memcmp(reference,mirror->pixels,240*160*4));
        }
        ssa_pc_renderer_free(mirror);free(mirror);ssa_water_free(w);free(w);free(out);free(reference);}
    /* Render-only water material fixture. Geometry, actors and game state stay
     * unchanged; replacing floor colors makes both Egypt/Rome views inspectable. */
    for(x=0;x<g->map.sector_count;++x){uint8_t *s=g->map.data+g->map.sectors+x*20;
        floor_colors[x]=s[10];s[10]=(uint8_t)water;}
    for(pitch=0;pitch<3;++pitch){unsigned changed=0;
        g->player.pitch=pitch==1?6000:pitch==2?-6000:0;g->surface_decals_enabled=g->water_reflections=0;
        CHECK(ssa_native_game_prepare_view(g)==1 && ssa_pc_renderer_resize(r,480,320)==1 && ssa_pc_renderer_draw(r,g)==1);
        memcpy(base,r->pixels,480*320*4);memcpy(depth,r->depth,480*320*sizeof(float));
        g->water_reflections=1;CHECK(ssa_pc_renderer_draw(r,g)==1 && !memcmp(depth,r->depth,480*320*sizeof(float)));
        for(x=0;x<480*320;++x){uint32_t a=base[x],b=r->pixels[x];unsigned shift;changed+=a!=b;
            for(shift=0;shift<24;shift+=8){unsigned before=(a>>shift)&255,after=(b>>shift)&255;
                CHECK(abs((int)after-(int)before)<=132);}}
        if(pitch!=1)CHECK(changed>100 && r->water.passes>0 && r->reflection && !r->reflection->rays);
        if(r->water.passes)CHECK(r->reflection->width==480 && r->reflection->height==320);
        if(g->level==6 && !pitch)CHECK(r->water.actor_pixels>0);
        memcpy(base,r->pixels,480*320*4);CHECK(ssa_pc_renderer_draw(r,g)==1 && !memcmp(base,r->pixels,480*320*4));
        if(!pitch)snapshot(dir,"water-reflections",g->level,r->pixels,480,320);
        g->surface_decals_enabled=1;g->frame_game.counter=counter+stamp+8;
        CHECK(ssa_pc_renderer_draw(r,g)==1 && !memcmp(depth,r->depth,480*320*sizeof(float)));
        if(pitch==2){unsigned animated=0;
            snapshot(dir,"water-glints",g->level,r->pixels,480,320);
            memcpy(base,r->pixels,480*320*4);
            CHECK(ssa_pc_renderer_draw(r,g)==1 && !memcmp(base,r->pixels,480*320*4));
            g->frame_game.counter+=12;CHECK(ssa_pc_renderer_draw(r,g)==1 && !memcmp(depth,r->depth,480*320*sizeof(float)));
            for(x=0;x<480*320;++x)animated+=base[x]!=r->pixels[x];
            CHECK(animated>0);snapshot(dir,"water-ripples",g->level,r->pixels,480,320);draws+=2;
        }
        CHECK(!memcmp(&inventory,&g->players[0].inventory,sizeof inventory));draws+=4;
    }
    g->frame_game.counter=counter;g->player.pitch=0;g->surface_decals_enabled=0;g->water_reflections=0;
    CHECK(ssa_native_game_draw(g)==1);memcpy(base,g->rgb888,38400*4);
    g->water_reflections=1;CHECK(ssa_native_game_draw(g)==1);changes=0;
    for(x=0;x<38400;++x)changes+=base[x]!=g->rgb888[x];
    CHECK(changes>10 && g->water_render.passes>0 && g->water_renderer && !g->water_renderer->rays);
    CHECK(g->water_renderer->width==240 && g->water_renderer->height==160);
    snapshot(dir,"water-gba",g->level,g->rgb888,240,160);draws+=2;
    if(g->level==6){SsaActorSource sources[100];unsigned changed=0;
        /* Removing render snapshots, without touching world entities, must
         * remove actual reflected sprite pixels from the second scene pass. */
        g->surface_decals_enabled=0;CHECK(ssa_pc_renderer_draw(r,g)==1);
        memset(base,0,480*320*4);
        CHECK(ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,&r->water,base,NULL)==1 && r->water.actor_pixels>0);
        memcpy(r->pixels,base,480*320*4);memcpy(sources,g->sources,sizeof sources);
        for(x=0;x<g->spawn.pool.active_count;++x)g->sources[x].entity=NULL;
        CHECK(ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,&r->water,base,NULL)==1);
        for(x=0;x<480*320;++x)changed+=base[x]!=r->pixels[x];
        CHECK(changed>10);memcpy(g->sources,sources,sizeof sources);++draws;
        /* The mirrored camera must use the same live FOV as the main scene. */
        memset(base,0,480*320*4);
        CHECK(ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,&r->water,base,NULL)==1);
        memcpy(r->pixels,base,480*320*4);
        for(unsigned offset=0;offset<=60;offset+=60){g->fov_offset=offset;changed=0;
            memset(base,0,480*320*4);
            CHECK(ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,&r->water,base,NULL)==1);
            for(x=0;x<480*320;++x)changed+=base[x]!=r->pixels[x];
            CHECK(changed>10);}
        g->fov_offset=10;
        memset(base,0,480*320*4);
        CHECK(ssa_pc_renderer_reflect(&r->reflection,g,&g->pc_camera,g->render_pitch,&r->water,base,NULL)==1);
        CHECK(!memcmp(r->pixels,base,480*320*4));
        /* Original/GBA projection and its reflection ignore the saved FOV. */
        g->gba_view=1;CHECK(ssa_native_game_draw(g)==1);memcpy(base,g->rgb888,38400*4);
        g->fov_offset=60;CHECK(ssa_native_game_draw(g)==1 && !memcmp(base,g->rgb888,38400*4));
        g->fov_offset=10;g->gba_view=0;
    }
    for(x=0;x<g->map.sector_count;++x)g->map.data[g->map.sectors+x*20+10]=floor_colors[x];
    g->water_reflections=g->surface_decals_enabled=0;
    CHECK(g->frame_game.counter==counter && !memcmp(&inventory,&g->players[0].inventory,sizeof inventory));
    free(floor_colors);free(base);free(depth);
}
static void masks(SsaNativeGame *g,const char *dir){
    static uint64_t library_signature;
    SsaSurfaceDecals *d=&g->surface_decals;SsaPlanePool pool={0};SsaRenderCamera camera={0};
    uint8_t fb[14402];unsigned x,y,c=1,floor=0,ceiling=0,different=0;uint32_t atlas[256*128];
    unsigned surface,bank,variant,edge_stains=0;enum { S=SSA_SURFACE_STAIN_SIZE };
    for(c=1;c<256 && (d->shades[c]==c || d->water_floor[c]);++c){}
    CHECK(c<256 && d->loaded && S==128 && SSA_SURFACE_STAIN_WORLD==256);
    {const uint8_t *bytes=(const uint8_t *)d->atlas->masks;size_t i;uint64_t signature=UINT64_C(14695981039346656037);
        for(i=0;i<sizeof d->atlas->masks;++i)signature=(signature^bytes[i])*UINT64_C(1099511628211);
        if(library_signature)CHECK(signature==library_signature);
        library_signature=signature;}
    CHECK(d->connections[SSA_SURFACE_STAIN_FULL]==15 && d->corners[SSA_SURFACE_STAIN_FULL]==15);
    for(surface=0;surface<2;++surface)for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank){
        const uint16_t *t=d->atlas->thresholds[surface][bank];unsigned buckets[16]={0},transitions=0,pairs=0;
        for(y=0;y<S;++y)for(x=0;x<S;++x){unsigned a=t[y*S+x]<32768;
            ++buckets[t[y*S+x]>>12];
            if(x){++pairs;transitions+=a!=(t[y*S+x-1]<32768);}
            if(y){++pairs;transitions+=a!=(t[(y-1)*S+x]<32768);}}
        for(x=0;x<16;++x)CHECK(buckets[x]>800 && buckets[x]<1250);
        /* Random speckles alternate about half the time; an ordered checker
         * lattice has excessive neighbor alternation even with balanced bins. */
        CHECK(transitions>pairs*45/100 && transitions<pairs*55/100);
    }
    for(surface=0;surface<2;++surface)for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank)
        for(variant=0;variant<SSA_SURFACE_DECAL_TILES;++variant){
        const uint8_t *m=d->atlas->masks[surface][bank][variant];unsigned links=d->connections[variant],corners=d->corners[variant];
        if(!(corners&SSA_SURFACE_STAIN_NW))CHECK(!m[0]);
        if(!(corners&SSA_SURFACE_STAIN_NE))CHECK(!m[S-1]);
        if(!(corners&SSA_SURFACE_STAIN_SW))CHECK(!m[(S-1)*S]);
        if(!(corners&SSA_SURFACE_STAIN_SE))CHECK(!m[S*S-1]);
        if(corners){unsigned base,i;const uint8_t *unfilled;
            for(base=1;base<SSA_SURFACE_STAIN_TILES;++base)
                if(d->connections[base]==links && !d->corners[base])break;
            CHECK(base<SSA_SURFACE_STAIN_TILES);unfilled=d->atlas->masks[surface][bank][base];
            /* Diagonal support must not paint rectangular quadrants across
             * either central seam of an otherwise identical tile piece. */
            for(i=0;i<S;++i){
                CHECK(m[i*S+63]==unfilled[i*S+63] && m[i*S+64]==unfilled[i*S+64]);
                CHECK(m[63*S+i]==unfilled[63*S+i] && m[64*S+i]==unfilled[64*S+i]);}}
        for(y=0;y<S;++y)for(x=0;x<S;++x){unsigned a=m[y*S+x];
            CHECK(a==0 || a==255);
            if(!variant)CHECK(!a);
            if(!x){if(!(links&SSA_SURFACE_STAIN_WEST))CHECK(!a);edge_stains+=a!=0;}
            if(x==S-1 && !(links&SSA_SURFACE_STAIN_EAST))CHECK(!a);
            if(!y && !(links&SSA_SURFACE_STAIN_NORTH))CHECK(!a);
            if(y==S-1 && !(links&SSA_SURFACE_STAIN_SOUTH))CHECK(!a);
        }
    }
    CHECK(edge_stains>100);
    for(surface=0;surface<2;++surface)for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank){unsigned stained=0,inner=0,outer=0,ni=0,no=0,differences=0;
        const uint8_t *full=d->atlas->masks[surface][bank][SSA_SURFACE_STAIN_FULL],*single=d->atlas->masks[surface][bank][1];
        for(y=0;y<S;++y)for(x=0;x<S;++x){int dx=(int)x*2-127,dy=(int)y*2-127,rr=dx*dx+dy*dy;
            stained+=full[y*S+x]!=0;
            differences+=full[y*S+x]!=d->atlas->masks[surface][0][SSA_SURFACE_STAIN_FULL][y*S+x];
            if(rr>=112*112)CHECK(!single[y*S+x]); /* Rounded, inset exposed corners. */
            if(rr<70*70){++ni;inner+=single[y*S+x]!=0;}
            if(rr>=75*75 && rr<105*105){++no;outer+=single[y*S+x]!=0;}}
        CHECK(stained>S*S/3 && stained<S*S*4/5);
        if(bank)CHECK(differences>S*S/10);
        CHECK(outer>20 && inner*no>outer*ni*2);
        /* Even the full connecting tile must reveal the floor/ceiling within
         * every small patch; broad shapes must not become flat filled panels. */
        for(y=0;y<S;y+=8)for(x=0;x<S;x+=8){unsigned xx,yy,count=0;
            for(yy=y;yy<y+8;++yy)for(xx=x;xx<x+8;++xx)count+=full[yy*S+xx]!=0;
            CHECK(count<64);}
    }
    for(surface=0;surface<2;++surface){unsigned empty=0,single=0,corners=0,edges=0,full=0,noisy=0,banks[SSA_SURFACE_DECAL_VARIATIONS]={0};
        for(y=0;y<64;++y)for(x=0;x<64;++x){unsigned tile=d->tiles[surface][y*64+x],links=d->connections[tile],i;
            unsigned right=d->tiles[surface][y*64+((x+1)&63)],down=d->tiles[surface][((y+1)&63)*64+x];
            CHECK(tile<SSA_SURFACE_DECAL_TILES);
            CHECK(d->variations[surface][y*64+x]<SSA_SURFACE_DECAL_VARIATIONS);
            ++banks[d->variations[surface][y*64+x]];
            CHECK(!!(links&SSA_SURFACE_STAIN_EAST)==!!(d->connections[right]&SSA_SURFACE_STAIN_WEST));
            CHECK(!!(links&SSA_SURFACE_STAIN_SOUTH)==!!(d->connections[down]&SSA_SURFACE_STAIN_NORTH));
            /* Every bank can meet every other bank without a changed port. */
            for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank)for(i=0;i<S;++i){
                CHECK(d->atlas->masks[surface][bank][tile][i*S+S-1]==d->atlas->masks[surface][0][right][i*S]);
                CHECK(d->atlas->masks[surface][bank][tile][(S-1)*S+i]==d->atlas->masks[surface][0][down][i]);
                CHECK(d->atlas->masks[surface][bank][tile][i*S]==d->atlas->masks[surface][0][tile][i*S]);
                CHECK(d->atlas->masks[surface][bank][tile][i]==d->atlas->masks[surface][0][tile][i]);}
            empty+=!tile || tile==SSA_SURFACE_NOISE_TILE;noisy+=tile==SSA_SURFACE_NOISE_TILE;
            single+=tile==1;corners+=links==3 || links==6 || links==9 || links==12;
            edges+=links==7 || links==11 || links==13 || links==14;full+=tile==SSA_SURFACE_STAIN_FULL;
        }
        CHECK(empty>64*64/2 && empty<64*64*4/5 && single>5 && corners>5 && edges>5 && full>5);
        {unsigned bx,by,empty_blocks=0;
            for(by=0;by<16;++by)for(bx=0;bx<16;++bx){unsigned xx,yy,count=0;
                for(yy=0;yy<4;++yy)for(xx=0;xx<4;++xx){unsigned t=d->tiles[surface][(by*4+yy)*64+bx*4+xx];
                    count+=t!=0 && t!=SSA_SURFACE_NOISE_TILE;}
                empty_blocks+=!count;}
            CHECK(surface?(empty_blocks>0 && empty_blocks<32):empty_blocks==0);}
        CHECK(noisy>empty/8 && noisy<empty/3);
        for(bank=0;bank<SSA_SURFACE_DECAL_VARIATIONS;++bank)CHECK(banks[bank]>800 && banks[bank]<1250);
    }
    for(x=1;x<256;++x)CHECK(d->shades[x]!=0);
    for(surface=0;surface<2;++surface){unsigned cell,stain=0,far=0,solid=0;
        for(cell=0;cell<64*64;++cell)if(!d->tiles[surface][cell])break;
        CHECK(cell<64*64);
        for(y=0;y<S;++y)for(x=0;x<S;++x){
            double wx=(cell%64)*SSA_SURFACE_STAIN_WORLD+x*2.0,wy=(cell/64)*SSA_SURFACE_STAIN_WORLD+y*2.0;
            CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,100)==c);
            CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,1300)==c);
            CHECK(!ssa_surface_decals_texel(d,surface,(int32_t)(wx/2),(int32_t)(wy/2)));
        }
        for(cell=0;cell<64*64;++cell)if(d->tiles[surface][cell]==SSA_SURFACE_STAIN_FULL)break;
        CHECK(cell<64*64);
        for(y=0;y<S;++y)for(x=0;x<S;++x){
            double wx=(cell%64)*SSA_SURFACE_STAIN_WORLD+x*2.0,wy=(cell/64)*SSA_SURFACE_STAIN_WORLD+y*2.0;
            unsigned density=ssa_surface_decals_texel(d,surface,(int32_t)(wx/2),(int32_t)(wy/2));
            uint8_t near_color=ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,100);
            uint8_t far_color=ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,1300);
            stain+=near_color!=c;far+=far_color!=c;
            if(density>=48){CHECK(near_color!=c);++solid;}
            if(!density)CHECK(near_color==c);
            CHECK(near_color!=c || far_color==c);
            CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,1536)==c);
        }
        CHECK(solid>S*S/3 && solid<S*S*4/5 && stain==solid && far<stain);
        for(cell=0;cell<64*64;++cell)if(d->tiles[surface][cell]==SSA_SURFACE_NOISE_TILE)break;
        CHECK(cell<64*64);stain=0;far=0;
        for(y=0;y<S;++y)for(x=0;x<S;++x){
            double wx=(cell%64)*SSA_SURFACE_STAIN_WORLD+x*2.0,wy=(cell/64)*SSA_SURFACE_STAIN_WORLD+y*2.0;
            uint8_t a=ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,100);
            uint8_t b=ssa_surface_decals_sample(d,surface,(uint8_t)c,wx,wy,1300);
            CHECK(a==c || a==d->shades[c]);CHECK(a!=c || b==c);
            stain+=a!=c;far+=b!=c;
            if(!x || !y || x==S-1 || y==S-1)CHECK(a==c);
        }
        CHECK(stain>30 && stain<S*S/32 && far<stain);
    }
    for(surface=0;surface<2;++surface)for(y=0;y<S;++y)for(x=0;x<9;++x){
        double edge=((int)x-4)*SSA_SURFACE_STAIN_WORLD,along=y*2.0;
        CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,edge-0.25,along,100)==
              ssa_surface_decals_sample(d,surface,(uint8_t)c,edge-0.25+16384,along,100));
        CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,along,edge-0.25,100)==
              ssa_surface_decals_sample(d,surface,(uint8_t)c,along,edge-0.25+16384,100));
    }
    /* World sampling uses exactly the selected tile's stored two-color pixel,
     * without generated/warped coverage, across the whole wrapping layout. */
    for(surface=0;surface<2;++surface)for(y=0;y<512;++y)for(x=0;x<512;++x){
        int32_t tx=(int32_t)x*16-4096,ty=(int32_t)y*16-4096;
        unsigned cell=(((unsigned)ty>>7)&63)*64+(((unsigned)tx>>7)&63),tile=d->tiles[surface][cell];
        unsigned a=ssa_surface_decals_texel(d,surface,tx,ty);
        CHECK(a==d->atlas->masks[surface][d->variations[surface][cell]][tile][((unsigned)ty&127)*S+((unsigned)tx&127)]);
        CHECK(a==ssa_surface_decals_texel(d,surface,tx+8192,ty-8192));
        CHECK(ssa_surface_decals_sample(d,surface,(uint8_t)c,tx*2.0,ty*2.0,100)==(a?d->shades[c]:c));
    }
    CHECK(ssa_surface_decals_texel(d,0,INT32_MIN,INT32_MAX)==ssa_surface_decals_texel(d,0,0,8191));
    CHECK(!ssa_surface_decals_texel(NULL,0,0,0) && !ssa_surface_decals_texel(d,2,0,0));
    for(y=0;y<256;++y)for(x=0;x<256;++x){double wx=(int)x*8-1024,wy=(int)y*8-1024;
        uint8_t a=ssa_surface_decals_sample(d,0,(uint8_t)c,wx,wy,100),b=ssa_surface_decals_sample(d,1,(uint8_t)c,wx,wy,100);
        floor+=a!=c;ceiling+=b!=c;different+=a!=b;
        CHECK(a==ssa_surface_decals_sample(d,0,(uint8_t)c,wx+0.75,wy+0.75,100));
        CHECK(ssa_surface_decals_sample(d,0,(uint8_t)c,wx,wy,2000)==c);
        CHECK(ssa_surface_decals_sample(d,1,0,wx,wy,0)==0);
    }
    CHECK(floor>100 && ceiling>100 && different>100);
    pool.count=pool.capacity=2;camera.z=64*65536;camera.cosine=65536;
    /* A sparse layout may legitimately leave the initial view clear. Place
     * this synthetic visibility fixture beside a stain on both surfaces. */
    for(x=0;x<64*64;++x)if(d->tiles[0][x] && d->tiles[0][x]<SSA_SURFACE_STAIN_TILES &&
        d->tiles[1][x] && d->tiles[1][x]<SSA_SURFACE_STAIN_TILES)break;
    CHECK(x<64*64);camera.x=(int32_t)((x%64)*SSA_SURFACE_STAIN_WORLD+128)*65536;
    camera.y=((int32_t)(x/64)*SSA_SURFACE_STAIN_WORLD)*65536;
    for(x=0;x<2;++x){SsaPlane *p=&pool.records[x];p->height=x?128:0;p->color=(uint16_t)c;p->min_x=5;p->max_x=114;
        memset(p->top,255,sizeof p->top);
        for(y=5;y<=114;++y){p->top[y+1]=(uint8_t)(x?0:70);p->bottom[y+1]=(uint8_t)(x?49:119);}}
    memset(fb,c,sizeof fb);fb[0]=0x12;fb[14401]=0x34;
    CHECK(ssa_surface_decals_reference(d,&pool,&camera,fb+1,14400)==1 && fb[0]==0x12 && fb[14401]==0x34);
    floor=ceiling=0;
    for(y=0;y<120;++y)for(x=0;x<120;++x){unsigned value=fb[1+y*120+x];
        if(x<5 || x>114 || (y>=50 && y<70))CHECK(value==c);
        else if(value!=c){if(y<50)++ceiling;else ++floor;}}
    CHECK(floor && ceiling);
    /* Blue material is excluded on floors, while the identical palette color
     * remains eligible on ceilings in the reference rendering path. */
    for(x=1;x<256 && !d->water_floor[x];++x){}
    CHECK(x<256);pool.records[0].color=pool.records[1].color=(uint16_t)x;
    memset(fb+1,(int)x,14400);ceiling=0;
    CHECK(ssa_surface_decals_reference(d,&pool,&camera,fb+1,14400)==1);
    for(y=0;y<14400;++y){if(y/120>=70)CHECK(fb[1+y]==x);else ceiling+=fb[1+y]!=x;}
    CHECK(ceiling>0);pool.records[0].color=pool.records[1].color=(uint16_t)c;
    CHECK(ssa_surface_decals_reference(d,&pool,&camera,fb,14399)==-1);
    for(y=0;y<128;++y)for(x=0;x<256;++x){unsigned surface=x>=128,v=ssa_surface_decals_sample(d,surface,(uint8_t)c,(x%128)*4,y*4,100);
        unsigned rgb=g->palette_effect.colors[v],r=rgb&31,gg=(rgb>>5)&31,b=(rgb>>10)&31;
        atlas[y*256+x]=((r*255/31)<<16)|((gg*255/31)<<8)|(b*255/31);}
    snapshot(dir,"floor-left-ceiling-right",g->level,atlas,256,128);
    {uint32_t *layout=malloc(1024*512*sizeof(*layout));CHECK(layout);
        for(y=0;y<640;++y)for(x=0;x<640;++x){unsigned surface=x>=320,tile=(x%320)/64+5*(y/64);
            unsigned shade=tile<SSA_SURFACE_DECAL_TILES?d->atlas->masks[surface][0][tile][(y%64)*2*S+(x%64)*2]:0,value=255-shade/2;
            layout[y*640+x]=(value<<16)|(value<<8)|value;}
        snapshot(dir,"stain-tile-library",g->level,layout,640,640);
        for(y=0;y<512;++y)for(x=0;x<1024;++x){unsigned surface=x>=512;
            unsigned shade=ssa_surface_decals_texel(d,surface,60*S+(int32_t)(x%512)*2,60*S+(int32_t)y*2),value=255-shade/2;
            layout[y*1024+x]=(value<<16)|(value<<8)|value;}
        snapshot(dir,"stain-clusters-floor-left-ceiling-right",g->level,layout,1024,512);free(layout);}
}
static void slave_pool(SsaNativeGame *g,SsaPcRenderer *r,const uint8_t *rom,const char *dir){
    static const unsigned sectors[]={9,38,39},heights[]={96,144,128};
    SsaPlanePool pool={0};SsaRenderCamera camera={0};uint8_t fb[14400];unsigned i,y,pitch;
    CHECK(ssa_native_game_init(g,rom,0x800000,3)==1 && ssa_native_game_prepare_view(g)==1);
    CHECK(!g->surface_decals.water_floor[120]); /* Pale RGB5(27,30,31), not saturated blue. */
    CHECK(!ssa_surface_decals_water(NULL,120,96));
    for(i=0;i<3;++i){const uint8_t *s=g->map.data+g->map.sectors+sectors[i]*20;
        CHECK(s[10]==120 && (unsigned)(s[0]|s[1]<<8)==heights[i]);
        CHECK(ssa_surface_decals_water(&g->surface_decals,120,heights[i])==(i==0));}
    /* Actual pool camera; original floor colors/heights remain untouched. */
    g->pc_camera.x=-2048*65536;g->pc_camera.y=768*65536;g->pc_camera.z=224*65536;
    g->pc_camera.sector=9;g->pc_camera.heading=0;g->surface_decals_enabled=1;g->water_reflections=1;
    CHECK(ssa_pc_renderer_resize(r,480,320)==1);
    for(pitch=0;pitch<2;++pitch){unsigned visible=0;
        g->render_pitch=pitch?-4096:0;CHECK(ssa_pc_renderer_draw(r,g)==1);++draws;
        for(i=0;i<480*320;++i)if(r->water.mask[i]>>24){CHECK((r->water.mask[i]&65535)==96);++visible;}
        CHECK(visible>100 && r->water.passes==1);
        snapshot(dir,pitch?"slave-pool-pitched":"slave-pool-flat",3,r->pixels,480,320);}
    /* GBA visible-plane overlays: pool receives reflections and no stains;
     * the same-color stair treads produce no water stencil. */
    camera.z=224*65536;camera.cosine=-65536;pool.count=3;
    for(i=0;i<3;++i){SsaPlane *p=&pool.records[i];p->height=(int16_t)heights[i];p->color=120;
        p->min_x=p->max_x=(int16_t)(50+i);memset(p->top,255,sizeof p->top);
        p->top[51+i]=80;p->bottom[51+i]=119;}
    memset(fb,120,sizeof fb);
    CHECK(ssa_surface_decals_reference(&g->surface_decals,&pool,&camera,fb,sizeof fb)==1);
    for(y=80;y<120;++y)CHECK(fb[y*120+50]==120);
    CHECK(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
        0,0,224,0,0,1,120,120)==1);
    CHECK(ssa_water_reference(&r->water,&pool,&camera,fb,sizeof fb)==1);
    for(y=80;y<120;++y){CHECK(r->water.mask[y*120+50]>>24);
        CHECK(!r->water.mask[y*120+51] && !r->water.mask[y*120+52]);}
    g->surface_decals.level=0;CHECK(!ssa_surface_decals_water(&g->surface_decals,120,96));
    g->surface_decals.level=6;CHECK(!ssa_surface_decals_water(&g->surface_decals,120,96));
    ssa_native_game_free(g);
}
static void caesar_umt_floor(SsaNativeGame *g,SsaPcRenderer *r,const uint8_t *rom){
    static const unsigned sectors[]={43,44,62};
    SsaPlanePool pool={0};SsaRenderCamera camera={0};uint8_t fb[14400];unsigned i,y;
    SsaPaletteEffect palette={0};SsaSurfaceDecals other={0};
    CHECK(ssa_native_game_init(g,rom,0x800000,11)==1);
    CHECK(ssa_map_sector(&g->map,-192*65536,-448*65536)==43);
    for(i=0;i<3;++i){const uint8_t *s=g->map.data+g->map.sectors+sectors[i]*20;
        CHECK(s[10]==36 && !s[0] && !s[1]);}
    CHECK(!g->surface_decals.water_floor[36] && !ssa_surface_decals_water(&g->surface_decals,36,0));
    CHECK(ssa_palette_effect_init(&palette,rom,0x800000,6)==1);
    CHECK(ssa_surface_decals_init(&other,palette.colors,6)==1 && other.water_floor[36]);
    /* Software shading must return the unchanged dry material, with no water
     * stencil even when reflection and animated decals are both enabled. */
    CHECK(ssa_water_begin(&r->water,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
        -192,-448,64,40,1,1,120,120)==1);
    CHECK(ssa_water_rgb(&r->water,36,-192,-448,0,100,0)==(g->palette_effect.colors[36]&0x7fff));
    CHECK(!r->water.mask[0] && !r->water.passes);
    /* GBA plane overlays use the same dry classification. */
    camera.z=64*65536;camera.cosine=65536;pool.count=1;
    pool.records[0].height=0;pool.records[0].color=36;pool.records[0].min_x=pool.records[0].max_x=50;
    memset(pool.records[0].top,255,sizeof pool.records[0].top);
    pool.records[0].top[51]=80;pool.records[0].bottom[51]=119;memset(fb,36,sizeof fb);
    CHECK(ssa_water_reference(&r->water,&pool,&camera,fb,sizeof fb)==1);
    for(y=80;y<120;++y)CHECK(!r->water.mask[y*120+50] && fb[y*120+50]==36);
    ssa_native_game_free(g);
}
int main(int argc,char **argv){
    uint8_t *rom=malloc(0x800000);SsaNativeGame *g=calloc(1,sizeof(*g));SsaPcRenderer r={0};FILE *f;
    uint32_t *old=malloc(480*320*4);float *depth=malloc(480*320*sizeof(float));unsigned level,pitch,i,total,reference_changed=0;
    CHECK(argc>=2 && rom && g && old && depth);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    water_blend_checks();materials(rom);
    for(level=0;level<=6;level+=6){
        CHECK(ssa_native_game_init(g,rom,0x800000,level)==1 && !g->surface_decals_enabled);
        CHECK(ssa_native_game_tick(g)==1);masks(g,argc>2?argv[2]:NULL);
        total=0;
        for(pitch=0;pitch<3;++pitch){uint32_t frame=g->frame_game.counter;SsaInventory inventory=g->players[0].inventory;
            g->player.pitch=pitch==1?6000:pitch==2?-6000:0;g->surface_decals_enabled=0;
            CHECK(ssa_native_game_prepare_view(g)==1 && ssa_pc_renderer_resize(&r,480,320)==1 && ssa_pc_renderer_draw(&r,g)==1);
            memcpy(old,r.pixels,480*320*4);memcpy(depth,r.depth,480*320*sizeof(float));
            if(!pitch)snapshot(argc>2?argv[2]:NULL,"off",level,r.pixels,480,320);
            g->surface_decals_enabled=1;CHECK(ssa_pc_renderer_draw(&r,g)==1);
            CHECK(!memcmp(depth,r.depth,480*320*sizeof(float)));
            for(i=0;i<480*320;++i){if(r.pixels[i]!=old[i]){++total;++changed_pixels;CHECK(r.depth[i]<FLT_MAX);} }
            memcpy(old,r.pixels,480*320*4);CHECK(ssa_pc_renderer_draw(&r,g)==1 && !memcmp(old,r.pixels,480*320*4));
            CHECK(g->frame_game.counter==frame && !memcmp(&inventory,&g->players[0].inventory,sizeof inventory));draws+=3;
            if(!pitch)snapshot(argc>2?argv[2]:NULL,"on",level,r.pixels,480,320);
            if(pitch==2)snapshot(argc>2?argv[2]:NULL,"look-down",level,r.pixels,480,320);
        }
        /* Sparse artwork can leave a whole starting view clear. Both render
         * paths must change visible surfaces across the combined fixtures. */
        g->player.pitch=0;g->surface_decals_enabled=0;CHECK(ssa_native_game_draw(g)==1);
        memcpy(old,g->rgb888,38400*4);g->surface_decals_enabled=1;CHECK(ssa_native_game_draw(g)==1);
        total=0;for(i=0;i<38400;++i)total+=old[i]!=g->rgb888[i];reference_changed+=total;draws+=2;
        snapshot(argc>2?argv[2]:NULL,"gba",level,g->rgb888,240,160);
        water_checks(g,&r,argc>2?argv[2]:NULL);
        ssa_native_game_free(g);
    }
    slave_pool(g,&r,rom,argc>2?argv[2]:NULL);
    caesar_umt_floor(g,&r,rom);
    CHECK(changed_pixels>100 && reference_changed>5);ssa_pc_renderer_free(&r);free(depth);free(old);free(g);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"draws\":%u,\"software_changed_pixels\":%u,\"renderers\":2,\"pitch_views\":3}\n",checks,draws,changed_pixels);return 0;
}
