#ifndef SSA_SETTINGS_H
#define SSA_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
enum {SSA_SETTINGS_BYTES=16};
enum {SSA_MUSIC_ORIGINAL,SSA_MUSIC_SOUNDFONT,SSA_MUSIC_MIDI,SSA_MUSIC_STYLES};
enum {SSA_RENDER_SOFTWARE,SSA_RENDER_GBA};
enum {SSA_DISPLAY_FULLSCREEN,SSA_DISPLAY_WINDOWED};
typedef struct SsaSettings {unsigned music_style,music_volume,sound_volume,renderer,jump_enabled,ingame_netricsa,thirdperson_enabled,display_mode,alternate_weapons;} SsaSettings;
#define SSA_SETTINGS_DEFAULT {0,100,100,SSA_RENDER_SOFTWARE,0,1,1,SSA_DISPLAY_FULLSCREEN,0}
int ssa_settings_encode(const SsaSettings *,uint8_t *,size_t);
int ssa_settings_decode(SsaSettings *,const uint8_t *,size_t);
#endif
