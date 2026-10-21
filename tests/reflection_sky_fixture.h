/* Change only the backdrop artwork: reflected water must change in RGB and
 * indexed output. Uses the real pool with render-only sky ceiling colors. */
static void reflection_sky_fixture(SsaPcRenderer *r,SsaNativeGame *g)
{
    unsigned family=ssa_texture_assets_family(g->level),pass,i,dark=1,light=1,changed=0;
    unsigned decals=g->surface_decals_enabled;size_t bytes=sizeof g->sky.pixels[0];
    uint8_t *saved=malloc(bytes),*ceilings=malloc(g->map.sector_count),indexed[14400],baseline[14400];
    uint32_t *rgb=malloc((size_t)r->width*r->height*4);SsaWaterRender *w=calloc(1,sizeof(*w));SsaPcRenderer *mirror=NULL;
    CHECK(saved && ceilings && rgb && w);memcpy(saved,g->sky.pixels[family],bytes);g->surface_decals_enabled=0;
    /* This pool camera faces a solid ceiling. Expose sky without changing
     * floor geometry or water classification, so reflection is unambiguous. */
    for(i=0;i<g->map.sector_count;++i){uint8_t *sector=g->map.data+g->map.sectors+i*20;
        ceilings[i]=sector[11];sector[11]=0;}
    for(i=1;i<256;++i){unsigned c=g->palette_effect.colors[i],d=g->palette_effect.colors[dark],l=g->palette_effect.colors[light];
        unsigned value=(c&31)+((c>>5)&31)+((c>>10)&31);
        if(value<(d&31)+((d>>5)&31)+((d>>10)&31))dark=i;
        if(value>(l&31)+((l>>5)&31)+((l>>10)&31))light=i;}
    CHECK(dark!=light);
    for(pass=0;pass<2;++pass){
        memset(g->sky.pixels[family],pass?(int)light:(int)dark,bytes);
        CHECK(ssa_pc_renderer_draw(r,g)==1 && r->water.passes);
        if(!pass)memcpy(rgb,r->pixels,(size_t)r->width*r->height*4);
        else {for(i=0;i<r->width*r->height;++i)if(r->water.mask[i]>>24)changed+=rgb[i]!=r->pixels[i];CHECK(changed>20);}
        CHECK(ssa_water_begin(w,&g->water_scene,&g->surface_decals,g->palette_effect.colors,
            g->pc_camera.x/65536.0,g->pc_camera.y/65536.0,g->pc_camera.z/65536.0,0,0,1,120,120)==1);
        /* Preserve real visible-water coverage while changing target resolution. */
        for(i=0;i<14400;++i)w->mask[i]=r->water.mask[(size_t)(i/120*r->height/120)*r->width+i%120*r->width/120];
        memset(indexed,0,sizeof indexed);
        CHECK(ssa_pc_renderer_reflect(&mirror,g,&g->pc_camera,g->render_pitch,w,NULL,indexed)==1 && w->passes);
        if(!pass)memcpy(baseline,indexed,sizeof baseline);
        else {changed=0;for(i=0;i<14400;++i)if(w->mask[i]>>24)changed+=baseline[i]!=indexed[i];CHECK(changed>10);}
    }
    memcpy(g->sky.pixels[family],saved,bytes);g->surface_decals_enabled=decals;
    for(i=0;i<g->map.sector_count;++i)g->map.data[g->map.sectors+i*20+11]=ceilings[i];
    CHECK(ssa_pc_renderer_draw(r,g)==1);
    ssa_pc_renderer_free(mirror);free(mirror);ssa_water_free(w);free(w);free(saved);free(ceilings);free(rgb);
}
