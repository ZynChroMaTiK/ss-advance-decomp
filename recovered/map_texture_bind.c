typedef struct MapTexture {
    u8 name[8];
    u16 width, height;
    s32 index;
} MapTexture;
typedef struct TextureMap {
    u8 pad0[8];
    u16 count;
    u8 pad1[22];
    MapTexture *textures;
} TextureMap;
extern s16 sub_0800B0FC(MapTexture *);
void sub_0800B0C4(TextureMap *map) {
    MapTexture *texture = map->textures;
    u32 i = 0;
    if (i < map->count) {
        do {
            s16 index = sub_0800B0FC(texture);
            texture->index = index;
            if (index == -1)
                texture->index = 0;
            ++i;
            ++texture;
        } while (i < map->count);
    }
}
