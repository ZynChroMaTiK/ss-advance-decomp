"""Find input-state literals, including ones left as data by conservative import."""
from pathlib import Path
import struct
import argparse
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

root = Path(__file__).resolve().parents[1]
rom = (root / "orig/SSA.gba").read_bytes()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("address", nargs="?", type=lambda x: int(x, 0), default=0x030042F0)
p.add_argument("--size", type=lambda x: int(x, 0), default=0x24)
a = p.parse_args()
for off in range(0, len(rom) - 3, 4):
    value = struct.unpack_from("<I", rom, off)[0]
    if not a.address <= value < a.address + a.size:
        continue
    print(f"literal {0x08000000+off:08X} = {value:08X}")
    for pos in range(max(0, off - 1024), off, 2):
        op = struct.unpack_from("<H", rom, pos)[0]
        if op & 0xF800 == 0x4800 and ((pos + 4) & ~3) + (op & 255)*4 == off:
            print(f"  candidate Thumb load {0x08000000+pos:08X}")
            for ins in md.disasm(rom[pos:pos+20], 0x08000000+pos):
                print(f"    {ins.address:08X} {ins.mnemonic:8} {ins.op_str}")
