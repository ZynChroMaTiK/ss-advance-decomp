#include "ssa/native_game.h"
#include "ssa/pc_scene.h"
#include "ssa/settings.h"
#include "ssa/actor_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"shadows line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include "sprite_shadow_fixture.h"
int main(int argc,char **argv)
{
    SsaNativeGame *g=calloc(1,sizeof(*g));SsaSpriteShadows *s=calloc(1,sizeof(*s));SsaPcRenderer r={0};
    uint8_t *rom=malloc(0x800000),art[16*16]={0},fb[14400];SsaEntity e[4];SsaActorSource a[4];
    SsaActorResource saved;SsaActorImage image={0,-8,-16,16,16,art};SsaActorQueue queue={0};SsaWorldVisibility visibility={0};int16_t clips[240];
    SsaSettings settings=SSA_SETTINGS_DEFAULT;uint16_t palette[256];uint8_t colors[4][256];FILE *f;
    unsigned i,changed=0,gba_changed=0,ground_radius,ground_strength;int floor;uint32_t *before;
    CHECK(argc>1 && g && s && rom);f=fopen(argv[1],"rb");CHECK(f && fread(rom,1,0x800000,f)==0x800000);fclose(f);
    CHECK(ssa_native_game_init(g,rom,0x800000,0)==1 && ssa_native_game_step(g)==1);
    for(i=0;i<256;++i){unsigned c=i&31;palette[i]=(uint16_t)(c|(c<<5)|(c<<10));}
    ssa_sprite_shadow_palette(colors,palette);
    CHECK(colors[0][31]==31 && palette[colors[3][31]]<palette[31]);
    saved=g->actor_assets.resources[1];g->actor_assets.resources[1]=(SsaActorResource){0,0,1,1,2,1,&image};
    for(i=0;i<4;++i){e[i]=g->players[0].entity;e[i].body.z=e[i].body.floor*65536;
        a[i]=(SsaActorSource){&e[i],1,0,0,0,0};}
    e[1].type=0xcd;e[1].radius=64*65536;e[2].type=0xe1;e[2].radius=0;e[3].type=0x65;
    a[0].flags=128; /* Prepared data can also be used by the exposed reflected player. */
    floor=e[0].body.floor;
    CHECK(ssa_sprite_shadows_prepare(s,&g->map,&g->actor_assets,a,4,&e[0],&g->surface_decals,colors[0])==1);
    CHECK(s->count==3 && s->sprites[1].radius>s->sprites[0].radius && s->sprites[0].radius>s->sprites[2].radius);
    CHECK(s->indices[0]==0 && s->indices[1]==1 && s->indices[2]==2 && s->indices[3]==-1);
    CHECK(s->sprites[0].pixels[0]==0 && s->sprites[0].pixels[16*8+4]!=0);
    CHECK(s->sprites[1].radius==68); /* Preserve the requested 25% enlargement. */
    ground_radius=s->sprites[0].radius;ground_strength=s->sprites[0].strength;
    e[0].body.z+=128*65536;
    CHECK(ssa_sprite_shadows_prepare(s,&g->map,&g->actor_assets,a,1,&e[0],&g->surface_decals,colors[0])==1);
    CHECK(s->count==1 && s->sprites[0].radius<ground_radius && s->sprites[0].strength<ground_strength && s->sprites[0].floor==floor);
    e[0].body.z+=128*65536;
    CHECK(ssa_sprite_shadows_prepare(s,&g->map,&g->actor_assets,a,1,&e[0],&g->surface_decals,colors[0])==1 && !s->count);
    e[0].body.z=floor*65536;
    CHECK(ssa_sprite_shadows_prepare(s,&g->map,&g->actor_assets,a,1,&e[0],&g->surface_decals,colors[0])==1);
    /* Attached sprites need their own expanded wall silhouette, including
     * columns outside the subject. Closed and opening clips follow GBA rules. */
    visibility.map=&g->map;visibility.head=visibility.tail=0;visibility.wall_count=1;
    visibility.walls[0].previous=visibility.walls[0].next=65535;
    visibility.walls[0].left=10;visibility.walls[0].right=90;
    visibility.walls[0].depth_left=visibility.walls[0].depth_right=20000;
    CHECK(ssa_actor_queue_begin(&queue,&visibility)==1);
    queue.wall_clips[0].flags=3;queue.wall_clips[0].bottom_height=32767;queue.wall_clips[0].top_height=-32768;
    CHECK(ssa_actor_queue_sprite_clips(&queue,10000,0,0,0,0,0,119,clips)==1);
    for(i=0;i<120;++i)CHECK(i>=10 && i<=90 ? clips[2*i]==120 && clips[2*i+1]==-1 : clips[2*i]==-1 && clips[2*i+1]==120);
    CHECK(ssa_actor_queue_sprite_clips(&queue,30000,0,0,0,0,0,119,clips)==1);
    for(i=0;i<120;++i)CHECK(clips[2*i]==-1 && clips[2*i+1]==120);
    memset(fb,80,120);memset(fb+120,20,120);
    queue.wall_clips[0].bottom_height=32;queue.wall_clips[0].top_height=64;
    queue.wall_clips[0].bottom=fb;queue.wall_clips[0].bottom_size=120;
    queue.wall_clips[0].top=fb+120;queue.wall_clips[0].top_size=120;
    CHECK(ssa_actor_queue_sprite_clips(&queue,10000,0,0,0,100,0,119,clips)==1);
    CHECK(clips[100]==20 && clips[101]==80);
    CHECK(ssa_actor_queue_sprite_clips(&queue,10000,0,0,32,64,0,119,clips)==1);
    CHECK(clips[100]==-1 && clips[101]==120);
    g->actor_assets.resources[1]=saved;
    settings.sprite_shadows=1;CHECK(ssa_native_game_apply_settings(g,&settings)==1 && g->sprite_shadows_enabled);
    CHECK(ssa_native_game_draw(g)==1); /* Original queue composition with sprite underlays. */
    /* At the exact spawn camera the Colt's floor is below the GBA viewport.
     * Lower this render-only camera so both subject and oval are visible. */
    {SsaRenderCamera view=g->render_view.camera;uint8_t subject[14400]={0};unsigned pass;
        view.z-=32*65536;
        CHECK(ssa_sprite_shadows_prepare(s,&g->map,&g->actor_assets,g->sources,g->spawn.pool.active_count,
            &g->players[0].entity,&g->surface_decals,g->shadow_colors[0])==1);
        for(pass=0;pass<3;++pass){const uint8_t *pixels;
            CHECK(ssa_world_scene_clear(&g->world_scene,0)==1);
            CHECK(ssa_world_scene_draw(&g->world_scene,&view,g->frame_game.counter,view.heading)==1);
            if(pass)CHECK(ssa_sprite_shadows_reference(s,&g->actor_scene,&view,g->frame_game.flags,
                g->sources,g->spawn.pool.active_count,g->source_z_offsets)==1);
            else CHECK(ssa_actor_scene_draw_offsets(&g->actor_scene,&view,g->frame_game.flags,
                g->sources,g->spawn.pool.active_count,g->source_z_offsets)==1);
            pixels=ssa_world_scene_pixels(&g->world_scene);
            if(!pass){SsaActorQueue *q=&g->actor_scene.queue;memcpy(fb,pixels,sizeof fb);
                CHECK(q->draw_count==1);
                for(i=0;i<q->draw_count;++i){SsaActorRecord *a=&q->actors[q->draw_order[i]];
                    CHECK(ssa_actor_raster_draw(&g->actor_assets,&q->projections[a->projection],
                        q->clip_words+a->clip_offset,SSA_ACTOR_CLIP_WORDS-a->clip_offset,subject,sizeof subject)==1);}}
            else if(pass==1){for(i=0;i<14400;++i){gba_changed+=fb[i]!=pixels[i];if(subject[i])CHECK(subject[i]==pixels[i]);}
                CHECK(gba_changed>0);memcpy(fb,pixels,sizeof fb);}
            else CHECK(!memcmp(fb,pixels,sizeof fb));
        }
    }
    CHECK(ssa_pc_renderer_resize(&r,480,320)==1);g->render_pitch=-4096;
    g->sprite_shadows_enabled=0;CHECK(ssa_pc_scene_draw(&r,g)==1);before=malloc(480*320*4);CHECK(before);memcpy(before,r.pixels,480*320*4);
    g->sprite_shadows_enabled=1;CHECK(ssa_pc_scene_draw(&r,g)==1);changed=0;
    for(i=0;i<480*320;++i)changed+=before[i]!=r.pixels[i];
    CHECK(changed>10);
    if(argc>2){f=fopen(argv[2],"wb");CHECK(f);fprintf(f,"P6\n480 320\n255\n");
        for(i=0;i<480*320;++i){fputc((r.pixels[i]>>16)&255,f);fputc((r.pixels[i]>>8)&255,f);fputc(r.pixels[i]&255,f);}CHECK(!fclose(f));}
    settings.sprite_shadows=0;CHECK(ssa_native_game_apply_settings(g,&settings)==1 && !g->sprite_shadows_enabled);
    CHECK(ssa_pc_scene_draw(&r,g)==1 && !memcmp(before,r.pixels,480*320*4));
    sprite_shadow_fixture(&r,g);
    free(before);ssa_pc_renderer_free(&r);ssa_native_game_free(g);free(g);free(s);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"native_shadow_pixels\":%u,\"gba_shadow_pixels\":%u}\n",checks,changed,gba_changed);return 0;
}
