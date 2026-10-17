#ifndef SSA_PROFILE_H
#define SSA_PROFILE_H
#include <stddef.h>
#include <stdint.h>
enum { SSA_PROFILE_LEVELS=12, SSA_PROFILE_BYTES=16 };
typedef struct SsaProfile { unsigned language; uint16_t completed; } SsaProfile;
void ssa_profile_reset(SsaProfile *);
/* A level unlocks when its immediate predecessor was beaten; level0 is free.
 * Password/direct starts alone never award completion. */
int ssa_profile_unlocked(const SsaProfile *,unsigned level);
int ssa_profile_complete(SsaProfile *,unsigned level);
/* Versioned, endian-independent file format with CRC32. Invalid input leaves
 * the destination unchanged. No ROM data or in-progress inventory is saved. */
int ssa_profile_encode(const SsaProfile *,uint8_t *,size_t);
int ssa_profile_decode(SsaProfile *,const uint8_t *,size_t);
#endif
