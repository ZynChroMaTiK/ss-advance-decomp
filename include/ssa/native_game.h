#ifndef SSA_NATIVE_GAME_H
#define SSA_NATIVE_GAME_H
/* Campaign maps admitted by the playable frontend; library map loading is separate. */
enum { SSA_NATIVE_CAMPAIGN_LEVELS=12 };
#include "ssa/game_runtime.h"
#include "ssa/weapon_progress.h"
#include "ssa/pc_input_runtime.h"
#include "ssa/enemy_control_runtime.h"
#include "ssa/actor_scene.h"
#include "ssa/sprite_shadows.h"
#include "ssa/render_view.h"
#include "ssa/hud_sprites.h"
#include "ssa/hud_notice.h"
#include "ssa/sprite_cache.h"
#include "ssa/display.h"
#include "ssa/particle_create.h"
#include "ssa/pickup_runtime.h"
#include "ssa/audio.h"
#include "ssa/palette_effect.h"
#include "ssa/automap.h"
#include "ssa/classic_hud.h"
#include "ssa/settings.h"
#include "ssa/surface_decals.h"
#include "ssa/water.h"
#include "ssa/godrays_reference.h"

/* Campaign-session dossier discoveries and individual read acknowledgements. */
typedef struct SsaNetricsaJournal {
    uint16_t weapons,enemies,read_weapons,read_enemies;
    uint8_t order[22],order_count; /* Oldest first; IDs1..10 weapons,11..22 enemies. */
} SsaNetricsaJournal;

/* Optional host audio sink. Observe only; called on the simulation thread.
 * Playback/device availability must not alter gameplay sound return values.
 * Context stays alive until unbound or game teardown; NULL unbinds. */
typedef struct SsaNativeAudioOps {
    void (*play)(void *,unsigned);
    void (*stop)(void *,unsigned);
    void *context;
    /* Optional PC spatial services. Snapshot arguments are valid only during
     * the callback. Legacy sinks fall back to ordinary centered play. */
    void (*play_at)(void *,unsigned,const SsaAudioPoint *);
    void (*listen)(void *,const SsaAudioListener *);
    void (*move)(void *,unsigned,const SsaAudioPoint *);
    void (*music)(void *,unsigned command,unsigned level);
    void (*source)(void *,SsaAudioSourceCommand,unsigned song,uint64_t source,const SsaAudioPoint *);
} SsaNativeAudioOps;

/* Native single-player composition. ROM is borrowed for script data and must
 * remain alive. All game/renderer storage stays at this stable address. */
