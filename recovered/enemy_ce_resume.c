typedef struct EnemyCeResume {
    u8 pad00[0x30];
    u16 flags;
    u8 pad32[0x0c];
    u8 state;
    u8 pad3f[0x20];
    u8 animation_done;
} EnemyCeResume;
extern s32 sub_08005394(void *,s32,s32);
void sub_08013438(EnemyCeResume *e)
{
    if(e->animation_done) {
        e->state=1;
        e->flags&=0xfffe;
        sub_08005394(e,10,5);
    }
}
