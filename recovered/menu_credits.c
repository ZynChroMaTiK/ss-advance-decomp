/* Matching candidate for 179DC; portable implementation is src/menu.c. */
typedef struct CreditsMenu {
    u16 language,unused2,page,selection,marker;
    u8 pad0a[15],leaving;
} CreditsMenu;
typedef union CreditsPosition {
    s32 value;
    struct { u16 fraction; s16 whole; } parts;
} CreditsPosition;
extern CreditsMenu *D_0300572C;
extern CreditsPosition D_03006DD4;
extern u8 *D_03006DD8;
extern u16 D_03006DDC,D_03006DDE;
extern s32 D_03006DE0;
extern u32 D_030042F0;
extern u8 **D_0844CF34[];
extern void sub_08003028(void);
extern void sub_08008EB0(s32);
extern u16 sub_08016748(u8 *,s32,s16,s32,s32);

s32 sub_080179DC(void)
{
    u8 buffer[32];
    u8 *source,*dest;
    u16 y;
    s32 selection;
    u32 input;
    if(D_0300572C->marker==0xffff){
        D_0300572C->marker=0;
        D_03006DD4.value=160<<16;
        D_03006DDC=0;
        D_03006DDE=0;
        D_03006DD8=D_0844CF34[D_0300572C->language][246];
    }
    if(D_03006DDC==0){
        sub_08003028();
        input=D_030042F0;
        if(input&0x40)D_03006DE0=0;
        else if(input&0x80)D_03006DE0=0x20000;
        else D_03006DE0=0x8000;
        if(input&0x30000){sub_08008EB0(16);++D_03006DDC;}
    }else{
        D_0300572C->page=4;
        D_0300572C->marker|=0xffff;
        D_0300572C->selection=0;
    }
    if(D_03006DDC==0){
        D_03006DD4.value-=D_03006DE0;
        if(D_03006DD4.value<=-0x100000){
            D_03006DD4.value+=0x100000;
            while(*D_03006DD8 && *D_03006DD8!=10)++D_03006DD8;
            if(!*D_03006DD8){
                ++D_03006DDC;
                D_0300572C->page=4;
                D_0300572C->marker=0xffff;
                D_0300572C->selection=0;
            }else{++D_03006DD8;++D_03006DDE;}
        }
        source=D_03006DD8;
        if(source){
            y=D_03006DD4.parts.whole;
            while((s16)y<=159){
                dest=buffer;selection=1;
                if(*source=='*'){++source;selection=0;}
                while(*source && *source!=10)*dest++=*source++;
                *dest=0;
                y=sub_08016748(buffer,selection,(s16)y,0,0);
                if(!*source)break;
                ++source;
            }
        }
    }
    if(D_0300572C->leaving)return 0;
    return 1;
}
