#include "ssa/menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"menu check failed: %s line%d\n",#x,__LINE__);exit(1);}}while(0)
static unsigned checks,frames,starts;
static void held_step(SsaMenu *m,unsigned input,unsigned held,uint16_t *pixels)
{
    CHECK(ssa_menu_step_input(m,input,held)==1);pixels[0]=0x1234;pixels[38401]=0x4321;
    CHECK(ssa_menu_draw(m,pixels+1,38400)==1 && pixels[0]==0x1234 && pixels[38401]==0x4321);
    CHECK(m->canvas.glyph_count<=128 && m->command_count<=4);++frames;
}
static void step(SsaMenu *m,unsigned input,uint16_t *pixels)
{
    CHECK(ssa_menu_step(m,input)==1);pixels[0]=0x1234;pixels[38401]=0x4321;
    CHECK(ssa_menu_draw(m,pixels+1,38400)==1 && pixels[0]==0x1234 && pixels[38401]==0x4321);
    CHECK(m->canvas.glyph_count<=128 && m->command_count<=4);++frames;
}
int main(int argc,char **argv)
{
    uint8_t *rom=malloc(0x800000),*copy;SsaMenu *m=calloc(1,sizeof(*m)),*saved=malloc(sizeof(*saved));
    uint16_t *pixels=malloc(38402*sizeof(*pixels));FILE *f;unsigned language,difficulty,music,i;
    CHECK(argc==2 && rom && m && saved && pixels);f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(rom,1,0x800000,f)==0x800000);fclose(f);
    /* First visit already has BG1, before any gameplay or menu reentry. */
    CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
    CHECK(m->page==SSA_MENU_TITLE && m->draw_page==SSA_MENU_TITLE && !m->language);
    m->draw_page=SSA_MENU_LANGUAGE; /* Compare the raw stone beneath the grid. */
    m->fading=m->fade_volume=0;CHECK(ssa_menu_draw(m,pixels+1,38400)==1);
    *saved=*m;
    m->draw_page=SSA_MENU_TITLE;CHECK(ssa_menu_draw(m,pixels+1,38400)==1);
    {unsigned x,y,opaque=0;
        for(y=0;y<160;++y)for(x=0;x<240;++x){unsigned c=m->grid[(y&7)*8+(x&7)];
            if(c){CHECK(m->canvas.background[y*240+x]==c);++opaque;}
            else CHECK(m->canvas.background[y*240+x]==saved->canvas.background[y*240+x]);
        }
        CHECK(opaque>0 && opaque<38400);
        m->draw_page=SSA_MENU_MAIN;CHECK(ssa_menu_draw(m,pixels+1,38400)==1);
        CHECK(memcmp(m->canvas.background,saved->canvas.background,sizeof(m->canvas.background))!=0);
    }
    for(language=0;language<3;++language)for(difficulty=0;difficulty<3;++difficulty)for(music=0;music<2;++music){
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,music)==1);
        *saved=*m;CHECK(ssa_menu_init(m,rom,0x800000,music)==-1 && !memcmp(m,saved,sizeof(*m)));
        CHECK(ssa_menu_step(m,128)==-1 && !memcmp(m,saved,sizeof(*m)));
        CHECK(ssa_menu_draw(m,pixels,38399)==-1 && !memcmp(m,saved,sizeof(*m)));
        step(m,SSA_MENU_START,pixels);CHECK(m->page==SSA_MENU_TITLE && m->command_count==1 && m->commands[0]==8);
        for(i=0;i<90;++i){step(m,SSA_MENU_START,pixels);CHECK(m->page==SSA_MENU_TITLE);}
        step(m,SSA_MENU_START,pixels);CHECK(m->page==SSA_MENU_MAIN);
        for(i=0;i<20;++i)step(m,0,pixels);
        CHECK(m->offset==0 && m->logo_y==8);
        step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_CONFIRM,pixels);
        CHECK(m->unsupported==3 && m->page==SSA_MENU_MAIN);
        step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_OPTIONS);
        CHECK(m->selection==4 && m->music_enabled==music);
        step(m,SSA_MENU_UP,pixels);CHECK(m->selection==4 && m->commands[0]==17);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==6);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_GRAPHICS);
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_OPTIONS && m->selection==6);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==5);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==3);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==1);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==1 && m->commands[0]==17);
        step(m,SSA_MENU_UP,pixels);
        step(m,SSA_MENU_UP,pixels);
        step(m,SSA_MENU_UP,pixels);
        step(m,SSA_MENU_UP,pixels);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_SETTINGS);
        step(m,SSA_MENU_LEFT,pixels);CHECK(m->language==2 && m->page==SSA_MENU_SETTINGS);
        step(m,SSA_MENU_RIGHT,pixels);CHECK(!m->language && !m->selection && m->music_enabled==music);
        for(i=0;i<3;++i){unsigned click=ssa_menu_pointer(m,120,28,1);
            CHECK(click==SSA_MENU_CONFIRM);step(m,click,pixels);
            CHECK(m->language==(i+1)%3 && m->page==SSA_MENU_SETTINGS && !m->selection && m->music_enabled==music);
        }
        for(i=0;i<language;++i)step(m,SSA_MENU_RIGHT,pixels);
        CHECK(m->language==language && m->canvas.language==language);
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_OPTIONS && m->selection==4);
        for(i=0;i<4;++i)step(m,SSA_MENU_DOWN,pixels);
        CHECK(m->selection==1 && m->music_enabled==music);
        step(m,SSA_MENU_CONFIRM,pixels);
        CHECK(m->page==SSA_MENU_CREDITS && !m->unsupported);
        held_step(m,0,0,pixels);CHECK(m->credits_y==160*65536-0x8000 && !m->credits_lines);
        for(i=0;i<40;++i)held_step(m,0,0,pixels);
        *saved=*m;CHECK(ssa_menu_draw(m,pixels+1,38400)==1 && !memcmp(m,saved,sizeof(*m)));
        CHECK(ssa_menu_step_input(m,0,128)==-1 && !memcmp(m,saved,sizeof(*m)));
        held_step(m,0,SSA_MENU_UP|SSA_MENU_DOWN,pixels);CHECK(m->credits_y==saved->credits_y && !m->credits_speed);
        held_step(m,0,SSA_MENU_DOWN,pixels);CHECK(m->credits_y==saved->credits_y-0x20000);
        held_step(m,0,0,pixels);CHECK(m->credits_speed==0x8000);
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_CREDITS && m->credits_done && !m->canvas.glyph_count);
        step(m,0,pixels);CHECK(m->page==SSA_MENU_OPTIONS && m->selection==4 && m->marker==65535);
        if(!difficulty && !music){
            step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_DOWN,pixels);
            step(m,SSA_MENU_DOWN,pixels);
            step(m,SSA_MENU_CONFIRM,pixels);
            for(i=0;i<2000 && m->page==SSA_MENU_CREDITS;++i)held_step(m,0,SSA_MENU_DOWN,pixels);
            CHECK(m->page==SSA_MENU_OPTIONS && m->credits_done && m->credits_lines==88);
        }
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_MAIN);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_SINGLE);
        step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_DOWN,pixels);step(m,SSA_MENU_CONFIRM,pixels);
        CHECK(!m->unsupported && m->page==SSA_MENU_PASSWORD && !strcmp(m->password,"A_______"));
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(!m->leaving && m->commands[0]==17);
        step(m,SSA_MENU_LEFT,pixels);CHECK(m->password_cursor==7);
        step(m,SSA_MENU_UP,pixels);CHECK(m->password[7]=='A');
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_SINGLE);
        step(m,SSA_MENU_UP,pixels);step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_DIFFICULTY);
        for(i=0;i<difficulty;++i)step(m,SSA_MENU_DOWN,pixels);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->difficulty==difficulty && m->leaving && m->fade_audio==music && !m->done);
        for(i=1;i<=61;++i){step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->done==(i==61));}
        CHECK(ssa_menu_step(m,0)==0 && m->done);++starts;
    }
    for(i=0;i<36;++i){unsigned ticks=0;
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        m->page=m->draw_page=SSA_MENU_PASSWORD;m->fade_volume=m->fading=0;
        CHECK(ssa_menu_password(m,i));strcpy(m->password,ssa_menu_password(m,i));
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->leaving && m->level==i%12 && m->difficulty==i/12 && m->loadout==i+1);
        while(!m->done && ticks++<100)step(m,0,pixels);
        CHECK(m->done && ticks==61);
        m->leaving=m->done=0;m->level_limit=6;m->loadout=0;
        step(m,SSA_MENU_CONFIRM,pixels);
        CHECK(i%12<6?m->leaving:!m->leaving && m->unsupported==SSA_MENU_UNAVAILABLE_LEVEL+i%12 && !m->loadout);
    }
    /* PC text entry uses real ROM codes, including the short MEDIUM_ code.
     * Type quickly before a simulation tick; no text is lost or treated as WASD. */
    for(i=0;i<36;++i){const char *code;unsigned j;
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        m->page=m->draw_page=SSA_MENU_PASSWORD;m->fading=m->fade_volume=0;
        memcpy(m->password,"A_______",9);code=ssa_menu_password(m,i);CHECK(code);
        for(j=0;code[j];++j)CHECK(ssa_menu_text(m,code[j]>='A' && code[j]<='Z'?(unsigned)code[j]+32:(unsigned)code[j])==1);
        CHECK(!strcmp(m->password,code) && m->password_cursor==strlen(code));
        CHECK(ssa_menu_text(m,'!')==0 && ssa_menu_text(m,0x10041)==0 && ssa_menu_text(m,' ')==0);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->leaving && m->loadout==i+1);
        CHECK(ssa_menu_text(m,'X')==0);
    }
    /* Underscores in the eight-slot display are padding, not required text.
     * Reproduce ordinary words typed without any arrow edit, including the
     * ROM's short MEDIUM_ entry, partial padding and mouse submission. */
    for(i=0;i<36;++i){const char *code;size_t stem,padding,j;
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        code=ssa_menu_password(m,i);CHECK(code);stem=strlen(code);while(stem && code[stem-1]=='_')--stem;
        for(padding=0;padding<=8-stem;++padding){
            memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
            m->page=m->draw_page=SSA_MENU_PASSWORD;m->fading=m->fade_volume=0;
            memcpy(m->password,"A_______",9);
            for(j=0;j<stem;++j)CHECK(ssa_menu_text(m,(unsigned)code[j]+32)==1);
            for(j=0;j<padding;++j)CHECK(ssa_menu_text(m,'_')==1);
            step(m,0,pixels);CHECK(m->password_typed && m->password_cursor==stem+padding);
            step(m,padding&1?ssa_menu_pointer(m,120,120,1):SSA_MENU_CONFIRM,pixels);
            CHECK(m->leaving && m->loadout==i+1 && m->level==i%12 && m->difficulty==i/12);
        }
    }
    {static const char *bad[]={"EA","HARDX","MEDIU","NOTACODE","________"};unsigned k,j;
        for(k=0;k<sizeof bad/sizeof bad[0];++k){
            memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
            m->page=m->draw_page=SSA_MENU_PASSWORD;m->fading=m->fade_volume=0;
            for(j=0;bad[k][j];++j)CHECK(ssa_menu_text(m,(unsigned)bad[k][j])==1);
            step(m,SSA_MENU_CONFIRM,pixels);CHECK(!m->leaving && !m->loadout && m->commands[0]==17);
        }
    }
    {char name[9]="SAM";uint8_t cursor=3;
        CHECK(ssa_ui_text(name,9,&cursor,' ')==1);CHECK(ssa_ui_text(name,9,&cursor,'2')==1);
        CHECK(!strcmp(name,"SAM 2") && cursor==5);
        CHECK(ssa_ui_text(name,9,&cursor,8)==1 && !strcmp(name,"SAM "));
        cursor=0;CHECK(ssa_ui_text(name,9,&cursor,127)==1 && !strcmp(name,"AM "));
        CHECK(ssa_ui_text(name,9,&cursor,8)==0);CHECK(ssa_ui_text(name,9,&cursor,'s')==1);
        CHECK(!strcmp(name,"SAM ") && cursor==1);
        CHECK(ssa_ui_text(name,4,&cursor,'X')==-1);
    }
    memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
    m->page=m->draw_page=SSA_MENU_PASSWORD;m->fading=m->fade_volume=0;memcpy(m->password,"A_______",9);
    for(i=0;i<8;++i)CHECK(ssa_menu_text(m,'a'+i)==1);
    CHECK(ssa_menu_text(m,'Z')==0 && !strcmp(m->password,"ABCDEFGH"));
    CHECK(ssa_menu_text(m,8)==1 && !strcmp(m->password,"ABCDEFG"));
    step(m,SSA_MENU_LEFT,pixels);CHECK(m->password_cursor==6);
    CHECK(ssa_menu_text(m,127)==1 && !strcmp(m->password,"ABCDEF"));
    CHECK(ssa_menu_text(m,'z')==1 && !strcmp(m->password,"ABCDEFZ"));
    step(m,SSA_MENU_UP,pixels);CHECK(!m->password_typed && !strcmp(m->password,"ABCDEFZA"));
    for(language=0;language<3;++language){
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        m->page=m->draw_page=SSA_MENU_MAIN;m->language=language;m->fading=m->fade_volume=0;
        for(i=0;i<16;++i)step(m,0,pixels);
        CHECK(!m->offset);
        for(i=0;i<5;++i){m->selection=(i+1)%5;
            CHECK(ssa_menu_pointer(m,120,82+(int)i*16,0)==0 && m->selection==i);
            CHECK(ssa_menu_pointer(m,120,82+(int)i*16,1)==SSA_MENU_CONFIRM);
        }
        CHECK(ssa_menu_pointer(m,1,1,1)==0 && m->selection==4);
        CHECK(ssa_menu_pointer(m,-1,100,1)==0 && ssa_menu_pointer(m,240,100,1)==0);
        CHECK(ssa_menu_pointer(m,120,114,1)==SSA_MENU_CONFIRM && m->selection==2);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_OPTIONS);
        CHECK(ssa_menu_pointer(m,120,82,1)==0); /* Don't click through old page. */
        step(m,0,pixels);CHECK(ssa_menu_pointer(m,120,82,1)==SSA_MENU_CONFIRM && m->selection==4);
        /* Quit is a distinct terminal result, never a new-game handoff. */
        m->page=m->draw_page=SSA_MENU_MAIN;m->selection=0;m->offset=0;
        for(i=0;i<4;++i)step(m,SSA_MENU_DOWN,pixels);
        CHECK(m->selection==4 && !m->quit_requested && !m->done);
        step(m,SSA_MENU_DOWN,pixels);CHECK(m->selection==4 && m->commands[0]==17);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_QUIT && m->selection==1 && !m->quit_requested);
        step(m,0,pixels);CHECK(ssa_menu_pointer(m,1,1,1)==0 && m->selection==1);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_MAIN && m->selection==4 && !m->quit_requested);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_QUIT && m->selection==1);
        step(m,SSA_MENU_UP,pixels);CHECK(!m->selection);
        step(m,SSA_MENU_BACK,pixels);CHECK(m->page==SSA_MENU_MAIN && m->selection==4 && !m->quit_requested);
        step(m,SSA_MENU_CONFIRM,pixels);step(m,0,pixels);
        CHECK(ssa_menu_pointer(m,120,136,1)==SSA_MENU_CONFIRM && m->selection==1);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_MAIN && !m->quit_requested);
        step(m,SSA_MENU_CONFIRM,pixels);step(m,0,pixels);
        CHECK(ssa_menu_pointer(m,120,120,1)==SSA_MENU_CONFIRM && !m->selection);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->quit_requested && !m->done && !m->leaving && !m->clear_requested);
        CHECK(ssa_menu_step(m,SSA_MENU_CONFIRM)==0);
    }
    /* Scores is a real Main destination. Pointer arrows page the original
     * three groups, ordinary clicks leave, and every re-entry resets motion. */
    for(language=0;language<3;++language){
        memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,rom,0x800000,1)==1);
        m->language=language;m->page=m->draw_page=SSA_MENU_MAIN;m->offset=0;
        m->fading=m->fade_volume=0;m->marker=0;m->logo_y=8;
        CHECK(ssa_menu_pointer(m,120,130,1)==SSA_MENU_CONFIRM && m->selection==3);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->page==SSA_MENU_SCORES && !m->unsupported);
        for(i=0;i<72;++i)step(m,0,pixels);
        CHECK(m->score_y==16*65536 && !m->score_speed);
        CHECK(ssa_menu_pointer(m,232,150,1)==SSA_MENU_DOWN);
        step(m,SSA_MENU_DOWN,pixels);
        for(i=1;i<88;++i)step(m,0,pixels);
        CHECK(m->score_y==-160*65536 && !m->score_speed);
        step(m,SSA_MENU_DOWN,pixels);
        for(i=1;i<88;++i)step(m,0,pixels);
        CHECK(m->score_y==-336*65536 && !m->score_speed);
        step(m,SSA_MENU_DOWN,pixels);CHECK(!m->score_speed);
        CHECK(ssa_menu_pointer(m,232,8,1)==SSA_MENU_UP);
        step(m,SSA_MENU_UP,pixels);
        for(i=1;i<88;++i)step(m,0,pixels);
        CHECK(m->score_y==-160*65536 && !m->score_speed);
        CHECK(ssa_menu_pointer(m,120,80,1)==SSA_MENU_CONFIRM);
        step(m,SSA_MENU_CONFIRM,pixels);CHECK(m->score_leaving);
        step(m,0,pixels);CHECK(m->page==SSA_MENU_MAIN && !m->done);
    }
    {int x,y,ox,oy,ow,oh;
        CHECK(ssa_ui_viewport(1000,1000,240,160,&ox,&oy,&ow,&oh)==1 && ox==0 && oy==167 && ow==1000 && oh==666);
        CHECK(ssa_ui_pointer(1000,1000,240,160,500,500,&x,&y)==1 && x==120 && y==80);
        CHECK(!ssa_ui_pointer(1000,1000,240,160,500,166,&x,&y));
        CHECK(!ssa_ui_pointer(1000,1000,240,160,500,833,&x,&y));
        CHECK(ssa_ui_pointer(1200,600,240,160,150,0,&x,&y)==1 && !x && !y);
        CHECK(!ssa_ui_pointer(1200,600,240,160,149,0,&x,&y));
        CHECK(ssa_ui_pointer(1200,600,240,160,1049,599,&x,&y)==1 && x==239 && y==159);
        CHECK(!ssa_ui_pointer(1200,600,240,160,1050,599,&x,&y));
        CHECK(!ssa_ui_pointer(0,600,240,160,0,0,&x,&y));
    }
    memset(m,0,sizeof(*m));CHECK(ssa_menu_init(m,NULL,0x800000,1)==-1);
    CHECK(ssa_menu_init(m,rom,0x7fffff,1)==-1);CHECK(ssa_menu_init(m,rom,0x800000,2)==-1);
    copy=malloc(0x800000);CHECK(copy);memcpy(copy,rom,0x800000);copy[0xb5434]=0;
    CHECK(ssa_menu_init(m,copy,0x800000,1)==-1 && !m->canvas.loaded);
    memcpy(copy,rom,0x800000);copy[0xaaeed]=0;
    CHECK(ssa_menu_init(m,copy,0x800000,1)==-1 && !m->canvas.loaded);
    memcpy(copy,rom,0x800000);CHECK(ssa_menu_init(m,copy,0x800000,1)==1);
    m->page=SSA_MENU_CREDITS;m->marker=65535;
    {const char *s=ssa_map_view_text(&m->canvas,246);size_t offset=(size_t)((const uint8_t *)s-copy);
        memset(copy+offset,'X',32);copy[offset+32]=0;
        CHECK(ssa_menu_step(m,0)==-1);}
    free(copy);free(pixels);free(saved);free(m);free(rom);
    printf("{\"result\":\"pass\",\"checks\":%u,\"frames\":%u,\"new_games\":%u,\"languages\":3,\"difficulties\":3,\"music_modes\":2}\n",checks,frames,starts);
    return 0;
}
