typedef struct NoticeGame {
    u32 counter;
    volatile u16 flags:16;
    u8 pad6[0x63];
    u8 length,count,shift;
    u8 pad6c[7];
    u8 y,progress,bonus_y;
    u16 timer,id,value;
    u8 pad7c[0x2a4];
    char text[512];
} NoticeGame;
extern NoticeGame *D_03004010;
extern u16 *D_0300572C;
extern char **D_0844CF34[];
extern int sub_080C56C8(char *,const char *,...);
void sub_0800E914(u16 id,u16 value,u8 y,u8 shift) {
    char *p;
    D_03004010->value=value;
    D_03004010->timer=120;
    D_03004010->y=y;
    D_03004010->id=id;
    D_03004010->shift=shift;
    D_03004010->length=sub_080C56C8(D_03004010->text,D_0844CF34[*D_0300572C][id],value);
    D_03004010->count=0;
    D_03004010->progress=0;
    D_03004010->flags&=0xfdff;
    p=D_03004010->text;
    while(*p){
        if(*p!=' ' && *p!='\n')++D_03004010->count;
        ++p;
    }
}
