# Native pause/menu control

`src/mode_control.c` recovers the complete original `080021E4` frame control
routine. It scans the live GAME player count, reading each player's packed
input at+A4. It does not filter by activity flags or the display-local index.
Native borrowed scalar pointers retain the original flags, selection, timer and
input-state accesses without embedding GBA pointers in host structures.

Paused frames set GAME+76 to100 for each visited player. Pressed bits90000
confirm the current selection;400000/800000 move selection when GAMEflags100 is
clear. Selection0 resumes with flag400 set and flags1/40 cleared. Selection1
changes to3; selection2 sets shared input state5 before the original UI command
sequence and resumes; selection3 changes to0. Unpaused input80000 opens the
menu. GAMEflags100 separately starts its notice once, setting flags801. These
are recovered state operations; the broader meaning of every UI command remains
outside this module.

The calls to08008EB0 and0800E914 remain mandatory typed events carrying their
original arguments. The former is a UI command dispatcher, not a sound call.
The latter creates a sprite with ID B1/B2/B3 and arguments0,38,0. No UI objects
are silently invented or skipped. Callbacks can change scalar state and later
inputs, which subsequent original reads observe. Borrowed pointers and callback
storage must remain stable. Nested calls fail; callback failure preserves prior
effects and clears the update guard.

Run `test-mode-control`. The test executes all of021E4 in the supplied ROM,
controlling only those two callee boundaries. All13,092 comparisons pass with
undefined-behavior sanitizers:7,680 flag/button/selection/count combinations,
5,000 reserved-bit random states,368 live callback mutations,3 failure baselines
and41 failure prefixes. It also checks60,101 callback reentry attempts and11
invalid binding guards. Event arguments and scalar snapshots agree at every
boundary. `mode-control-validation.json` records evidence and source hashes.

This is a standalone native control component for the remaining CONTROL stage.
The combined game adapter still delegates CONTROL to its external callback; a
frontend must bind the canonical GAMEflags and shared life_game.input_state,
provide menu scalars and implement the two UI events. No Escape-key policy,
menu rendering or playable executable is claimed by this module.

The playable frontend now composes this routine through `pause.c`, a one-player
event-queue adapter, and `intermission.c` original notice-sprite presentation.
Esc/P, keyboard/mouse Continue/Quit/Yes/No, frozen scene and pause/resume audio
are connected. See [current integration and verification](completion.md).
