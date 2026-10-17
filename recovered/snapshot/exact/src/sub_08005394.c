#include "common.h"

typedef struct ActorAnimationEntity {
    u8 pad[0x58];
    u16 animation;
    u16 display_frame;
    u8 index;
    u8 elapsed;
    u8 period;
    u8 done;
} ActorAnimationEntity;

void sub_08005394(ActorAnimationEntity *entity, u16 animation, u16 period) {
    if (animation <= 0x8D || animation == 0xFFFF) {
        entity->animation = animation;
        entity->period = period;
        entity->index = 0;
        entity->elapsed = 0;
        entity->done = 0;
    }
}
