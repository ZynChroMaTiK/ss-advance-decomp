#ifndef SSA_INPUT_WIN32_H
#define SSA_INPUT_WIN32_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>
#include "ssa/pc_input_runtime.h"

typedef struct SsaWin32Input {
    HWND window;
    SsaPcInputRuntime *runtime;
    RAWINPUTDEVICE previous_mouse;
    DWORD thread_id,last_error;
    unsigned attached,had_previous,focused;
} SsaWin32Input;

/* Zero-init; attaches to a caller-owned HWND on the current thread. One backend
 * attachment per process. Explicit registration takes over only mouse usage1/2
 * and remembers its prior registration. Flags0: foreground input only. No
 * keyboard hooks/subclassing, capture, clipping, hiding or cursor warping.
 * Returns0/-1. Attach/detach outside game updates. Borrowed initialized runtime
 * and window must outlive attachment. */
int ssa_win32_input_attach(SsaWin32Input *,HWND,SsaPcInputRuntime *);
/* Restore previous registration (or remove ours). If another component replaced
 * our registration, preserve its newer choice and return1; normal success0.
 * Failure -1 retains attachment for retry. Call before destroying the window. */
int ssa_win32_input_detach(SsaWin32Input *);
/* Forward from the owning WndProc. Return0 means unhandled: call DefWindowProc.
 * Nonzero means handled, including -1 error: return *result. WM_INPUT always
 * calls DefWindowProc internally for required system cleanup, even on failure.
 * Pump/forward events outside game updates or in external POLL before sampling.
 * Other update phases reject input; never pump messages from render callbacks.
 * last_error retains diagnostics. Legacy mouse messages are not translated:
 * do not also feed them into the same runtime's fire/relative-mouse functions. */
int ssa_win32_input_message(SsaWin32Input *,HWND,UINT,WPARAM,LPARAM,LRESULT *result);
/* Parse an already-copied RAWINPUT payload (same path used by WM_INPUT). Relative
 * mouse motion and left-button flags only. Absolute motion/wheel ignored; absolute
 * packets may still carry button edges. Background packets ignored. Returns
 *1 mouse handled,0 ignored,-1 malformed/invalid. No HRAWINPUT fabricated here. */
int ssa_win32_input_raw(SsaWin32Input *,const void *,size_t);
#endif
