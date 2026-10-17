typedef struct RenderPlayer {
    u8 pad0[0xA4];
    u32 keys,zoom;
    u8 pad1[0x10];
    u16 flags;
    u8 pad2[0x42];
} RenderPlayer;
typedef struct RenderGame {u8 pad[7];u8 ticks;} RenderGame;
typedef struct RenderLocal {u16 unused,index;} RenderLocal;
typedef struct RenderCamera {
    s32 x,y,z;
    u16 heading,unused;
    s32 sine,cosine;
} RenderCamera;
extern RenderPlayer *D_03004648;
extern RenderGame *D_03004010;
extern RenderLocal D_0300464C;
extern RenderCamera D_03004020;
extern void *D_03004930;
extern u16 D_03004180;
extern void sub_08007C94(u8);
extern void sub_08008420(u32);
extern void sub_080094E8(u16,s32 *,s32 *);
extern void sub_0800B6B0(void *);
extern void sub_08002CA0(void);
void sub_08002B88(void) {
    RenderPlayer *p,*selected;
    u32 i,amount;
    u16 flags;
    sub_08007C94(0);
    p=D_03004648;
    i=0;
    do {
        flags=p->flags;
        if(4&flags) {
          if(8&flags) {
            if(p->keys&1) {
                amount=D_03004010->ticks;
                amount*=p->zoom>>4;
                p->zoom+=amount;
                if(p->zoom>0x10000) p->zoom=0x10000;
            }
            if(p->keys&2) {
                amount=D_03004010->ticks;
                amount*=p->zoom>>4;
                p->zoom-=amount;
                if(p->zoom<0x1000) p->zoom=0x1000;
            }
          }
        }
        ++i;
        ++p;
    } while(i<=3);
    selected=&D_03004648[D_0300464C.index];
    if(selected->flags&8) {
        sub_08008420(selected->zoom);
    } else {
        sub_080094E8((u16)-D_03004020.heading,&D_03004020.sine,&D_03004020.cosine);
        sub_0800B6B0(D_03004930);
        if(D_03004180) sub_08002CA0();
    }
}
