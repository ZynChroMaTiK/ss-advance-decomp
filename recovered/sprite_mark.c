typedef struct SpriteMarkObject {
    u8 padding[28];
    u32 flags;
    u8 tail[16];
} SpriteMarkObject;
extern u8 *D_0300559C;
extern u32 *D_030055C0;
extern volatile u32 D_030055C4;

void sub_0800FB5C(void)
{
    u32 *cursor = D_030055C0;
    u32 i;
    for (i = 0; i < D_030055C4; ++i) {
        u32 offset = *cursor++ * 48;
        SpriteMarkObject *object = (SpriteMarkObject *)(D_0300559C + offset);
        object->flags |= 0x04000000;
    }
}
