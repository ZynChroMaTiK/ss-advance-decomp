/* Matching candidate for16BE4. Portable host implementation: src/menu.c. */
typedef struct PasswordMenu {
    u16 language,unused2,page,selection,marker,offset,counter,scroll,level;
    u8 pad12[4],input_state,pad17[2],leaving,difficulty,new_game;
    char password[9];
    s8 cursor;
    u8 god,loadout,pad28[2],up,down,music;
} PasswordMenu;
typedef struct PasswordGame {
    u8 pad00[16],player_count,mode,pad12[0x56],local_player,pad69[0x13],continues,pad7d[11];
    u32 score;
} PasswordGame;
extern PasswordMenu *D_0300572C;
extern PasswordGame *D_03004010;
extern u32 D_030042F0;
extern char *D_0844C1D4[];
extern void sub_08003028(void);
extern void sub_08008EB0(s32);
extern void sub_08008E74(s32);
extern void sub_08015614(s32,s32,s32);
extern s32 sub_080C5708(char *,char *);
extern s32 sub_080C556C(s32,s32);
extern s32 sub_080C54F4(s32,s32);
extern s16 sub_080168E4(s32,s32,s16,s16,s32);
extern void sub_0801892C(char *,s16 *,s16 *,s16,s32);
extern void sub_080187C0(char *,s16,s16,s16,s16,s16,u16);
s32 sub_08016BE4(void)
{
    s16 width,height,y;
    u32 input,index;
    PasswordMenu *menu;
    char *c;
    menu=D_0300572C;
    if(menu->marker==0xffff)menu->marker=0;
    if(!D_0300572C->leaving){
        sub_08003028();input=D_030042F0;
        if(input&0x100000){
            ++D_0300572C->cursor;
            if(D_0300572C->cursor>7)D_0300572C->cursor=0;
        }else if(input&0x200000){
            --D_0300572C->cursor;
            if(D_0300572C->cursor<0)D_0300572C->cursor=7;
        }
        if(input&0x80){
            if(D_0300572C->down<255)++D_0300572C->down;
            else D_0300572C->down=32;
        }else D_0300572C->down=0;
        if(input&0x40){
            if(D_0300572C->up<255)++D_0300572C->up;
            else D_0300572C->up=32;
        }else D_0300572C->up=0;
        if((input&0x800000) || ((input&0x80) && D_0300572C->down>30 && !(D_0300572C->down&3))){
            c=D_0300572C->password+D_0300572C->cursor;
            if(*c=='A')*c='_';else if(*c=='0')*c='Z';else if(*c=='_')*c='9';else --*c;
        }else if((input&0x400000) || ((input&0x40) && D_0300572C->up>30 && !(D_0300572C->up&3))){
            c=D_0300572C->password+D_0300572C->cursor;
            if(*c=='Z')*c='0';else if(*c=='9')*c='_';else if(*c=='_')*c='A';else ++*c;
        }
        if(input&0x10000){
            for(index=0;index<36;++index)if(!sub_080C5708(D_0300572C->password,D_0844C1D4[index]))break;
            if(index==36)sub_08008EB0(17);
            else{
                menu=D_0300572C;menu->level=sub_080C556C(index,12);menu->difficulty=sub_080C54F4(index,12);
                D_0300572C->loadout=index+1;D_0300572C->input_state=0;
                sub_08008EB0(13);sub_08015614(0,64,31);
                if(D_0300572C->music)sub_08008E74(5);
                D_0300572C->leaving=1;D_0300572C->new_game=1;D_03004010->player_count=1;
                if(D_0300572C->difficulty==0)D_03004010->continues=5;
                else if(D_0300572C->difficulty==1)D_03004010->continues=3;
                else D_03004010->continues=2;
                if(!D_03004010->continues)D_03004010->continues=1;
                D_03004010->score=0;D_03004010->mode=0;D_03004010->local_player=0;
            }
        }
        if(input&0x20000){
            sub_08008EB0(16);menu=D_0300572C;menu->page=1;menu->marker=0xffff;menu->selection=0;
        }
    }
    y=sub_080168E4(5,0,82,2,0)+8;
    sub_0801892C(D_0300572C->password,&width,&height,0,0);
    sub_080187C0(D_0300572C->password,120-(width>>1),y,1,1,0,D_0300572C->cursor+1);
    sub_080187C0(D_0300572C->password,120-(width>>1),y,1,2,0,0);
    return D_0300572C->leaving?0:1;
}
