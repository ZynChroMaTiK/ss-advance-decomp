/* Subject pixels must win over their attached shadow at every tested pitch.
 * A broad, low sprite deliberately overlaps the ellipse. Shared CPU/GPU test. */
static void sprite_shadow_fixture(SsaPcRenderer *r,SsaNativeGame *g)
{
    SsaActorResource saved=g->actor_assets.resources[1];SsaActorSource sources[100];
    int32_t offsets[100];SsaRenderCamera camera=g->pc_camera;unsigned count=g->spawn.pool.active_count;
    unsigned enabled=g->sprite_shadows_enabled,fov=g->fov_offset;int pitch=g->render_pitch;
    uint16_t palette=g->palette_effect.colors[255];SsaEntity e={0};uint8_t art[32*16];
    SsaActorImage im={0,-16,-8,32,16,art};uint32_t *before=malloc(321*213*4);unsigned mode,i;int32_t sine,cosine;
    CHECK(before && ssa_pc_renderer_resize(r,321,213)==1);
    memcpy(sources,g->sources,sizeof sources);memcpy(offsets,g->source_z_offsets,sizeof offsets);
    memset(g->source_z_offsets,0,sizeof offsets);memset(art,255,sizeof art);g->palette_effect.colors[255]=0x7c1f;
    g->actor_assets.resources[1]=(SsaActorResource){0,0,1,1,2,1,&im};
    g->spawn.pool.active_count=1;g->sources[0]=(SsaActorSource){&e,1,0,0,0,0};
    ssa_sincos(&g->math,camera.heading,&sine,&cosine);
    e.body.x=camera.x+sine*128;e.body.y=camera.y+cosine*128;e.sector=camera.sector;e.type=0xcd;e.radius=48*65536;
    {const uint8_t *s=g->map.data+g->map.sectors+camera.sector*20;
        int z=(int16_t)(s[0]|s[1]<<8);e.body.z=z*65536;e.body.height=32*65536;g->pc_camera.z=(z+32)*65536;}
    for(mode=0;mode<3;++mode){unsigned subject=0,shadow=0;
        g->render_pitch=mode==0?0:mode==1?-2048:2048;g->fov_offset=mode==2?60:10;
        g->sprite_shadows_enabled=0;CHECK(ssa_pc_scene_draw(r,g)==1);memcpy(before,r->pixels,321*213*4);
        g->sprite_shadows_enabled=1;CHECK(ssa_pc_scene_draw(r,g)==1);
        for(i=0;i<321*213;++i){if(before[i]==0xff00ff){++subject;CHECK(r->pixels[i]==before[i]);}
            shadow+=before[i]!=r->pixels[i];}
        CHECK(subject>100 && shadow>20);
        g->sprite_shadows_enabled=0;CHECK(ssa_pc_scene_draw(r,g)==1 && !memcmp(before,r->pixels,321*213*4));
    }
    g->actor_assets.resources[1]=saved;g->palette_effect.colors[255]=palette;
    memcpy(g->sources,sources,sizeof sources);memcpy(g->source_z_offsets,offsets,sizeof offsets);
    g->spawn.pool.active_count=count;g->pc_camera=camera;g->render_pitch=pitch;g->fov_offset=fov;g->sprite_shadows_enabled=enabled;
    free(before);
}
