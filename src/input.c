/* Behavioral native port of the input routines in the supplied AENP ROM.
 * Validated against the original Thumb routines by tests/test_input_rom.py.
 * ROM identities, addresses and matching work remain in analysis/ and decomp/.
 */
#include "ssa/input.h"

void ssa_input_record(SsaInput *input, uint32_t held)
{
    uint8_t packed = 0;
    if (input->record_count > 0xE0E || !input->record_buffer) return;
    if (held & 0x20) packed = 1;
    else if (held & 0x10) packed = 2;
    if (held & 0x40) packed |= 4;
    else if (held & 0x80) packed |= 8;
    if (held & 1) packed |= 0x10;
    if (held & 2) packed |= 0x20;
    if (held & 0x200) packed |= 0x40;
    if (held & 0x100) packed |= 0x80;
    input->record_buffer[input->record_count++] = packed;
}

uint32_t ssa_input_playback(SsaInput *input)
{
    uint32_t held = 0;
    uint8_t packed;
    if (!input->playback) return 0;
    packed = input->repeat_keys;
    if (packed == 0xFF) {
        input->playback = 0;
        return 0;
    }
    if (packed & 1) held = 0x20;
    else if (packed & 2) held = 0x10;
    if (packed & 4) held |= 0x40;
    else if (packed & 8) held |= 0x80;
    if (packed & 0x10) held |= 1;
    if (packed & 0x20) held |= 2;
    if (packed & 0x40) held |= 0x200;
    if (packed & 0x80) held |= 0x100;
    if (input->repeat_count) {
        --input->repeat_count;
    } else {
        input->repeat_count = *input->playback++;
        input->repeat_keys = *input->playback++;
    }
    return held;
}

void ssa_input_poll(SsaInput *input, uint16_t key_register, int input_suspended,
                    void (*soft_reset)(void *), void *userdata)
{
    uint32_t held = (uint16_t)~key_register;
    uint32_t previous;
    input->raw_keys = held | ((held & ~input->raw_keys) << 16);
    if ((input->raw_keys & 0xFC0F) == 0xFC0F) {
        if (soft_reset) soft_reset(userdata);
    } else {
        input->reset_state = 0;
    }
    previous = input->keys & 0xFFFF;
    if ((input->flags & 2) && !input_suspended) {
        ssa_input_record(input, held);
    } else if ((input->flags & 4) && !input_suspended) {
        held = ssa_input_playback(input);
    }
    input->keys = held | ((held & ~previous) << 16);
}
