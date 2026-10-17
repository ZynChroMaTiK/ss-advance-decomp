#ifndef SSA_INPUT_H
#define SSA_INPUT_H

#include <stdint.h>

/* Native representation of the fields used by ROM routines 08003028,
 * 08003150 and 08003298. Host pointers deliberately have native width.
 * This is not a structure to memcpy to/from a GBA save state. */
typedef struct SsaInput {
    uint32_t raw_keys; /* low 16: held; high 16: newly pressed */
    uint32_t keys;     /* after recording/playback selection */
    uint8_t *record_buffer;
    const uint8_t *playback;
    uint16_t record_count;
    uint8_t repeat_count;
    uint8_t repeat_keys;
    uint8_t reset_state;
    uint8_t flags;     /* 2: record, 4: playback; recording takes priority */
} SsaInput;

/* Recording storage must have at least 0xE10 bytes (including terminator
 * space used by the game's separate recording finalizer). Playback must
 * reference a valid game's RLE stream, terminated by a packed 0xFF value. */
void ssa_input_record(SsaInput *input, uint32_t held);
uint32_t ssa_input_playback(SsaInput *input);

/* key_register uses the original active-low GBA format. Only bits 0..9
 * are physical buttons. Keyboard/mouse action mapping and player camera
 * integration are separate work, not implemented by this module. */
/* input_suspended is bit 0 at offset 4 of the object referenced by
 * 0x03004010. Its broader game-state meaning is not yet established. */
void ssa_input_poll(SsaInput *input, uint16_t key_register, int input_suspended,
                    void (*soft_reset)(void *), void *userdata);

#endif