typedef struct SsaNativeGame {
    const uint8_t *rom;size_t rom_size;
    unsigned level,loaded,error_line,finished,restart_requested;
    unsigned music_mode,music_serial;
    unsigned gba_view; /* Level camera/autoaim, with the original cursor projected onto its shot. */
    unsigned vertical_aim; /* Opt-in mouse pitch; GBA rendering always uses original autoaim. */
    unsigned mipmaps; /* Original ROM mip assets; GBA always enables them. */
    unsigned fov_offset; /* Native horizontal FOV at3:2, in degrees above60. */
    unsigned hud_style,hud_suppressed; /* Suppressed only for clean frontend captures. */
    unsigned weapon_bobbing; /* PC first-person weapon/flash motion; default0. */
    int32_t weapon_bob_x,weapon_bob_y; /* Shared nonnegative8.8 offsets for both hands. */
    unsigned pc_weapon_separate; /* Unshifted art for bob overflow beyond3:2. */
    SsaSpriteOam pc_weapon_oam,pc_hud_oam,pc_center_hud_oam;
    unsigned sprite_shadows_enabled;
    uint8_t shadow_colors[4][256];
    unsigned godrays_enabled; /* Independent optional light shafts; default0. */
    unsigned surface_decals_enabled; /* Optional floor/ceiling detail; default0. */
    SsaSurfaceDecals surface_decals;
    unsigned water_reflections; /* Saved Extras feature; default0. */
    SsaWaterScene water_scene;
    SsaWaterRender water_render;
    SsaGodrayReference godrays;
    struct SsaPcRenderer *water_renderer;
    SsaNetricsaJournal journal;
    uint16_t hud_netricsa_ticks; /*2s analysis, then3s inbox travel/hold/return. */
    uint8_t hud_netricsa_repeat; /*0 none,1 legacy frame fallback,2 mixer-owned cue pair. */
    uint32_t high_score; /* Host's saved record; HUD displays max(record,score). */
    int16_t hud_ammo_before[10];
    uint8_t hud_ammo_flash[10]; /* Forty60Hz ticks (~0.67s) after finite ammo use. */
    int16_t hud_health_before;
    uint8_t hud_health_flash; /* Same40-tick palette feedback on health loss. */
    SsaClassicHudText hud_text;
    uint16_t hud_weapon_ticks; /* Horizontal Classic selector:180ticks/3s, hidden by fire. */
    uint8_t hud_weapon_dismissed; /* A buffered shot must not reopen it when switching commits. */
    uint32_t automap_zoom;
    uint8_t automap_scanline;
    SsaMapCollision automap_query,use_query;
    unsigned hud_use_available; /* Read-only presentation cache, refreshed after simulation. */
    uint32_t error_stage,error_entry;unsigned error_slot;
    uint32_t score,sound_serial,last_sound,notice_serial;
    SsaPaletteEffect palette_effect;
    SsaNativeAudioOps audio_ops;
    unsigned audio_weapon_loop; /* Local continuous weapon sound, source1. */
    unsigned previous_weapon; /* Last actually equipped weapon;0 before a change. */
    /* PC variants share the original weapon ID, ammunition and controller.
     * Variant0=original; Colt variant1=dual. No recovered inventory bits reused. */
    struct {
        unsigned enabled,dual_colts,variant,requested_variant,previous_variant;
        unsigned equip_dual,weapon,firing_left,next_left;
        int32_t left_y;
        uint8_t flash_left[100];
        SsaWeaponObject hands[2]; /* Independent original-speed recoil, right then left. */
        /* Clock1/1200s: shot540, hand spacing270, reload1284;
         * dual left starts396later and both finish1680after right starts. */
        uint64_t clock,next_shot,reload_end;
        unsigned rounds[2],reload_phase,reload_dual,shot_serial;
        unsigned shotgun_inserting; /* Edge latch for the shell-insertion cue. */
    } alternates;
    uint8_t player_death_facing; /* Body direction when death animation starts; independent of mouse look. */
    uint64_t audio_entity_sources[100],audio_source_serial;
    uint8_t audio_entity_states[100],audio_entity_types[100],audio_cd_loops[100],audio_movers[25];
    int32_t footstep_x,footstep_y; /* Position before this simulation tick. */
    SsaBody audio_player_before; /* Snapshot before entities, compared before movers. */
    int16_t audio_player_health;
    uint16_t audio_player_flags;
    uint8_t audio_jump_held;
    char notice[128];unsigned notice_ticks;
    SsaMathTables math;
    SsaVisibilityTables visibility_tables;SsaWallRenderTables wall_tables;
    SsaTextureAssets textures;SsaSpriteAssets sprite_assets;SsaSkyAssets sky;
    SsaMapAssets map;SsaMapTextureBindings bindings;SsaWorld world;SsaSlide slide;
    SsaSpawnDefinitions definitions;SsaSpawnPlan plan;SsaLevelRuntime level_runtime;
    SsaPlayerSpawn players[4];SsaPlayerRuntime player;SsaPlayerGameplay gameplay;
    SsaScriptSpawn spawn;SsaCombatDamage damage;SsaCombatRuntime combat;
    SsaEnemyQueries queries;SsaProjectileRuntime projectiles;SsaEnemyControlRuntime enemies;
    int32_t enemy_speeds[12];
    SsaInventoryRules inventory_rules;SsaWeaponRules weapon_rules;
    SsaWeaponAnimations weapon_animations;SsaWeaponLoadouts loadouts;
    SsaWeaponRuntime weapon;SsaWeaponGame weapon_game;
    SsaActorAnimations actor_animations;SsaActorAnimation player_animation;
    SsaActorAnimationRuntime actor_runtime;
    SsaPickupRespawnData pickup_data;SsaPickupRespawnRuntime pickup_respawn;
    SsaPickupRuntime pickup;SsaPickupRuntimeOps touch_ops;
    SsaParticleRules particle_rules;SsaWeaponImpactState impact;
    SsaEntityRuntime entities;SsaFrame frame;SsaFrameGame frame_game;SsaGameRuntime game;
    SsaPcInput input;SsaPcInputRuntime input_runtime;
    SsaWeaponObject sprite_objects[84];SsaSpriteDispatch sprites;SsaSpritePool sprite_pool;
    /* Original layers: center, score/clock, lives, status, weapon icon, ammo. */
    SsaSpriteCache cache;SsaSpriteOam oam,original_hud_oam[6];SsaHudSpriteState hud;SsaHudNotice hud_notice;SsaDisplayRegisters registers;
    SsaWorldScene world_scene;SsaActorAssets actor_assets;SsaActorScene actor_scene;
    SsaRenderView render_view;SsaActorSource sources[100];
    int32_t source_z_offsets[100]; /* Presentation only, indexed like sources. */
    SsaRenderCamera pc_camera;int32_t render_pitch;
    SsaViewPose pc_pose;SsaWeaponAim camera_trace;
    uint16_t rgb555[SSA_DISPLAY_PIXELS];uint32_t rgb888[SSA_DISPLAY_PIXELS];
    SsaCombatRuntimeOps combat_ops;SsaLevelPlayerOps player_ops;SsaCombatCdOps cd_ops;
    SsaEnemyControlRuntimeOps enemy_ops;SsaProjectileRuntimeOps projectile_ops;
    SsaEntityRuntimeOps entity_ops;SsaScriptSpawnOps script_ops;SsaSectorMoverOps mover_ops;
    SsaPickupRespawnRuntimeOps pickup_ops;SsaWeaponRuntimeOps weapon_ops;
    SsaSpriteDispatchOps sprite_ops;SsaGameRuntimeOps game_ops;
} SsaNativeGame;
/* PC campaign handoff: values only, never world/actor pointers or transient
 * weapon effects. Capture at an idle boundary; apply before the first tick. */
