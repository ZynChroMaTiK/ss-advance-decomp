#include "ssa/menu.h"
#include "ssa/map_assets.h"
#include <stdio.h>
#include <string.h>
enum {SELECT_TOP=40,SELECT_SPACING=16};
enum {PANEL_LEFT=32,VALUE_LEFT=144,BAR_RIGHT=176,BAR_SPAN=BAR_RIGHT-PANEL_LEFT,
      BAR_TOP=96,BAR_SPACING=32,BAR_HEIGHT=8};
static const int settings_top[7]={8,32,48,64,80,112,152};
static const unsigned option_ids[5]={4,5,2,3,1}; /* Settings, Extras, Language, Clear Data, Credits. */

static unsigned u32(const uint8_t *p)
{return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static int decode(SsaMenu *m,unsigned at,uint8_t *out,size_t size)
{
    return (u32(m->canvas.rom+at)>>8)==size &&
        ssa_lz77_decode(m->canvas.rom+at,m->canvas.rom_size-at,out,size,NULL);
}
int ssa_menu_init(SsaMenu *m,const uint8_t *rom,size_t size,unsigned music_enabled)
{
    if(!m || m->canvas.loaded || music_enabled>1)return -1;
    if(ssa_map_view_init(&m->canvas,rom,size,0,0,1)!=1)return -1;
    if(!decode(m,0xa8368,m->tiles,sizeof(m->tiles)) || !decode(m,0xb5434,m->logo,sizeof(m->logo)) ||
       !decode(m,0xaaeec,m->grid,sizeof(m->grid))){
        memset(m,0,sizeof(*m));return -1;}
    m->page=m->draw_page=SSA_MENU_TITLE;m->marker=65535;m->music_enabled=music_enabled;
    m->settings=(SsaSettings)SSA_SETTINGS_DEFAULT;
    m->logo_y=-64;m->fade_volume=0xf00;m->fade_step=-64;m->fading=1;
    m->canvas.fade_volume=0;m->level_limit=12;return 1;
}
static void command(SsaMenu *m,unsigned c)
{if(m->command_count<4)m->commands[m->command_count++]=c;}
static void page(SsaMenu *m,unsigned p)
{m->page=p;m->selection=p==SSA_MENU_OPTIONS?option_ids[0]:0;m->marker=65535;}
int ssa_ui_viewport(int w,int h,int rw,int rh,int *x,int *y,int *ow,int *oh)
{
    if(w<=0 || h<=0 || rw<=0 || rh<=0 || !x || !y || !ow || !oh)return 0;
    if((int64_t)w*rh<=(int64_t)h*rw){*ow=w;*oh=(int)((int64_t)w*rh/rw);if(*oh<1)*oh=1;}
    else{*oh=h;*ow=(int)((int64_t)h*rw/rh);if(*ow<1)*ow=1;}
    *x=(w-*ow)/2;*y=(h-*oh)/2;return 1;
}
int ssa_ui_pointer(int w,int h,int rw,int rh,int x,int y,int *out_x,int *out_y)
{
    int left,top,ow,oh;
    if(!out_x || !out_y || !ssa_ui_viewport(w,h,rw,rh,&left,&top,&ow,&oh) ||
       x<left || y<top || x-left>=ow || y-top>=oh)return 0;
    *out_x=(int)((int64_t)(x-left)*rw/ow);*out_y=(int)((int64_t)(y-top)*rh/oh);return 1;
}
int ssa_ui_text(char *s,size_t capacity,uint8_t *cursor,unsigned c)
{
    size_t n;
    if(!s || !cursor || !capacity || capacity>256)return -1;
    for(n=0;n<capacity && s[n];++n){}
    if(n==capacity || *cursor>n)return -1;
    if(c==8){if(!*cursor)return 0;--*cursor;memmove(s+*cursor,s+*cursor+1,n-*cursor);return 1;}
    if(c==127){if(*cursor==n)return 0;memmove(s+*cursor,s+*cursor+1,n-*cursor);return 1;}
    if(c>='a' && c<='z')c-='a'-'A';
    if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c==' '))return 0;
    if(n+1>=capacity)return 0;
    memmove(s+*cursor+1,s+*cursor,n-*cursor+1);s[(*cursor)++]=(char)c;return 1;
}
int ssa_menu_text(SsaMenu *m,unsigned c)
{
    if(!m || !m->canvas.loaded)return -1;
    if(m->page!=SSA_MENU_PASSWORD || m->leaving)return 0;
    if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c==8 || c==127))return 0;
    if(!m->password_typed){m->password_typed=1;m->password[0]=0;m->password_cursor=0;}
    return ssa_ui_text(m->password,sizeof(m->password),&m->password_cursor,c);
}
static const char *option_text(SsaMenu *m,unsigned selection,char language[64])
{
    static const char *labels[3]={"Language","Sprache","Langue"};
    static const char *clear[3]={"Clear Data","Daten loeschen","Effacer donnees"};
    static const char *settings[3]={"Settings","Einstellungen","Reglages"};
    static const char *extra[3]={"Extras","Extras","Extras"};
    const char *name;
    if(selection==1)return ssa_map_view_text(&m->canvas,8);
    if(selection==3)return clear[m->language];
    if(selection==4)return settings[m->language];
    if(selection==5)return extra[m->language];
    if(m->language>=3 || !(name=ssa_map_view_text(&m->canvas,237+m->language)))return NULL;
    if(snprintf(language,64,"%s: %s",labels[m->language],name)>=64)return NULL;
    return language;
}
static const char *select_text(unsigned language){
 static const char *labels[3]={"Select Level","Levelauswahl","Choix du niveau"};return labels[language];
}
const char *ssa_menu_level_name(SsaMenu *m,unsigned level){
 if(!m || !m->canvas.loaded || m->language>=3 || level>=12)return NULL;
 m->canvas.language=m->language;return ssa_map_view_text(&m->canvas,48+level);
}
static int unlocked(const SsaMenu *m,unsigned level){return !level || (m->completed_levels&(1u<<(level-1)))!=0;}
static const char *single_text(SsaMenu *m,unsigned i){return i==1?select_text(m->language):ssa_map_view_text(&m->canvas,i?4:9);}
static const char *main_text(SsaMenu *m,unsigned i){
    static const char *quit[3]={"Quit","Beenden","Quitter"};
    return i==4?quit[m->language]:ssa_map_view_text(&m->canvas,i);
}
static const char *answer_text(unsigned language,unsigned yes){
 static const char *answers[3][2]={{"No","Yes"},{"Nein","Ja"},{"Non","Oui"}};return answers[language][yes];
}
unsigned ssa_menu_pointer(SsaMenu *m,int x,int y,unsigned click)
{
    unsigned i,count,indices[5];int top,offset=0;
    if(!m || !m->canvas.loaded || m->leaving || x<0 || x>=240 || y<0 || y>=160 || m->draw_page!=m->page)return 0;
    switch(m->page){
    case SSA_MENU_MAIN:count=5;top=74;break;
    case SSA_MENU_SINGLE:count=3;top=82;break;
    case SSA_MENU_OPTIONS:count=5;top=74;break;
    case SSA_MENU_SETTINGS:
        if(x<16 || x>=224)return 0;
        if(y>=settings_top[1]-8 && y<settings_top[4]){
            m->selection=y<settings_top[2]?0:y<settings_top[3]?1:2;return click?SSA_MENU_CONFIRM:0;}
        if(y>=settings_top[4] && y<settings_top[5])i=3;
        else if(y>=settings_top[5] && y<settings_top[6]-8)i=4;
        else if(y>=settings_top[6]-8 && y<160){m->selection=5;return click?SSA_MENU_CONFIRM:0;}
        else return 0;
        m->selection=i;
        if(click && y>=BAR_TOP+(int)(i-3)*BAR_SPACING && y<=BAR_TOP+(int)(i-3)*BAR_SPACING+BAR_HEIGHT &&
           x>=PANEL_LEFT && x<=BAR_RIGHT){unsigned *v=i==3?&m->settings.music_volume:&m->settings.sound_volume;
            *v=(unsigned)(x-PANEL_LEFT)*100/BAR_SPAN;}
        return 0;
    case SSA_MENU_EXTRA:
        if(x<16 || x>=224)return 0;
        if(y>=40 && y<136)m->selection=(unsigned)(y-40)/24;
        else if(y>=144 && y<160)m->selection=4;
        else return 0;
        return click?SSA_MENU_CONFIRM:0;
    case SSA_MENU_CLEAR_DATA:case SSA_MENU_QUIT:count=2;top=112;break;
    case SSA_MENU_LEVEL_SELECT:
        if(y<24 && click && (x<24 || x>=216))return x<24?SSA_MENU_LEFT:SSA_MENU_RIGHT;
        for(i=m->level_scroll;i<12 && i<m->level_scroll+SSA_MENU_LEVELS_PER_CHAPTER;++i){const char *s=ssa_menu_level_name(m,i);
            if(!s)return 0;
            {unsigned w=ssa_map_view_text_width(&m->canvas,s,1);int left=120-(int)(w/2),row=SELECT_TOP+(int)(i-m->level_scroll)*SELECT_SPACING;
             if(x>=left-3 && x<left+(int)w+3 && y>=row && y<row+16){m->selection=i;return click?SSA_MENU_CONFIRM:0;}}
        }
        return 0;
    case SSA_MENU_LANGUAGE:count=3;top=64;for(i=0;i<count;++i)indices[i]=237+i;break;
    case SSA_MENU_DIFFICULTY:count=3;top=82;for(i=0;i<count;++i)indices[i]=243+i;break;
    case SSA_MENU_TITLE:return click?SSA_MENU_START:0;
    case SSA_MENU_PASSWORD:case SSA_MENU_CREDITS:return click?SSA_MENU_CONFIRM:0;
    default:return 0;
    }
    m->canvas.language=m->language;
    for(i=0;i<count;++i){char language[64];const char *s=m->page==SSA_MENU_OPTIONS?option_text(m,option_ids[i],language):
            m->page==SSA_MENU_MAIN?main_text(m,i):m->page==SSA_MENU_SINGLE?single_text(m,i):(m->page==SSA_MENU_CLEAR_DATA || m->page==SSA_MENU_QUIT)?answer_text(m->language,!i):
            ssa_map_view_text(&m->canvas,indices[i]);
        const char *line=s;int row=top;
        if(!s)return 0;
        if(m->page==SSA_MENU_MAIN)offset=i&1?-(int)m->offset:(int)m->offset;
        do{unsigned width=ssa_map_view_text_width(&m->canvas,line,0);int left=120+offset-(int)(width/2);
            if(x>=left-3 && x<left+(int)width+3 && y>=row && y<row+16){m->selection=m->page==SSA_MENU_OPTIONS?option_ids[i]:i;return click?SSA_MENU_CONFIRM:0;}
            while(*line && *line!='\n')++line;
            if(*line)++line;
            row+=16;
        }while(*line);
        top=row;
    }
    return 0;
}
static void move(SsaMenu *m,unsigned input,unsigned last)
{
    if(input&SSA_MENU_UP){if(m->selection){--m->selection;command(m,12);}else command(m,17);}
    else if(input&SSA_MENU_DOWN){if(m->selection<last){++m->selection;command(m,12);}else command(m,17);}
}
static int credits_init(SsaMenu *m)
{
    const char *s,*end,*line;size_t count=0;
    if(m->language>=3)return -1;
    m->canvas.language=m->language;s=ssa_map_view_text(&m->canvas,246);
    if(!s)return -1;
    /* The original uses a 32-byte stack line buffer. Reject malformed data. */
    for(line=s,end=s;*end;++end){
        if(*end=='\n'){count=0;line=end+1;}
        else if(!(end==line && *end=='*') && ++count>=32)return -1;
    }
    m->credits_offset=(uint32_t)((const uint8_t *)s-m->canvas.rom);
    m->credits_end=(uint32_t)((const uint8_t *)end-m->canvas.rom);
    m->credits_y=160*65536;m->credits_done=m->credits_lines=0;return 1;
}
static void credits_step(SsaMenu *m,unsigned input,unsigned held)
{
    if(m->credits_done){page(m,SSA_MENU_OPTIONS);return;}
    m->credits_speed=held&SSA_MENU_UP?0:held&SSA_MENU_DOWN?0x20000:0x8000;
    if(input&(SSA_MENU_CONFIRM|SSA_MENU_BACK)){command(m,16);++m->credits_done;return;}
    m->credits_y-=m->credits_speed;
    if(m->credits_y<=-16*65536){
        m->credits_y+=16*65536;
        while(m->credits_offset<m->credits_end && m->canvas.rom[m->credits_offset]!='\n')++m->credits_offset;
        if(m->credits_offset==m->credits_end){++m->credits_done;page(m,SSA_MENU_OPTIONS);}
        else{++m->credits_offset;++m->credits_lines;}
    }
}
const char *ssa_menu_password(const SsaMenu *m,unsigned index)
{
    unsigned address,offset;const char *s,*end;
    if(!m || !m->canvas.loaded || index>=36)return NULL;
    address=u32(m->canvas.rom+0x44c1d4+index*4);
    if(address<0x08000000)return NULL;
    offset=address-0x08000000;
    if(offset>=m->canvas.rom_size)return NULL;
    s=(const char *)m->canvas.rom+offset;end=memchr(s,0,m->canvas.rom_size-offset);
    return end && end-s<=8?s:NULL;
}
static void start(SsaMenu *m)
{m->leaving=1;m->fade_volume=0;m->fade_step=64;m->fading=1;m->fade_audio=m->music_enabled;}
static int password_equal(const SsaMenu *m,const char *expected)
{
    size_t n,length;
    if(!m->password_typed)return !strcmp(m->password,expected);
    /* The typed field draws missing slots as underscores. Treat those slots
     * consistently at submission, including the ROM's short MEDIUM_ entry.
     * Keep the recovered eight-character cycling editor's comparison intact. */
    n=strlen(m->password);length=strlen(expected);
    while(n && m->password[n-1]=='_')--n;
    while(length && expected[length-1]=='_')--length;
    return n==length && !memcmp(m->password,expected,n);
}
static int password_step(SsaMenu *m,unsigned input,unsigned held)
{
    unsigned i;char *c;
    if(m->password_cursor>(m->password_typed?8:7) || m->password[8])return -1;
    if(m->password_typed){size_t n=strlen(m->password);
        if(input&SSA_MENU_RIGHT){if(m->password_cursor<n)++m->password_cursor;}
        else if(input&SSA_MENU_LEFT){if(m->password_cursor)--m->password_cursor;}
        if((input|held)&(SSA_MENU_UP|SSA_MENU_DOWN)){
            memset(m->password+n,'_',8-n);m->password[8]=0;m->password_typed=0;
            if(m->password_cursor==8)m->password_cursor=7;
        }
    }else{
        if(input&SSA_MENU_RIGHT)m->password_cursor=(uint8_t)((m->password_cursor+1)&7);
        else if(input&SSA_MENU_LEFT)m->password_cursor=(uint8_t)((m->password_cursor+7)&7);
    }
    m->password_down=held&SSA_MENU_DOWN?(m->password_down==255?32:(uint8_t)(m->password_down+1)):0;
    m->password_up=held&SSA_MENU_UP?(m->password_up==255?32:(uint8_t)(m->password_up+1)):0;
    c=&m->password[m->password_cursor];
    if((input&SSA_MENU_DOWN) || ((held&SSA_MENU_DOWN) && m->password_down>30 && !(m->password_down&3)))
        *c=*c=='A'?'_':*c=='0'?'Z':*c=='_'?'9':(char)(*c-1);
    else if((input&SSA_MENU_UP) || ((held&SSA_MENU_UP) && m->password_up>30 && !(m->password_up&3)))
        *c=*c=='Z'?'0':*c=='9'?'_':*c=='_'?'A':(char)(*c+1);
    if(input&SSA_MENU_CONFIRM){
        for(i=0;i<36;++i){const char *s=ssa_menu_password(m,i);
            if(!s)return -1;
            if(password_equal(m,s))break;
        }
        if(i==36)command(m,17);
        else if(i%12>=m->level_limit){command(m,17);m->unsupported=SSA_MENU_UNAVAILABLE_LEVEL+i%12;}
        else{m->level=i%12;m->difficulty=i/12;m->loadout=i+1;command(m,13);start(m);}
    }
    if(input&SSA_MENU_BACK){command(m,16);page(m,SSA_MENU_SINGLE);}
    return 1;
}
int ssa_menu_step(SsaMenu *m,unsigned input)
{return ssa_menu_step_input(m,input,input);}
int ssa_menu_step_input(SsaMenu *m,unsigned input,unsigned held)
{
    unsigned p;
    if(!m || !m->canvas.loaded || input>127 || held>127 || m->language>=3 || m->level_limit>12 ||
       (m->page==SSA_MENU_LEVEL_SELECT && (m->selection>=12 || (m->level_scroll!=0 && m->level_scroll!=SSA_MENU_LEVELS_PER_CHAPTER))))return -1;
    if(m->page==SSA_MENU_CREDITS && m->marker==65535 && credits_init(m)!=1)return -1;
    m->command_count=m->fade_audio=m->unsupported=m->clear_requested=m->settings_unavailable=0;
    if(m->done || m->quit_requested)return 0;
    /* 1862C: fade, background scroll, logo motion, page callback, counter. */
    if(m->fading){m->fade_volume+=m->fade_step;
        if(m->fade_volume<0){m->fade_volume=0;m->fading=0;}
        else if(m->fade_volume>0xf00){m->fade_volume=0xf00;m->fading=0;}}
    if(!(m->counter&7))m->scroll=(uint16_t)((m->scroll+64)&511);
    p=m->page;
    if(p==12 || p==13 || p==SSA_MENU_LEVEL_SELECT || p==SSA_MENU_SETTINGS || p==SSA_MENU_EXTRA){if(m->logo_y>-64)--m->logo_y;}
    else if(p!=10 && m->logo_y<8)++m->logo_y;
    if(m->marker==65535){m->marker=0;if(p==0){m->offset=240;command(m,8);}
        else if(p==SSA_MENU_TITLE && !m->counter)command(m,8);}
    if(p==0 && m->offset)m->offset-=16;
    if(!m->leaving)switch(p){
    case SSA_MENU_LANGUAGE:
        move(m,input,2);
        if(input&SSA_MENU_CONFIRM){command(m,13);m->language=m->selection;page(m,11);command(m,8);m->counter=0;}
        break;
    case SSA_MENU_TITLE:
        if((input&SSA_MENU_START) && m->counter>90){command(m,13);page(m,0);}
        break;
    case SSA_MENU_MAIN:
        move(m,input,4);
        if(input&SSA_MENU_CONFIRM){command(m,13);
            if(!m->selection)page(m,1);
            else if(m->selection==2)page(m,4);
            else if(m->selection==4){page(m,SSA_MENU_QUIT);m->selection=1;}
            else m->unsupported=m->selection==1?3:13;
        }
        break;
    case SSA_MENU_SINGLE:
        move(m,input,2);
        if(input&SSA_MENU_CONFIRM){command(m,13);
            if(!m->selection){m->selecting_level=0;command(m,13);page(m,9);}
            else if(m->selection==1){m->selecting_level=1;m->level_scroll=0;page(m,SSA_MENU_LEVEL_SELECT);}
            else{m->selecting_level=0;memcpy(m->password,"A_______",9);m->password_cursor=0;m->password_typed=0;page(m,SSA_MENU_PASSWORD);}
        }
        if(input&SSA_MENU_BACK){command(m,16);page(m,0);}
        break;
    case SSA_MENU_OPTIONS:{unsigned row;
        /* Keep existing IDs while navigating the visible row order. */
        for(row=0;row<5 && option_ids[row]!=m->selection;++row){}
        m->selection=row<5?row:0;move(m,input,4);m->selection=option_ids[m->selection];
        if(input&(SSA_MENU_CONFIRM|SSA_MENU_LEFT|SSA_MENU_RIGHT)){
            if(m->selection==1){command(m,13);page(m,SSA_MENU_CREDITS);}
            else if(m->selection==2){m->language=(m->language+((input&SSA_MENU_LEFT)?2:1))%3;command(m,13);}
            else if(input&SSA_MENU_CONFIRM){command(m,13);
                if(m->selection==4)page(m,SSA_MENU_SETTINGS);
                else if(m->selection==5)page(m,SSA_MENU_EXTRA);
                else{page(m,SSA_MENU_CLEAR_DATA);m->selection=1;}}}
        if(input&SSA_MENU_BACK){command(m,16);page(m,0);}
        break;}
    case SSA_MENU_SETTINGS:
        move(m,input,5);
        if((input&SSA_MENU_BACK) || (m->selection==5 && (input&SSA_MENU_CONFIRM))){
            command(m,16);page(m,SSA_MENU_OPTIONS);m->selection=4;break;}
        if(input&(SSA_MENU_LEFT|SSA_MENU_RIGHT|SSA_MENU_CONFIRM)){
            if(!m->selection){
                m->settings.display_mode^=1;
                command(m,13);
            }else if(m->selection==1){
                m->settings.renderer^=1;
                command(m,13);
            }else if(m->selection==2){
                unsigned previous=m->settings.music_style,next=previous,attempt;
                for(attempt=0;attempt<SSA_MUSIC_STYLES;++attempt){
                    next=(next+((input&SSA_MENU_LEFT)?SSA_MUSIC_STYLES-1:1))%SSA_MUSIC_STYLES;
                    if(next==SSA_MUSIC_ORIGINAL || (next==SSA_MUSIC_SOUNDFONT && m->soundfont_available) ||
                       (next==SSA_MUSIC_MIDI && m->midi_available))break;
                }
                m->settings.music_style=next;if(next!=previous)command(m,13);
            }else if(m->selection<5){unsigned *v=m->selection==3?&m->settings.music_volume:&m->settings.sound_volume;
                if(input&SSA_MENU_LEFT)*v=*v<5?0:*v-5;else *v=*v>95?100:*v+5;
                command(m,13);}
        }
        break;
    case SSA_MENU_EXTRA:
        move(m,input,4);
        if((input&SSA_MENU_BACK) || (m->selection==4 && (input&SSA_MENU_CONFIRM))){
            command(m,16);page(m,SSA_MENU_OPTIONS);m->selection=5;break;}
        if(input&(SSA_MENU_LEFT|SSA_MENU_RIGHT|SSA_MENU_CONFIRM)){
            if(!m->selection)m->settings.jump_enabled^=1;
            else if(m->selection==1)m->settings.thirdperson_enabled^=1;
            else if(m->selection==2)m->settings.ingame_netricsa^=1;
            else if(m->selection==3)m->settings.alternate_weapons^=1;
            else break;
            command(m,13);
        }
        break;
    case SSA_MENU_DIFFICULTY:
        move(m,input,2);
        if(input&SSA_MENU_CONFIRM){command(m,13);m->difficulty=m->selection;command(m,13);
            if(m->selecting_level)m->loadout=m->difficulty*12+m->level+1;
            else m->level=m->loadout=0;
            start(m);}
        if(input&SSA_MENU_BACK){command(m,16);page(m,m->selecting_level?SSA_MENU_LEVEL_SELECT:SSA_MENU_SINGLE);
            if(m->selecting_level)m->selection=m->level;}
        break;
    case SSA_MENU_LEVEL_SELECT:{unsigned before;
        move(m,input,11);
        before=m->selection;
        if((input&SSA_MENU_LEFT) && m->selection>=SSA_MENU_LEVELS_PER_CHAPTER)m->selection-=SSA_MENU_LEVELS_PER_CHAPTER;
        else if((input&SSA_MENU_RIGHT) && m->selection<SSA_MENU_LEVELS_PER_CHAPTER)m->selection+=SSA_MENU_LEVELS_PER_CHAPTER;
        if(input&(SSA_MENU_LEFT|SSA_MENU_RIGHT))command(m,m->selection==before?17:12);
        m->level_scroll=m->selection/SSA_MENU_LEVELS_PER_CHAPTER*SSA_MENU_LEVELS_PER_CHAPTER;
        if(input&SSA_MENU_CONFIRM){
            if(!unlocked(m,m->selection))command(m,17);
            else if(m->selection>=m->level_limit){command(m,17);m->unsupported=SSA_MENU_UNAVAILABLE_LEVEL+m->selection;}
            else{m->level=m->selection;command(m,13);page(m,SSA_MENU_DIFFICULTY);}
        }
        if(input&SSA_MENU_BACK){command(m,16);page(m,SSA_MENU_SINGLE);m->selection=1;}
        break;}
    case SSA_MENU_QUIT:
        move(m,input,1);
        if(input&(SSA_MENU_LEFT|SSA_MENU_RIGHT))m->selection^=1;
        if(input&(SSA_MENU_CONFIRM|SSA_MENU_BACK)){
            unsigned quit=(input&SSA_MENU_CONFIRM) && !(input&SSA_MENU_BACK) && !m->selection;
            command(m,quit?13:16);
            if(quit)m->quit_requested=1;
            else{page(m,SSA_MENU_MAIN);m->selection=4;}
        }
        break;
    case SSA_MENU_CLEAR_DATA:
        move(m,input,1);
        if(input&(SSA_MENU_LEFT|SSA_MENU_RIGHT))m->selection^=1;
        if(input&(SSA_MENU_CONFIRM|SSA_MENU_BACK)){
            unsigned clear=(input&SSA_MENU_CONFIRM) && !m->selection;
            command(m,clear?13:16);page(m,SSA_MENU_OPTIONS);m->selection=3;
            if(clear){m->language=0;m->completed_levels=0;m->clear_requested=1;}
        }
        break;
    case SSA_MENU_CREDITS:credits_step(m,input,held);break;
    case SSA_MENU_PASSWORD:if(password_step(m,input,held)!=1)return -1;break;
    default:return -1;
    }
    m->done=m->leaving && !m->fading;m->draw_page=p;m->draw_counter=m->counter;
    if(!m->done)++m->counter;
    return 1;
}
static int item(SsaMenu *m,unsigned index,unsigned selection,int y,int offset,unsigned font)
{
    return ssa_map_view_text_centered(&m->canvas,ssa_map_view_text(&m->canvas,index),y,font,
        m->selection==selection?3:4,offset);
}
static void grid_centered(SsaMenu *m,const char *text,int y,unsigned font,unsigned palette)
{
    int x=120-(int)(ssa_map_view_text_width(&m->canvas,text,font)/2);
    (void)ssa_map_view_text_line(&m->canvas,text,((x+4)/8)*8,y,font,palette);
}
static int credits_draw(SsaMenu *m)
{
    uint32_t at=m->credits_offset;int y=m->credits_y/65536;
    if(m->credits_y<0 && m->credits_y%65536)--y;
    if(m->credits_done)return 1;
    if(at>m->credits_end || m->credits_end>=m->canvas.rom_size)return -1;
    while(y<=159){
        char line[32];unsigned n=0,selection=1;
        if(at<m->credits_end && m->canvas.rom[at]=='*'){++at;selection=0;}
        while(at<m->credits_end && m->canvas.rom[at]!='\n'){
            if(n>=sizeof(line)-1)return -1;
            line[n++]=(char)m->canvas.rom[at++];
        }
        line[n]=0;
        /* 179DC passes selection 0/1, but always font 0 to 16748. */
        y=ssa_map_view_text_centered(&m->canvas,line,y,0,m->selection==selection?3:4,0);
        if(at==m->credits_end)break;
        ++at;
    }
    return 1;
}
int ssa_menu_draw(SsaMenu *m,uint16_t *out,size_t pixels)
{
    unsigned x,y,i;int bottom;
    if(!m || !m->canvas.loaded || !out || pixels<38400 || m->language>=3 ||
       (m->draw_page==SSA_MENU_LEVEL_SELECT && (m->selection>=12 || (m->level_scroll!=0 && m->level_scroll!=SSA_MENU_LEVELS_PER_CHAPTER))))return -1;
    m->canvas.language=m->language;m->canvas.glyph_count=0;
    for(y=0;y<160;++y)for(x=0;x<240;++x){
        unsigned sx=(x+m->scroll)&511,ty=y/8,tx=sx/8;
        unsigned tile=(64-(ty/8)*8+(ty&7)*64+tx)&511;
        m->canvas.background[y*240+x]=m->tiles[tile*64+(y&7)*8+(sx&7)];
        /* BG1 is an unscrolled transparent8bpp tile over scrolling BG0.
         * Enable it on the first title/menu visit as well as later visits. */
        if(m->draw_page!=SSA_MENU_LANGUAGE && m->grid[(y&7)*8+(x&7)])
            m->canvas.background[y*240+x]=m->grid[(y&7)*8+(x&7)];
    }
    switch(m->draw_page){
    case SSA_MENU_LANGUAGE:
        (void)item(m,242,5,32,0,0);bottom=64;
        for(i=0;i<3;++i)bottom=item(m,237+i,i,bottom,0,0);
        break;
    case SSA_MENU_TITLE:
        if((m->draw_counter&32) && m->draw_counter>90)(void)item(m,65,0,88,0,0);
        (void)item(m,66,0,124,0,1);break;
    case SSA_MENU_MAIN:
        bottom=74;
        for(i=0;i<5;++i)bottom=ssa_map_view_text_centered(&m->canvas,main_text(m,i),bottom,0,
            m->selection==i?3:4,i&1?-(int)m->offset:(int)m->offset);
        break;
    case SSA_MENU_SINGLE:
        bottom=82;for(i=0;i<3;++i)bottom=ssa_map_view_text_centered(&m->canvas,single_text(m,i),bottom,0,m->selection==i?3:4,0);break;
    case SSA_MENU_OPTIONS:
        bottom=74;
        for(i=0;i<5;++i){char language[64];unsigned id=option_ids[i];const char *s=option_text(m,id,language);
            if(!s)return -1;
            bottom=ssa_map_view_text_centered(&m->canvas,s,bottom,0,m->selection==id?3:4,0);}
        break;
    case SSA_MENU_SETTINGS:break; /* Labels are composed in small batches below. */
    case SSA_MENU_EXTRA:{
        static const char *labels[3][6]={{"Extras","Jump","Third-person","In-game NETRICSA","Alternate weapons","Back"},
            {"Extras","Springen","Dritte Person","NETRICSA im Spiel","Alternative Waffen","Zurueck"},
            {"Extras","Saut","Troisieme personne","NETRICSA en jeu","Armes alternatives","Retour"}};
        static const char *on[3]={"On","Ein","Oui"},*off[3]={"Off","Aus","Non"};
        char line[64];
        grid_centered(m,labels[m->language][0],8,0,3);
        for(i=0;i<4;++i){unsigned value=i==0?m->settings.jump_enabled:i==1?m->settings.thirdperson_enabled:i==2?m->settings.ingame_netricsa:m->settings.alternate_weapons;
            unsigned palette=m->selection==i?3:4;int y=48+(int)i*24;
            snprintf(line,sizeof(line),"%s:",labels[m->language][i+1]);
            (void)ssa_map_view_text_line(&m->canvas,line,PANEL_LEFT,y,1,palette);
            (void)ssa_map_view_text_line(&m->canvas,value?on[m->language]:off[m->language],VALUE_LEFT,y,1,palette);}
        grid_centered(m,labels[m->language][5],152,1,m->selection==4?3:4);
        break;}
    case SSA_MENU_LEVEL_SELECT:{
        static const char *locked[3]={"Locked","Gesperrt","Verrouille"};
        static const char *unavailable[3]={"Not yet available","Noch nicht verfuegbar","Pas encore disponible"};
        static const char *chapters[3][2]={{"Egypt","Rome"},{"Aegypten","Rom"},{"Egypte","Rome"}};
        const char *status=m->selection>=m->level_limit?unavailable[m->language]:!unlocked(m,m->selection)?locked[m->language]:"";
        (void)ssa_map_view_text_centered(&m->canvas,select_text(m->language),8,0,3,0);
        (void)ssa_map_view_text_line(&m->canvas,"<",8,12,1,4);(void)ssa_map_view_text_line(&m->canvas,">",224,12,1,4);
        (void)ssa_map_view_text_centered(&m->canvas,chapters[m->language][m->level_scroll/SSA_MENU_LEVELS_PER_CHAPTER],26,1,3,0);
        (void)ssa_map_view_text_centered(&m->canvas,status,144,1,4,0);break;}
    case SSA_MENU_CLEAR_DATA:case SSA_MENU_QUIT:{
        static const char *warning[3]={"Delete all saved progress\nand settings?","Gespeicherten Fortschritt\nund Einstellungen loeschen?","Effacer la progression\net les reglages sauvegardes?"};
        static const char *quit[3]={"Quit the game?","Spiel beenden?","Quitter le jeu ?"};
        (void)ssa_map_view_text_centered(&m->canvas,m->draw_page==SSA_MENU_QUIT?quit[m->language]:warning[m->language],80,1,4,0);
        for(i=0;i<2;++i)(void)ssa_map_view_text_centered(&m->canvas,answer_text(m->language,!i),112+(int)i*16,0,m->selection==i?3:4,0);
        break;}
    case SSA_MENU_DIFFICULTY:
        bottom=82;for(i=0;i<3;++i)bottom=item(m,243+i,i,bottom,0,0);break;
    case SSA_MENU_CREDITS:if(credits_draw(m)!=1)return -1;break;
    case SSA_MENU_PASSWORD:{unsigned first,last,width;SsaMapGlyph selected;char display[9];const char *s=m->password;
        if(m->password_cursor>(m->password_typed?8:7) || m->password[8])return -1;
        if(m->password_typed){size_t n=strlen(s);memcpy(display,s,n);memset(display+n,'_',8-n);display[8]=0;s=display;}
        bottom=item(m,5,0,82,2,0)+8;first=m->canvas.glyph_count;
        /* Original measures with spacing0, then draws with spacing1. */
        width=ssa_map_view_text_width(&m->canvas,s,0)-(unsigned)strlen(s);
        (void)ssa_map_view_text_line(&m->canvas,s,120-(int)(width/2),bottom,0,2);
        last=m->canvas.glyph_count;
        if(first+m->password_cursor<last && last<128){
            selected=m->canvas.glyphs[first+m->password_cursor];selected.palette=1;
            memmove(m->canvas.glyphs+first+1,m->canvas.glyphs+first,(last-first)*sizeof(selected));
            m->canvas.glyphs[first]=selected;++m->canvas.glyph_count;
        }
        break;}
    default:return -1;
    }
    if(ssa_map_view_compose(&m->canvas,out,pixels)!=1)return -1;
    if(m->draw_page==SSA_MENU_SETTINGS){
        static const char *labels[3][7]={
            {"Settings","Display mode","Renderer","Music style","Music volume","Sounds volume","Back"},
            {"Einstellungen","Anzeige","Renderer","Musikstil","Musiklautstaerke","Effektlautstaerke","Zurueck"},
            {"Reglages","Affichage","Rendu","Style musical","Volume musique","Volume des sons","Retour"}};
        static const char *modes[3][2]={{"Fullscreen","Windowed"},{"Vollbild","Fenster"},{"Plein ecran","Fenetre"}};
        /* Individual rows avoid the original128-glyph budget when localized
         * Settings labels and the new display row are present together. */
        for(i=0;i<7;++i){char line[80];const char *value=NULL;
            unsigned palette=!i || m->selection==i-1?3:4;
            m->canvas.glyph_count=0;
            if(i==1)value=modes[m->language][m->settings.display_mode==SSA_DISPLAY_WINDOWED];
            else if(i==2)value=m->settings.renderer==SSA_RENDER_GBA?"GBA":"Software";
            else if(i==3){value="Original";
                if(m->settings.music_style==SSA_MUSIC_SOUNDFONT && m->soundfont_available)value="SoundFont";
                else if(m->settings.music_style==SSA_MUSIC_MIDI && m->midi_available)value="MIDI";}
            if(value)snprintf(line,sizeof(line),"%s:",labels[m->language][i]);
            else snprintf(line,sizeof(line),"%s",labels[m->language][i]);
            if(!i || i==6)grid_centered(m,line,settings_top[i],i?1:0,palette);
            else (void)ssa_map_view_text_line(&m->canvas,line,PANEL_LEFT,settings_top[i],1,palette);
            if(value)(void)ssa_map_view_text_line(&m->canvas,value,VALUE_LEFT,settings_top[i],1,palette);
            if(i==4 || i==5){snprintf(line,sizeof(line),"%u%%",i==5?m->settings.sound_volume:m->settings.music_volume);
                (void)ssa_map_view_text_line(&m->canvas,line,184,BAR_TOP+(int)(i-4)*BAR_SPACING,1,palette);}
            if(ssa_map_view_overlay(&m->canvas,out,pixels)!=1)return -1;
        }
        for(i=0;i<2;++i){unsigned value=i?m->settings.sound_volume:m->settings.music_volume;
            for(y=0;y<=BAR_HEIGHT;++y)for(x=0;x<=BAR_SPAN;++x){unsigned c;
                if(!y || y==BAR_HEIGHT || !x || x==BAR_SPAN)c=m->selection==i+3?0x03ff:0x4210;
                else c=x*100<=value*BAR_SPAN?0x02bf:0x1084;
                out[(BAR_TOP+i*BAR_SPACING+y)*240+PANEL_LEFT+x]=(uint16_t)c;}}
    }
    if(m->draw_page==SSA_MENU_LEVEL_SELECT){
        static const uint8_t lock[7]={14,10,10,31,27,27,31};
        for(i=m->level_scroll;i<12 && i<m->level_scroll+SSA_MENU_LEVELS_PER_CHAPTER;++i){
            const char *name=ssa_menu_level_name(m,i);unsigned top=SELECT_TOP+(i-m->level_scroll)*SELECT_SPACING;
            if(!name)return -1;
            /* Six localized rows plus header/status can exceed the original
             *128-glyph OAM budget. Overlay each row as a separate font batch. */
            m->canvas.glyph_count=0;
            (void)ssa_map_view_text_centered(&m->canvas,name,(int)top,1,m->selection==i?3:4,0);
            if(ssa_map_view_overlay(&m->canvas,out,pixels)!=1)return -1;
            if(!unlocked(m,i))for(y=0;y<7;++y)for(x=0;x<5;++x)if(lock[y]&(1u<<(4-x)))
                out[(top+y)*240+16+x]=0x4210;
        }
    }
    /* Original persistent three64x64 eight-bit logo objects precede text OAM. */
    for(y=0;y<64 && m->draw_page!=SSA_MENU_LEVEL_SELECT && m->draw_page!=SSA_MENU_SETTINGS && m->draw_page!=SSA_MENU_EXTRA;++y)for(x=0;x<192;++x){int dy=m->logo_y+(int)y;
        unsigned at=(x/64)*4096+((y/8)*8+(x&63)/8)*64+(y&7)*8+(x&7),c=m->logo[at];
        if(c && dy>=0 && dy<160)out[(unsigned)dy*240+x+24]=m->canvas.palette[c];
    }
    for(i=0;i<38400;++i){unsigned c=out[i],r=c&31,g=(c>>5)&31,b=(c>>10)&31,d=((unsigned)m->fade_volume>>8)&15;
        out[i]=(uint16_t)((r-r*d/16)|((g-g*d/16)<<5)|((b-b*d/16)<<10));}
    return 1;
}
