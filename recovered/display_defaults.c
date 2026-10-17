struct DisplayState {
    void *pixels;
    unsigned char page, counter;
    unsigned short unused;
    int pa, pb, pc, pd, x, y;
};
extern struct DisplayState D_030000C0;
void sub_080074F0(void)
{
    D_030000C0.x = 0;
    D_030000C0.y = 0;
    D_030000C0.pa = 128;
    D_030000C0.pb = 0;
    D_030000C0.pc = 0;
    D_030000C0.pd = 192;
}