typedef struct SsaNativeCarry {
    SsaInventory inventory;
    uint32_t score;
    unsigned weapon,third_person,previous_weapon;
    unsigned alternate_weapons,dual_colts,weapon_variant,previous_variant;
    unsigned colt_rounds[2]; /* Shots used in each six-round PC magazine. */
    SsaNetricsaJournal journal;
} SsaNativeCarry;
/* Read-only discovery snapshot; enemy spawns/hits alone do not unlock dossiers. */
int ssa_native_game_netricsa_entries(SsaNetricsaJournal *,const SsaNativeGame *);
int ssa_native_game_netricsa_unread(const SsaNativeGame *);
/* Newest unread discovery:0 none,1..10 weapon,11..22 enemy,-1 error. Read-only. */
int ssa_native_game_netricsa_latest(const SsaNativeGame *);
int ssa_native_game_netricsa_read(SsaNativeGame *,unsigned weapons,unsigned enemies);
int ssa_native_game_carry_capture(SsaNativeCarry *,const SsaNativeGame *);
/* PC M toggles the original flag8 automap, gated by Extras. Zoom is4096..65536; +1 enlarges,
 * -1 shrinks by the original one-tick1/16 step. No world update is performed. */
int ssa_native_game_automap(SsaNativeGame *,unsigned enabled);
int ssa_native_game_hud_style(SsaNativeGame *,unsigned style);
/* Read-only indexed Classic layer; also used by full-resolution composition. */
int ssa_native_game_classic_hud(const SsaNativeGame *,uint8_t *,size_t);
/* Same foreground plus an optional black-backing mask; both buffers240x160. */
int ssa_native_game_classic_hud_layers(const SsaNativeGame *,uint8_t *,uint8_t *,size_t);
int ssa_native_game_automap_zoom(SsaNativeGame *,int steps);
/* Read-only ongoing-campaign1231C projection (setting0). Returns equipped and
 * requested guns separately and per-weapon ownership/ammo/shot/hit rows.
 * No cross-level counter accumulation or original score screen is implied. */
