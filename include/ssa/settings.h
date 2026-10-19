#ifndef SSA_SETTINGS_H
#define SSA_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
enum {SSA_SETTINGS_BYTES=16};
enum {SSA_MUSIC_ORIGINAL,SSA_MUSIC_SOUNDFONT,SSA_MUSIC_MIDI,SSA_MUSIC_STYLES};
enum {SSA_RENDER_SOFTWARE,SSA_RENDER_GBA};
enum {SSA_DISPLAY_FULLSCREEN,SSA_DISPLAY_WINDOWED};
enum {SSA_HUD_ORIGINAL,SSA_HUD_CLASSIC};
enum {SSA_ASPECT_NATIVE,SSA_ASPECT_GBA};
/* One field catalogue for storage, range checks and comparisons. The explicit
 * versioned disk encoder below remains independent of native struct layout. */
#define SSA_SETTINGS_FIELDS(X) \
 X(music_style,SSA_MUSIC_STYLES-1) X(music_volume,100) X(sound_volume,100) \
 X(renderer,SSA_RENDER_GBA) X(jump_enabled,1) X(ingame_netricsa,1) \
 X(thirdperson_enabled,1) X(display_mode,SSA_DISPLAY_WINDOWED) X(alternate_weapons,1) \
 X(automap_enabled,1) X(hud_style,SSA_HUD_CLASSIC) X(aspect_ratio,SSA_ASPECT_GBA) \
 X(weapon_bobbing,1) X(surface_decals,1) X(water_reflections,1)
#define SSA_SETTINGS_DECLARE(name,max) unsigned name;
typedef struct SsaSettings {SSA_SETTINGS_FIELDS(SSA_SETTINGS_DECLARE)} SsaSettings;
#undef SSA_SETTINGS_DECLARE
#define SSA_SETTINGS_DEFAULT {0,100,100,SSA_RENDER_SOFTWARE,0,1,1,SSA_DISPLAY_FULLSCREEN,0,0,SSA_HUD_ORIGINAL,SSA_ASPECT_NATIVE,0,0,0}
int ssa_settings_encode(const SsaSettings *,uint8_t *,size_t);
int ssa_settings_decode(SsaSettings *,const uint8_t *,size_t);
int ssa_settings_valid(const SsaSettings *);
int ssa_settings_equal(const SsaSettings *,const SsaSettings *);
#endif
