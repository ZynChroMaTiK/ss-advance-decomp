#ifndef SSA_NATIVE_GAME_H
#define SSA_NATIVE_GAME_H
/* Campaign maps admitted by the playable frontend; library map loading is separate. */
enum { SSA_NATIVE_CAMPAIGN_LEVELS=12 };
#include "ssa/game_runtime.h"
#include "ssa/weapon_progress.h"
#include "ssa/pc_input_runtime.h"
#include "ssa/enemy_control_runtime.h"
#include "ssa/actor_scene.h"
#include "ssa/render_view.h"
#include "ssa/hud_sprites.h"
#include "ssa/hud_notice.h"
#include "ssa/sprite_cache.h"
#include "ssa/display.h"
#include "ssa/particle_create.h"
#include "ssa/pickup_runtime.h"
#include "ssa/audio.h"
#include "ssa/palette_effect.h"

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
    SsaSpriteCache cache;SsaSpriteOam oam;SsaHudSpriteState hud;SsaHudNotice hud_notice;SsaDisplayRegisters registers;
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
} SsaNativeCarry;
int ssa_native_game_carry_capture(SsaNativeCarry *,const SsaNativeGame *);
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
#endif
