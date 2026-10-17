/* Matching candidate for AENP 0x080094E8; types come from context.h. */
extern volatile u16 D_0801C930[];
void sub_080094E8(u16 heading, s32 *sine, s32 *cosine)
{
    s32 s = 0;
    s32 c = 0;
    u32 quadrant = heading >> 14;
    u32 phase = heading & 0x3FFF;
    if (phase == 0) {
        switch (quadrant) {
        case 0: s = 0; c = 0x10000; break;
        case 1: s = 0x10000; break;
        case 2: s = 0; c = -0x10000; break;
        case 3: s = -0x10000; break;
        }
    } else {
        switch (quadrant) {
        case 0: s = D_0801C930[phase]; c = D_0801C930[0x4000-phase]; break;
        case 1: s = D_0801C930[0x4000-phase]; c = -D_0801C930[phase]; break;
        case 2: s = -D_0801C930[phase]; c = -D_0801C930[0x4000-phase]; break;
        case 3: s = -D_0801C930[0x4000-phase]; c = D_0801C930[phase]; break;
        }
    }
    if (sine) *sine = s;
    if (cosine) *cosine = c;
}


