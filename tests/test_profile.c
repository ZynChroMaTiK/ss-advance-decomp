#include "ssa/profile.h"
#include "ssa/menu.h"
#include "ssa/native_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,frames,starts;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"profile check %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint16_t pixels[38400];
static void step(SsaMenu *m,unsigned key){CHECK(ssa_menu_step(m,key)==1);CHECK(ssa_menu_draw(m,pixels,38400)==1);++frames;}
static void preview(SsaMenu *m,const char *name,unsigned language){
 char path[128];FILE *f;unsigned i;m->draw_page=m->page;m->fade_volume=m->fading=0;m->logo_y=-64;
 CHECK(ssa_menu_draw(m,pixels,38400)==1);snprintf(path,sizeof path,"build/native-tests/%s-%u.ppm",name,language);
 f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 160\n255\n");
 for(i=0;i<38400;++i){unsigned c=pixels[i];uint8_t rgb[3]={(uint8_t)((c&31)*255/31),(uint8_t)(((c>>5)&31)*255/31),(uint8_t)(((c>>10)&31)*255/31)};CHECK(fwrite(rgb,1,3,f)==3);}
 CHECK(!fclose(f));
}
static void chapter_rows(SsaMenu *m){
 SsaMapView reference=m->canvas;uint16_t expected[38400];unsigned i,x,y;
 CHECK(m->completed_levels==0xfff && !m->fade_volume);
 CHECK(ssa_menu_draw(m,pixels,38400)==1);
 for(i=0;i<6;++i){unsigned level=m->level_scroll+i,top=40+i*16;const char *label=ssa_menu_level_name(m,level);
  CHECK(label);
  reference.glyph_count=0;
  CHECK(ssa_map_view_text_centered(&reference,label,(int)top,1,m->selection==level?3:4,0)==(int)top+8);
  CHECK(ssa_map_view_compose(&reference,expected,38400)==1);
  for(y=top;y<top+8;++y)for(x=0;x<240;++x)CHECK(pixels[y*240+x]==expected[y*240+x]);
 }
}
int main(int argc,char **argv){
 uint8_t *rom=malloc(0x800000),b[SSA_PROFILE_BYTES],bad[SSA_PROFILE_BYTES];SsaMenu *m=calloc(1,sizeof(*m));
 SsaProfile p,q,before;FILE *f;unsigned language,mask,level,i,difficulty;
 CHECK(argc==2 && rom && m);f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
 for(language=0;language<3;++language)for(mask=0;mask<4096;++mask){
  p.language=language;p.completed=(uint16_t)mask;CHECK(ssa_profile_encode(&p,b,sizeof b)==1);
  ssa_profile_reset(&q);CHECK(ssa_profile_decode(&q,b,sizeof b)==1 && q.language==language && q.completed==mask);
  for(level=0;level<12;++level)CHECK(ssa_profile_unlocked(&q,level)==(!level || (mask&(1u<<(level-1)))!=0));
 }
 ssa_profile_reset(&p);CHECK(ssa_profile_complete(&p,5)==1 && p.completed==32);
 CHECK(ssa_profile_unlocked(&p,6)==1 && !ssa_profile_unlocked(&p,5) && !ssa_profile_unlocked(&p,1));
 CHECK(ssa_profile_complete(&p,5)==0 && ssa_profile_complete(&p,12)==-1 && ssa_profile_unlocked(&p,12)==-1);
 CHECK(ssa_profile_encode(&p,b,sizeof b)==1);q.language=2;q.completed=0x555;before=q;
 for(i=0;i<128;++i){memcpy(bad,b,sizeof b);bad[i/8]^=(uint8_t)(1u<<(i%8));CHECK(ssa_profile_decode(&q,bad,sizeof bad)==-1 && q.language==before.language && q.completed==before.completed);}
 for(i=0;i<SSA_PROFILE_BYTES;++i)CHECK(ssa_profile_decode(&q,b,i)==-1 && q.completed==before.completed);
 p.language=3;CHECK(ssa_profile_encode(&p,b,sizeof b)==-1);p.language=0;p.completed=0x1000;CHECK(ssa_profile_encode(&p,b,sizeof b)==-1);
 for(language=0;language<3;++language){
  memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);m->language=language;m->level_limit=SSA_NATIVE_CAMPAIGN_LEVELS;
  m->page=m->draw_page=SSA_MENU_SINGLE;m->marker=0;m->fade_volume=m->fading=0;
  step(m,0);CHECK(ssa_menu_pointer(m,120,106,1)==SSA_MENU_CONFIRM && m->selection==1);
  step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_LEVEL_SELECT);step(m,0);
  CHECK(ssa_menu_level_name(m,11) && !ssa_menu_level_name(m,12));
  preview(m,"level-select",language);
  /* Fixed chapter pages: arrows retain row, boundaries do not shift a mixed
   * sliding window, and Up/Down crosses between levels6and7. */
  step(m,SSA_MENU_RIGHT);CHECK(m->selection==6 && m->level_scroll==6);
  step(m,SSA_MENU_RIGHT);CHECK(m->selection==6 && m->level_scroll==6);
  step(m,SSA_MENU_LEFT);CHECK(!m->selection && !m->level_scroll);
  step(m,SSA_MENU_LEFT);CHECK(!m->selection && !m->level_scroll);
  for(i=0;i<5;++i)step(m,SSA_MENU_DOWN);
  CHECK(m->selection==5 && !m->level_scroll);
  step(m,SSA_MENU_DOWN);CHECK(m->selection==6 && m->level_scroll==6);
  step(m,SSA_MENU_UP);CHECK(m->selection==5 && !m->level_scroll);
  step(m,0);CHECK(ssa_menu_pointer(m,228,16,1)==SSA_MENU_RIGHT);
  step(m,SSA_MENU_RIGHT);CHECK(m->selection==11 && m->level_scroll==6);
  step(m,0);CHECK(ssa_menu_pointer(m,120,124,0)==0 && m->selection==11);
  preview(m,"level-select-chapter2",language);
  step(m,0);CHECK(ssa_menu_pointer(m,12,16,1)==SSA_MENU_LEFT);
  step(m,SSA_MENU_LEFT);CHECK(m->selection==5 && !m->level_scroll);
  m->completed_levels=0xfff;chapter_rows(m);
  step(m,SSA_MENU_RIGHT);chapter_rows(m);
  step(m,SSA_MENU_LEFT);for(i=0;i<5;++i)step(m,SSA_MENU_UP);
  m->completed_levels=0;
  step(m,SSA_MENU_DOWN);step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_LEVEL_SELECT && m->commands[0]==17 && !m->leaving);
  m->completed_levels=1;step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_DIFFICULTY && m->level==1);
  step(m,SSA_MENU_BACK);CHECK(m->page==SSA_MENU_LEVEL_SELECT && m->selection==1);
  step(m,SSA_MENU_BACK);CHECK(m->page==SSA_MENU_SINGLE && m->selection==1);
  step(m,SSA_MENU_UP);step(m,SSA_MENU_CONFIRM);step(m,SSA_MENU_CONFIRM);
  CHECK(m->leaving && !m->level && !m->loadout && !m->selecting_level);
  for(level=0;level<12;++level)for(difficulty=0;difficulty<3;++difficulty){
   memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);m->language=language;m->completed_levels=0xfff;m->level_limit=SSA_NATIVE_CAMPAIGN_LEVELS;
   m->page=m->draw_page=SSA_MENU_SINGLE;m->selection=1;step(m,SSA_MENU_CONFIRM);
   for(i=0;i<level;++i)step(m,SSA_MENU_DOWN);
   CHECK(m->selection==level && m->level_scroll==(level/6)*6);
   step(m,0);CHECK(ssa_menu_pointer(m,120,40+(int)(level-m->level_scroll)*16+4,1)==SSA_MENU_CONFIRM);
   step(m,SSA_MENU_CONFIRM);
   if(level>=SSA_NATIVE_CAMPAIGN_LEVELS){CHECK(m->page==SSA_MENU_LEVEL_SELECT && !m->leaving && m->unsupported==SSA_MENU_UNAVAILABLE_LEVEL+level);continue;}
   CHECK(m->page==SSA_MENU_DIFFICULTY);
   if(level==6 && !difficulty){step(m,SSA_MENU_BACK);CHECK(m->page==SSA_MENU_LEVEL_SELECT && m->selection==6 && m->level_scroll==6);
    step(m,0);step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_DIFFICULTY);}
   for(i=0;i<difficulty;++i)step(m,SSA_MENU_DOWN);
   step(m,SSA_MENU_CONFIRM);CHECK(m->leaving && m->level==level && m->loadout==difficulty*12+level+1);++starts;
  }
  memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);m->language=language;m->completed_levels=0x555;
  m->settings.music_volume=25;m->settings.jump_enabled=m->settings.alternate_weapons=1;
  CHECK(ssa_scores_insert(&m->scores,4000000,"RESET_TEST__")==0);
  m->page=m->draw_page=SSA_MENU_OPTIONS;m->selection=3;step(m,SSA_MENU_CONFIRM);
  CHECK(m->page==SSA_MENU_CLEAR_DATA && m->selection==1);step(m,0);preview(m,"clear-data",language);
  step(m,SSA_MENU_CONFIRM);CHECK(m->page==SSA_MENU_OPTIONS && m->completed_levels==0x555 && m->language==language && !m->clear_requested);
  CHECK(m->settings.music_volume==25 && m->settings.jump_enabled && m->scores.entries[0].score==4000000);
  step(m,SSA_MENU_CONFIRM);step(m,SSA_MENU_UP);step(m,SSA_MENU_BACK);
  CHECK(m->completed_levels==0x555 && m->language==language && !m->clear_requested);
  step(m,SSA_MENU_CONFIRM);step(m,0);CHECK(ssa_menu_pointer(m,120,118,1)==SSA_MENU_CONFIRM && !m->selection);
  step(m,SSA_MENU_CONFIRM);CHECK(m->clear_requested && !m->language && !m->completed_levels && m->selection==3);
  CHECK(m->settings.music_volume==100 && !m->settings.jump_enabled && !m->settings.alternate_weapons && m->scores.entries[0].score==2000000);
  step(m,0);CHECK(!m->clear_requested);
 }
 free(m);free(rom);printf("{\"result\":\"pass\",\"checks\":%u,\"frames\":%u,\"level_starts\":%u,\"campaign_names\":12,\"languages\":3}\n",checks,frames,starts);return 0;
}
