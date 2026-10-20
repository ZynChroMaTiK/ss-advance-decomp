#ifndef SSA_MENU_H
#define SSA_MENU_H
#include "ssa/map_view.h"
#include "ssa/settings.h"
#include "ssa/scores.h"
enum { SSA_MENU_UP=1,SSA_MENU_DOWN=2,SSA_MENU_CONFIRM=4,SSA_MENU_BACK=8,
       SSA_MENU_START=16,SSA_MENU_LEFT=32,SSA_MENU_RIGHT=64 };
enum { SSA_MENU_LANGUAGE=10,SSA_MENU_TITLE=11,SSA_MENU_MAIN=0,
       SSA_MENU_SINGLE=1,SSA_MENU_OPTIONS=4,SSA_MENU_PASSWORD=8,SSA_MENU_DIFFICULTY=9,SSA_MENU_CREDITS=12,
       SSA_MENU_SCORES=13,SSA_MENU_LEVEL_SELECT=14,SSA_MENU_CLEAR_DATA=15,SSA_MENU_SETTINGS=16,SSA_MENU_EXTRA=17,
       SSA_MENU_QUIT=18,SSA_MENU_SCORE_NAME=19,SSA_MENU_GRAPHICS=20 };
enum { SSA_MENU_UNAVAILABLE_LEVEL=100 };
enum { SSA_MENU_LEVELS_PER_CHAPTER=6 };
typedef struct SsaMenu {
    SsaMapView canvas;
    uint8_t tiles[32768],logo[12288];
    unsigned page,selection,marker,offset,language,difficulty,music_enabled;
    uint16_t counter,scroll;
    int logo_y,fade_volume,fade_step;
    unsigned fading,leaving,done,unsupported;
    unsigned commands[4],command_count,fade_audio;
    unsigned draw_page;
    uint16_t draw_counter;
    uint32_t credits_offset,credits_end;
    int32_t credits_y,credits_speed;
    uint16_t credits_done,credits_lines;
    unsigned level,loadout,level_limit;
    char password[9];
    uint8_t password_cursor,password_up,password_down;
    uint8_t password_typed;
    uint8_t grid[64]; /* Original BG1 tile080AAEEC, repeated over BG0. */
    uint16_t completed_levels;
    unsigned level_scroll,selecting_level,clear_requested; /* level_scroll: chapter start0/6. */
    SsaSettings settings;
    unsigned soundfont_available,midi_available,settings_unavailable,vulkan_available;
    unsigned quit_requested; /* PC Main-menu Quit; never a new-game result. */
    int32_t score_y,score_speed;
    unsigned score_ticks,score_leaving;
    SsaScores scores;
    uint32_t score_candidate;
    unsigned score_rank,score_highlight,score_submitted;
    uint8_t score_cursor,score_up,score_down,score_typed;
    unsigned score_length;
} SsaMenu;
/* Starts at Title in English. Settings cycles Language in place (Left backwards,
 * Right/Confirm forwards), using original language names and localized labels.
 * Options IDs1/3/4/5/6: Credits, Clear Data, Settings, Extras, Graphics.
 * Settings owns Language, FOV and audio; Graphics owns Display mode, Renderer
 * and Aspect ratio. Extras includes jump, third-person, in-game NETRICSA,
 * alternate weapons, automap, HUD style, first-person weapon bobbing and surface decals.
 * No separate music toggle. Original title/main/
 * single-player/difficulty/credits/Scores/name-entry pages. The frontend owns
 * score-file persistence. Other
 * menu destinations are explicit unsupported boundaries, not fake features. */
int ssa_menu_init(SsaMenu *,const uint8_t *,size_t,unsigned music_enabled);
int ssa_menu_step(SsaMenu *,unsigned pressed);
/* Held input is distinct from navigation edges: credits Up pauses, Down speeds up. */
int ssa_menu_step_input(SsaMenu *,unsigned pressed,unsigned held);
/* Native PC extensions. Coordinates use the original 240x160 menu canvas.
 * Hover changes selection; click returns the input to submit on the next step.
 * Outside labels never confirms. Title/credits/password keep click-to-confirm. */
unsigned ssa_menu_pointer(SsaMenu *,int x,int y,unsigned click);
int ssa_menu_text(SsaMenu *,unsigned character);
/* Post-game name entry. Caller copies its persisted table into scores first.
 * Submitted record raises score_submitted for one tick; caller stores only
 * that event, then may merge the resulting latest table back into scores. */
int ssa_menu_score_begin(SsaMenu *,uint32_t score);
/* Shared letterbox mapping and bounded ASCII editor for future UI screens.
 * Text accepts letters/digits/underscore/space, uppercases letters, and handles
 * Backspace(8), Delete(127). cursor may point at the trailing NUL. */
int ssa_ui_viewport(int w,int h,int rw,int rh,int *x,int *y,int *ow,int *oh);
int ssa_ui_pointer(int w,int h,int rw,int rh,int x,int y,int *out_x,int *out_y);
int ssa_ui_text(char *,size_t capacity,uint8_t *cursor,unsigned character);
/* Original36 password strings, borrowed from ROM. NULL on malformed data. */
const char *ssa_menu_password(const SsaMenu *,unsigned index);
/* Original localized campaign names only (0..11); no multiplayer maps. */
const char *ssa_menu_level_name(SsaMenu *,unsigned level);
int ssa_menu_draw(SsaMenu *,uint16_t *,size_t pixels);
#endif
