# Native Windows input backend

`platform/win32/input_win32.c` attaches to a caller-owned HWND and forwards
Windows keyboard, focus and raw mouse events to `SsaPcInputRuntime`. It builds
as native x64 Windows C with MSVC and User32. It is an input backend, not a
windowed game, renderer, emulator or playable port.

## Event routing

The caller forwards messages from its WndProc to `ssa_win32_input_message`.
WASD, Space, H and E use `WM_KEYDOWN`/`WM_KEYUP`; the previous-state bit30 marks
autorepeat. Unrelated keys and system-key messages remain with the caller.
This follows Microsoft's [WM_KEYDOWN contract](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown).

Mouse registration selects usage page1, usage2 with flags zero and the caller's
window. It does not request `INPUTSINK`, background input or keyboard hooks.
The registration is foreground-only, as specified by
[RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice).
The raw parser accepts relative X/Y counts and left-button down/up edges;
vertical counts are forwarded to the portable core, whose current control
design ignores vertical look. Absolute motion and wheel actions are ignored;
absolute packets may still supply left-button edges. Background packets are
ignored. The backend does not turn legacy `WM_LBUTTON*` or `WM_MOUSEMOVE`
messages into gameplay events, preventing duplicate fire/motion when Windows
also emits legacy messages. The caller must follow the same policy.

`WM_INPUT` uses the two-call size/read pattern from
[GetRawInputData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata).
It checks both sizes before parsing copied data. `DefWindowProcW` is called
inside this handler even when parsing fails, satisfying foreground
[WM_INPUT cleanup](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input).
The caller must not invoke default handling a second time for consumed input.
The public raw-payload parser shares this production path and is independently
testable without inventing an OS raw-input handle. Relative/absolute and button
flags follow [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse).

Focus loss clears held and pending input through the actual PC runtime adapter;
focus gain creates no presses. Pump/forward messages outside game updates or
inside the external POLL stage **before** sampling PC input. Do not pump from
render/simulation callbacks: event guards reject those phases, and focus calls
report `ERROR_BUSY` rather than silently claiming success. Attach/detach happen
outside game updates, on the window's owning thread. There is no worker input
thread, global hook, cursor move, cursor hiding, clipping or mouse capture.

## Registration ownership and cleanup

An explicit attach saves the process's prior mouse registration using
[GetRegisteredRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getregisteredrawinputdevices).
Only one backend instance may attach at a time. This is necessary because
Windows has one registration target per device class in a process; registration
is an explicit application decision, not a library startup side effect.
[Microsoft documents this limitation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices).

Detach restores the previous mouse registration or removes this registration
using `RIDEV_REMOVE` with a null target. If another component has replaced it
since attach, detach preserves the newer registration and returns1. Normal
detach returns0. An API failure returns-1 and retains attachment for retry.
The previous registration's target must remain valid if it is to be restored.
Capture and clipping are untouched; tests verify their values are unchanged.

The caller owns the HWND and initialized PC runtime. Keep both alive until
detach succeeds, and detach before destroying the window or freeing the runtime.
No subclass or hidden lifetime hook is installed. `last_error` holds Windows
diagnostics. A message return of0 means unhandled; return the supplied LRESULT
for every nonzero result, including -1. Errors must be surfaced by the host.

```c
LRESULT result = 0;
int handled = ssa_win32_input_message(&backend, hwnd, msg, wp, lp, &result);
if (handled) {
    /* Host records backend.last_error when handled < 0. */
    return result;
}
return DefWindowProcW(hwnd, msg, wp, lp);
```

The example assumes a live attachment and the owning window. Include
`platform/win32`, link the backend source/library, `ssa_pc_input_runtime` and
`User32.lib`. Existing PC runtime input distribution, jump and H-view remain
responsible for applying the sampled events to the selected player.

## Windows validation

Run from the workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-win32-input.ps1
```

This changes execution policy only for that child PowerShell process. The
runner discovers the installed Visual Studio 2022 BuildTools via `vswhere`,
uses its bundled CMake with the VS2022 x64 generator, and creates all generated
files under `build/win32-input`. It installs nothing. The generated test project
links the real native core dependency graph and compiles the backend and test
with C11, `/W4 /WX`, Release settings. It produces
`Release/ssa_win32_input_backend.lib` and `Release/ssa_win32_input_test.exe`.

Passing evidence is **3,215 assertions**, 23 routed window messages and 1,035
raw payloads, including 1,025 relative-motion cases. The fixture creates only
message-only windows and uses `PostMessage`/`SendMessage` for its keyboard/focus
messages. It checks opposing movement, Space tap, H repeat handling, E use,
left-button taps/holds, duplicate legacy rejection, relative wrap, absolute and
background packets, malformed payloads, wrong-thread and update-phase guards,
registration takeover/restoration and replacement by another component.

The fixture uses the actual PC runtime initialization and event functions with
a minimal valid routing projection. It does **not** step a game or substitute
empty game callbacks. No physical keyboard/mouse interaction, successful
`WM_INPUT` handle path, visible window, cursor movement, `SendInput`, game
rendering or ROM execution is claimed. The invalid-handle case tests the API
failure/cleanup branch; successful raw-device messages still require a future
physical-device integration check in the actual game window.

Counts, toolchain and source hashes are in `win32-input-validation.json`.
