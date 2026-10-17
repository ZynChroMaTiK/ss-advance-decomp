extern u32 D_030054E0[];
extern u32 D_03005588;
void sub_0800E320(u32 source, u32 count, u32 tile) {
    u32 *entry = D_030054E0;
    while (*entry) entry += 3;
    *entry++ = source;
    *entry++ = tile;
    *entry++ = count;
    *entry = 0;
    D_03005588 -= count << 6;
}
