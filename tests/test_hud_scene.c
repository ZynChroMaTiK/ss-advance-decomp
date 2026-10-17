/* Reuse the established real-map movement/actor fixture, not a mock world. */
#define main actor_scene_fixture_main
#include "test_actor_scene.c"
#undef main
#include "ssa/hud_scene.h"

int main(int argc,char **argv)
{
    FILE *file;uint8_t *rom;Assets *a;Fixture *f;SsaSpriteAssets *sprites;
    SsaHudScene *hud,*saved;uint16_t *pixels,*previous;unsigned level,index;
    unsigned hud_frames=0,weapon_hidden=0,raised=0,health_changes=0,hud_guards=0;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file!=NULL);
    rom=malloc(8u*1024u*1024u);a=calloc(1,sizeof(*a));f=calloc(1,sizeof(*f));
    sprites=calloc(1,sizeof(*sprites));hud=calloc(1,sizeof(*hud));saved=malloc(sizeof(*saved));
    pixels=malloc(38400*sizeof(*pixels));previous=malloc(38400*sizeof(*previous));
    CHECK(rom&&a&&f&&sprites&&hud&&saved&&pixels&&previous);
    CHECK(fread(rom,1,8u*1024u*1024u,file)==8u*1024u*1024u);CHECK(!fclose(file));
    assets_load(a,rom,8u*1024u*1024u);
    CHECK(ssa_sprite_assets_load(sprites,rom,8u*1024u*1024u)==1);
    for(level=0;level<16;level++) {
        unsigned family=(unsigned)ssa_sprite_palette_family(level);SsaPlayerSpawn *p;
        start(f,a,rom,8u*1024u*1024u,level);p=&f->players[0];
        CHECK(ssa_hud_scene_init(hud,&p->inventory,&p->entity,sprites,&a->math,
            rom,8u*1024u*1024u,level,0)==1);
        CHECK(hud->dispatch.active_count==75&&hud->pool.free_count==9);
        CHECK(hud->weapon.inventory==&p->inventory&&hud->weapon.source==&p->entity);
        CHECK(hud->weapon_object==&hud->objects[0]&&hud->control.current==1);
        for(index=0;index<240;index++) {
            SsaBody body;int32_t old_y;unsigned third;size_t n;
            tick(f,index);body=p->entity.body;old_y=hud->weapon_object->y;third=f->pose.third_person;
            if(index==30){p->inventory.health=75;p->inventory.stat_c4=25;health_changes++;}
            if(index==140){p->inventory.health=25;p->inventory.stat_c4=100;health_changes++;}
            if(index==200){p->inventory.health=100;p->inventory.stat_c4=0;health_changes++;}
            hud->state.health=p->inventory.health;hud->state.armor=p->inventory.stat_c4;
            hud->state.local_flags=p->motion.movement.flags;hud->state.time=index;
            hud->state.score=level*2000u+index;hud->state.game_flags=(uint16_t)(index>=40&&index<44?1:0);
            CHECK(ssa_hud_scene_step_idle(hud,1,index>=50&&index<54?3:0,third)==1);
            CHECK(hud->state.health==p->inventory.health&&hud->state.armor==p->inventory.stat_c4);
            CHECK(memcmp(&body,&p->entity.body,sizeof(body))==0);
            CHECK(p->inventory.ammo[0]==999&&hud->state.ammo[0]==999);
            if(!(hud->state.game_flags&1)) {
                CHECK(((hud->weapon_object->flags&1)==0)==!!(third||(hud->state.local_flags&8)));
                if(third)weapon_hidden++;
            }
            if((hud->state.game_flags&1)||(index>=50&&index<54))CHECK(hud->weapon_object->y==old_y);
            if(hud->weapon_object->y==0x5000)raised++;
            CHECK(ssa_hud_scene_draw(hud,ssa_world_scene_pixels(&f->geometry),14400,family,pixels,38400)==1);
            CHECK(hud->oam.count>0&&hud->oam.count<=128&&hud->cache.transfer_count==0);
            CHECK(!hud->updating&&!hud->dispatch.updating&&!hud->cache.updating&&!hud->oam.updating);
            hud_frames++;
            if(index==0||index==140) {
                char path[128];snprintf(path,sizeof(path),"build/native-tests/hud-scene-%02u-%03u.bin",level,index);
                file=fopen(path,"wb");CHECK(file!=NULL);
                CHECK(fwrite(ssa_world_scene_pixels(&f->geometry),1,14400,file)==14400);
                CHECK(fwrite(hud->oam.entries,1,1024,file)==1024);
                CHECK(fwrite(hud->cache.tiles,1,32768,file)==32768);
                CHECK(fwrite(sprites->background_palette[family],2,256,file)==256);
                CHECK(fwrite(sprites->palette,2,256,file)==256);
                CHECK(fwrite(pixels,2,38400,file)==38400);CHECK(!fclose(file));
            }
            if(index==0) {
                unsigned *locks[]={&hud->updating,&hud->dispatch.updating,&hud->pool.updating,
                    &hud->cache.updating,&hud->oam.updating,&hud->weapon.updating,&hud->loadout.updating};
                memcpy(previous,pixels,38400*2);
                for(n=0;n<sizeof(locks)/sizeof(*locks);n++) {
                    *locks[n]=1;*saved=*hud;
                    CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1);
                    CHECK(ssa_hud_scene_draw(hud,ssa_world_scene_pixels(&f->geometry),14400,family,pixels,38400)==-1);
                    CHECK(ssa_hud_scene_free(hud)==-1&&memcmp(saved,hud,sizeof(*hud))==0);
                    CHECK(memcmp(previous,pixels,38400*2)==0);*locks[n]=0;hud_guards+=3;
                }
                *saved=*hud;CHECK(ssa_hud_scene_step_idle(hud,256,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->dispatch.active_count=85;*saved=*hud;
                CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->dispatch.active_count=75;hud->dispatch.active[0]=84;*saved=*hud;
                CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->dispatch.active[0]=0;hud->dispatch.objects[0]=NULL;*saved=*hud;
                CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->dispatch.objects[0]=&hud->objects[0];
                hud->weapon.object=NULL;hud->weapon_object=NULL;*saved=*hud;
                CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->weapon.object=&saved->objects[0];hud->weapon_object=&saved->objects[0];*saved=*hud;
                CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1&&memcmp(saved,hud,sizeof(*hud))==0);hud_guards++;
                hud->weapon.object=&hud->objects[0];hud->weapon_object=&hud->objects[0];
                CHECK(ssa_hud_scene_draw(hud,ssa_world_scene_pixels(&f->geometry),14400,2,pixels,38400)==-1);hud_guards++;
                CHECK(memcmp(previous,pixels,38400*2)==0);
            }
        }
        /* Original weapon slot precedes HUD slots: commit must be visible now. */
        p->inventory.weapons|=2;p->inventory.ammo[1]=30;
        hud->control.requested=2;hud->control.flags|=0x100;
        hud->weapon_object->y=0x5000;hud->state.game_flags=0;
        CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==1);
        CHECK(hud->control.current==1&&hud->state.current_weapon==2&&hud->state.weapon_action==1);
        hud->weapon_object->y=0xd800;hud->state.game_flags=0;
        CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==1);
        CHECK(hud->control.current==2&&hud->state.current_weapon==2&&hud->state.weapon_action==2);
        CHECK(hud->state.weapon_flags==hud->control.flags&&!(hud->control.flags&0x100));
        CHECK(hud->weapon_object->y==0xdc00&&hud->weapon_object->animation==1);
        CHECK(ssa_hud_scene_draw(hud,ssa_world_scene_pixels(&f->geometry),14400,family,pixels,38400)==1);
        CHECK(ssa_hud_scene_free(hud)==1&&p->entity.owner==&f->world&&sprites->loaded);
        CHECK(ssa_hud_scene_step_idle(hud,1,0,0)==-1);hud_guards++;
        finish(f);
    }
    CHECK(hud_frames==3840&&weapon_hidden>0&&raised>0&&health_changes==48);
    ssa_sprite_assets_free(sprites);ssa_actor_assets_free(&a->actors);ssa_texture_assets_free(&a->textures);
    free(previous);free(pixels);free(saved);free(hud);free(sprites);free(f);free(a);free(rom);
    printf("{\"asan_ubsan_assertions\":%u,\"maps\":16,\"presentation_frames\":%u,\"hidden_weapon_frames\":%u,\"raised_weapon_frames\":%u,\"canonical_health_changes\":%u,\"hud_guards\":%u,\"airborne_frames\":%u,\"visible_sam_frames\":%u}\n",
        checks,hud_frames,weapon_hidden,raised,health_changes,hud_guards,airborne,body_frames);
    return 0;
}
