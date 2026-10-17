#ifndef SSA_VIEW_H
#define SSA_VIEW_H
#include "ssa/geometry.h"

/* New PC feature, separate from the recovered original camera-height routine.
 * Distances use signed 16.16. Initialize once per local player. */
typedef struct SsaView {
    int32_t distance, lift;
    uint8_t third_person, h_down;
} SsaView;
typedef struct SsaViewPose {
    int32_t x, y, z;
    int32_t target_x, target_y, target_z;
    int32_t boom_distance;
    uint8_t third_person, obstructed;
} SsaViewPose;

/* Default chase framing:256 units behind,64 units above the player's eye. */
void ssa_view_init(SsaView *view);
/* Platform adapter must translate physical H to 'H', supply key up/down and
 * repeat status, and disable controls in menus/text entry. 'h' also accepted.
 * One toggle per fresh press; disabled/repeated presses consume the held edge.
 * Returns 1 only on a toggle. This is event routing, not an OS keyboard hook. */
int ssa_view_key(SsaView *view, int key, int down, int repeat, int controls_enabled);
void ssa_view_focus_lost(SsaView *view);
/* Call after horizontal movement, jump/vertical physics and ssa_camera_step.
 * Uses original yaw convention: 0 faces +Y, 0x4000 faces +X.
 * Supply a DEDICATED collision workspace (its query results are overwritten).
 * NULL workspace safely yields eye view. No gameplay trigger events execute.
 * Output target is ahead of the player's eye; gameplay aim stays with the
 * player, not the displaced camera. Never changes body, yaw or eye state.
 * Render the player body only when pose.third_person is true, and the first
 * person weapon otherwise. Rendering/animation are not implemented here.
 * Returns 0 on null required input, leaving output unchanged; otherwise 1. */
int ssa_view_pose(const SsaView *view, const SsaBody *body, const SsaCamera *eye,
                  uint16_t heading, const SsaMathTables *tables,
                  SsaMapCollision *collision, SsaViewPose *out);
/* PC orbit: rotate the rear offset with aim pitch, keep the world-up lift and
 * swept clearance/retraction. Pitch is positive up,65536 units per turn. */
int ssa_view_pose_pitch(const SsaView *,const SsaBody *,const SsaCamera *,
    uint16_t heading,int32_t pitch,const SsaMathTables *,SsaMapCollision *,SsaViewPose *);
/* Camera angles toward pose.target, independent of the player's weapon aim.
 * Uses ROM angle tables with full coordinate precision. Returns0 if invalid
 * or coincident; leaves outputs unchanged on failure. */
int ssa_view_angles(const SsaViewPose *,const SsaMathTables *,uint16_t *heading,int32_t *pitch);
#endif
