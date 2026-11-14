// Padding that tools/build_ds.py sizes (via build/arm9_pad.h) so the ARM7
// binary starts on a 4 KB boundary in the ROM. The original R4 kernel
// white-screens otherwise: ARM7 at 0x83e00 and 0x89400 failed, 0x84000 booted.
#if __has_include("arm9_pad.h")
#include "arm9_pad.h"
#endif
#ifndef ARM9_PAD
#define ARM9_PAD 0
#endif

const volatile unsigned char g_arm9_pad[ARM9_PAD + 4] __attribute__((used)) = {1};
