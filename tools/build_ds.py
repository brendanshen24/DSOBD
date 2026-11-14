#!/usr/bin/env python3
"""Build dsobd.nds so the original R4 kernel boots it.

That kernel white-screens unless the ARM7 binary starts on a 4 KB boundary in
the ROM (ARM7 at 0x83e00 and 0x89400 failed, 0x84000 booted). ndstool places
ARM7 right after ARM9 (512-byte aligned), so we grow source/arm9_pad.c until
it lands aligned, and refuse to finish otherwise.
"""
import struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "dsobd.nds"
PAD_OBJ = ROOT / "build/dsobd/source/arm9_pad.c.o"
ELF = ROOT / "build/dsobd.elf"
PAD_H = ROOT / "build/arm9_pad.h"
ALIGN = 0x1000


def build(pad):
    PAD_H.parent.mkdir(exist_ok=True)
    PAD_H.write_text(f"#define ARM9_PAD {pad}\n")
    PAD_OBJ.unlink(missing_ok=True)
    ELF.unlink(missing_ok=True)   # make's mtime checks skip the relink/repack
    ROM.unlink(missing_ok=True)   # otherwise, leaving a stale ROM header
    subprocess.run(["make", "-f", "Makefile.gauges", *sys.argv[1:]], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    h = ROM.read_bytes()[:0x40]
    arm9_size, arm7_off = struct.unpack_from("<I", h, 0x2C)[0], struct.unpack_from("<I", h, 0x30)[0]
    return arm9_size, arm7_off


pad = 0
for _ in range(6):
    arm9_size, arm7_off = build(pad)
    if arm7_off % ALIGN == 0:
        break
    pad += ALIGN - arm7_off % ALIGN
else:
    sys.exit(f"error: could not align ARM7 (offset {arm7_off:#x}); R4 would white-screen")

if arm9_size >= 0x80000:
    sys.exit(f"error: ARM9 is {arm9_size:#x} bytes; every R4-proven build was under 0x80000")
print(f"R4 layout OK: ARM9 {arm9_size:#x} bytes, ARM7 at {arm7_off:#x} (pad {pad})")
