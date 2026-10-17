#include "common.h"

typedef struct WeaponSprite {
    u8 pad0[32];
    u16 animation;
    u16 frame;
    u8 period;
    u8 elapsed;
    u8 done;
} WeaponSprite;

void sub_0800FC04(WeaponSprite *object, u16 animation, u16 period) {
    if (animation <= 0xE8 || animation == 0xFFFF) {
        object->animation = animation;
        object->period = period;
        object->frame = 0;
        object->elapsed = 0;
        object->done = 0;
    }
}