int ssa_native_game_weapon_progress(SsaWeaponProgress *,SsaWeaponProgressStats [10],
    const SsaNativeGame *);
/* Original next-level construction resets health/protection and pending pickups,
 * then123C4 restores equipped weapons/ammo. PC history, score and H view persist.
 * The exact capture/apply pair remains available for entry-state restart. */
int ssa_native_game_carry_next(SsaNativeCarry *,const SsaNativeGame *);
int ssa_native_game_carry_apply(SsaNativeGame *,const SsaNativeCarry *);
/* Returns1/-1. Zero-init before init. Free also accepts partial initialization. */
int ssa_native_game_init(SsaNativeGame *,const uint8_t *,size_t,unsigned level);
/* Original password settings1..36 grant their ROM loadout/difficulty.
 * Zero retains ordinary new-game initialization. Nonzero must name this level. */
int ssa_native_game_init_loadout(SsaNativeGame *,const uint8_t *,size_t,unsigned level,unsigned setting);
/* Without prior progress: original New Game setting0 on map0; the ROM's
 * level/difficulty starter row on later maps. With progress: retain its complete
 * inventory, selection, history, score and H view, including carried weapons.
 * Passwords retain their explicit init_loadout setting, including map0. */
int ssa_native_game_init_campaign(SsaNativeGame *,const uint8_t *,size_t,unsigned level,
    unsigned difficulty,const SsaNativeCarry *progress);
void ssa_native_game_free(SsaNativeGame *);
int ssa_native_game_bind_audio(SsaNativeGame *,const SsaNativeAudioOps *);
/* One original tick at60Hz, full recovered simulation and presentation. */
int ssa_native_game_step(SsaNativeGame *);
/* PC frontend: advance simulation without drawing the GBA reference at every
 * catch-up tick. Prepare camera/actor snapshots once per presented batch. */
int ssa_native_game_tick(SsaNativeGame *);
int ssa_native_game_prepare_view(SsaNativeGame *);
int ssa_native_game_draw(SsaNativeGame *);
/* PC extension: select an owned weapon, retaining original lower/raise. */
int ssa_native_game_select_weapon(SsaNativeGame *,unsigned weapon);
/* PC wheel/relative selection. Positive steps select higher IDs, negative lower;
 * wrap and skip unowned/empty weapons. Start from the requested weapon so rapid
 * events work during lowering/raising.1changed,0unchanged,-1invalid/busy. */
int ssa_native_game_cycle_weapon(SsaNativeGame *,int steps);
/* PC last-used selection.0 when absent/unusable,1queued,-1invalid/busy. */
int ssa_native_game_previous_weapon(SsaNativeGame *);
enum { SSA_ALTERNATE_RETAIN=0,SSA_ALTERNATE_NEW_GAME=1,SSA_ALTERNATE_LOADOUT=2 };
/* PC Extras policy. NEW_GAME grants/equips one Colt. LOADOUT upgrades an
 * existing Colt to dual outside map0, without changing gear/ammo/selected ID.
 * RETAIN preserves earned variant ownership during campaign carry/restart. */
int ssa_native_game_alternate_weapons(SsaNativeGame *,unsigned enabled,unsigned start_mode);
/* Same OAM service with the PC alternate-hand presentation applied. Also accepts
 * filtered sprite snapshots used by pause/NETRICSA; does not advance gameplay. */
int ssa_native_game_build_oam(SsaNativeGame *,SsaSpriteOam *,SsaSpriteDispatch *);
struct SsaSettings;
/* Apply runtime settings at both level entry and live option changes. */
int ssa_native_game_apply_settings(SsaNativeGame *,const struct SsaSettings *);
#endif
