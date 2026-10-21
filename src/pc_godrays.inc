/* Native adapters only: the fixed-point geometry/rasterizer and indexed blend
 * table are shared with the GBA path. Existing scene depth supplies occlusion. */
typedef struct GodrayJob {SsaPcRenderer *r;unsigned top,bottom;} GodrayJob;
static unsigned godray_index(SsaPcGeometry *cache,uint32_t color){
    unsigned key=((color>>19)&31)|((color>>6)&992)|((color<<7)&31744),bit=1u<<(key&7),i,best=0,error=UINT_MAX;
    if(!(cache->godray_valid[key>>3]&bit)){
        for(i=0;i<256;++i){unsigned c=cache->godray_palette.colors[i];
            int dr=(int)(key&31)-(int)(c&31),dg=(int)((key>>5)&31)-(int)((c>>5)&31),db=(int)(key>>10)-(int)((c>>10)&31);
            unsigned e=(unsigned)(dr*dr+dg*dg+db*db);if(e<error){best=i;error=e;}}
        cache->godray_nearest[key]=(uint8_t)best;cache->godray_valid[key>>3]|=(uint8_t)bit;
    }
    return cache->godray_nearest[key];
}
static void godray_span(void *context,const SsaGodraySpan *span){
    GodrayJob *job=context;SsaPcRenderer *r=job->r;SsaPcGeometry *cache=r->geometry;unsigned n;
    int64_t q=span->q,vq=span->vq,uq=span->uq;
    if(span->y<job->top || span->y>=job->bottom)return;
    for(n=0;n<span->count;++n,q+=span->dq,vq+=span->dvq,uq+=span->duq){uint32_t depth;unsigned shade;
        (void)ssa_godray_sample(q/65536,vq/65536,&depth);
        shade=ssa_godray_dither(q/65536,vq/65536,uq/65536,span->bottom,span->top);
        size_t at=(size_t)span->y*r->width+span->x+n;unsigned source,target;
        if(!shade || depth==UINT32_MAX || depth/256.0>=r->depth[at])continue;
        source=godray_index(cache,r->pixels[at]);target=cache->godray_palette.blend[shade][source];
        /* Preserve an unmodified reflected color when the palette has no
         * brighter match for this step, rather than quantizing a no-op. */
        if(target!=source)r->pixels[at]=rgb(cache->godray_palette.colors[target]);
    }
}
static int godrays_draw(SsaPcRenderer *r,const SsaNativeGame *g,const Camera *c){
    SsaGodrayCamera camera={0};GodrayJob job={r,c->top,c->bottom};SsaGodrayOps ops={godray_span,&job};int count;
    if(!r->geometry)return -1;
    if(!r->geometry->godray_palette.loaded || memcmp(r->geometry->godray_palette.colors,g->palette_effect.colors,512)){
        memset(r->geometry->godray_valid,0,sizeof r->geometry->godray_valid);
        memcpy(r->geometry->godray_palette.colors,g->palette_effect.colors,512);
        r->geometry->godray_palette.loaded=1;}
    if(!g->godrays.palette.loaded)return -1;
    memcpy(r->geometry->godray_palette.blend,g->godrays.palette.blend,sizeof g->godrays.palette.blend);
    camera.x=(int32_t)(c->x*256);camera.y=(int32_t)(c->y*256);camera.z=(int32_t)(c->z*256);
    camera.sine=(int32_t)(c->sine*65536);camera.cosine=(int32_t)(c->cosine*65536);
    camera.pitch_sine=(int32_t)(c->ps*65536);camera.pitch_cosine=(int32_t)(c->pc*65536);
    camera.focal_x=camera.focal_y=(int32_t)(c->focal*256);camera.width=r->width;camera.height=r->height;
    camera.clip=c->clip;camera.clip_z=(int32_t)c->clip_z;
    count=ssa_godrays_draw(&g->map,&camera,&ops);if(count<0)return -1;
    r->godray_triangles=(uint64_t)count;return 1;
}
