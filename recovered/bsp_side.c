int sub_0800B1B0(s16 *point, s32 *origin, s32 *delta) {
    s32 x=point[1], y=point[3];
    s32 dx=delta[0], dy=delta[1];
    s32 ox=origin[0], oy=origin[1];
    u32 a,b;
    if (!dx) {
        if (x<=ox) return dy>0;
        return (u32)dy>>31;
    }
    if (!dy) {
        if (y<=oy) return (u32)dx>>31;
        return dx>0;
    }
    x-=ox; y-=oy;
    a=(u32)(dy^x)>>31;
    b=(u32)(dx^y)>>31;
    if (a!=b) {
        if (a) goto one;
        goto zero;
    }
    if (y*dx>=dy*x) {
one:
        return 1;
    }
zero:
    return 0;
}

