typedef struct PlaneWorld {
    u8 pad[0x60];
    void *start,*end;
    u8 more[0x340-0x68];
    void *opening;
} PlaneWorld;
extern PlaneWorld D_03004930;
void sub_0800A870(void)
{
    D_03004930.end=D_03004930.start;
    D_03004930.opening=(u8 *)&D_03004930+0x70;
}
