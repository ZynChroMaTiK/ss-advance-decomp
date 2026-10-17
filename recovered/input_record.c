/* Candidate for ROM 0x08003150. Match status is recorded by decomp.wtf. */
typedef struct {
    u32 raw_keys;
    u32 keys;
    u32 saved_state;
    u8 *record_buffer;
    u8 *playback;
    u16 state;
    u8 unknown_16[8];
    u16 record_count;
    u8 repeat_count;
    u8 repeat_keys;
    u8 reset_state;
    u8 flags;
} InputState;

extern InputState D_030042F0;

void sub_08003150(s32 keys)
{
    InputState *input = &D_030042F0;
    u8 packed;

    if ((u32)D_030042F0.record_count <= 0xE0EU) {
        packed = 0;
        if (keys & 0x20) packed = 1;
        else if (keys & 0x10) packed = 2;
        if (keys & 0x40) packed |= 4;
        else if (keys & 0x80) packed |= 8;
        if (keys & 1) packed |= 0x10;
        if (keys & 2) packed |= 0x20;
        if (keys & 0x200) packed |= 0x40;
        if (keys & 0x100) packed |= 0x80;
        D_030042F0.record_buffer[D_030042F0.record_count++] = packed;
    }
}

