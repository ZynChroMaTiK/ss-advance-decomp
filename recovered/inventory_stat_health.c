typedef struct PickupBody {
    u8 padding[0x32];
    s16 health;
} PickupBody;
typedef struct PickupPlayer {
    u8 padding[0xac];
    PickupBody *body;
} PickupPlayer;

void sub_0800549C(PickupPlayer *player, s16 amount) {
    if (amount == 100) {
        player->body->health += 100;
    } else if (player->body->health < 100) {
        player->body->health += amount;
        if (player->body->health > 100) player->body->health = 100;
    }
}

