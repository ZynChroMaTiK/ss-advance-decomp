#include "ssa/sprite_assets.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    SsaSpriteAssets assets = {0};
    FILE *file;
    long length;
    uint8_t *rom;
    unsigned i, assertions = 0;
    assert(argc == 2);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); rom = malloc((size_t)length); assert(rom);
    assert(fread(rom, 1, (size_t)length, file) == (size_t)length); fclose(file);
    assert(ssa_sprite_assets_load(&assets, rom, (size_t)length) == 1); free(rom);
    for (i = 0; i < SSA_SPRITE_IMAGE_COUNT; ++i) {
        SsaSpriteImage *im = &assets.images[i];
        size_t stride = im->width + 9u, size = stride * im->height;
        uint8_t *pixels = malloc(size), *saved = malloc(size);
        uint8_t width = im->parts[0].width;
        unsigned y;
        assert(pixels && saved); memset(pixels, 0xA5, size);
        assert(ssa_sprite_image_indices(im, pixels, stride, size) == 1); ++assertions;
        memcpy(saved, pixels, size);
        for (y = 0; y < im->height; ++y) {
            unsigned x;
            for (x = im->width; x < stride; ++x)
                assert(pixels[(size_t)y * stride + x] == 0xA5);
        }
        im->parts[0].width = 0;
        assert(ssa_sprite_image_indices(im, pixels, stride, size) == -1);
        assert(memcmp(saved, pixels, size) == 0); assertions += 2;
        im->parts[0].width = width; ++im->min_x;
        assert(ssa_sprite_image_indices(im, pixels, stride, size) == -1);
        assert(memcmp(saved, pixels, size) == 0); assertions += 2; --im->min_x;
        assert(ssa_sprite_image_indices(im, pixels, im->width,
            (size_t)im->width * im->height - 1u) == -1);
        assert(memcmp(saved, pixels, size) == 0); assertions += 2;
        assert(ssa_sprite_image_indices(im, NULL, stride, size) == -1); ++assertions;
        free(pixels); free(saved);
    }
    ssa_sprite_assets_free(&assets); ssa_sprite_assets_free(&assets);
    assert(assets.loaded == 0);
    printf("{\"sanitizer_assertions\":%u,\"decoded_images\":%u}\n",
        assertions, (unsigned)SSA_SPRITE_IMAGE_COUNT);
    return 0;
}
