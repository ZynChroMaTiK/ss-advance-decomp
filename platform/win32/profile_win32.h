#ifndef SSA_PROFILE_WIN32_H
#define SSA_PROFILE_WIN32_H
#include "ssa/profile.h"
#include <wchar.h>
enum { SSA_SAVE_LANGUAGE,SSA_SAVE_COMPLETE,SSA_SAVE_CLEAR };
typedef struct SsaWin32Profile {
 SsaProfile data;
 wchar_t path[32768];
 unsigned enabled,last_error;
} SsaWin32Profile;
/* Missing files are a clean first launch. Corrupt/unreadable files report an
 * error and use defaults in memory; no file is changed merely by loading it. */
int ssa_win32_profile_open(SsaWin32Profile *,const wchar_t *,unsigned enabled);
/* Serialize concurrent writers, reload the latest file, apply only this event,
 * then flush and atomically replace it. Clear does not merge old progress. */
int ssa_win32_profile_store(SsaWin32Profile *,unsigned action,unsigned value);
#endif
