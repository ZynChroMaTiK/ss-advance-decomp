"""Inspect a ROM range with explicit CPU mode; does not claim function boundaries."""
import argparse
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("address", type=lambda s: int(s, 0))
p.add_argument("length", type=lambda s: int(s, 0))
p.add_argument("--arm", action="store_true")
a = p.parse_args()
rom = (Path(__file__).resolve().parents[1] / "orig/SSA.gba").read_bytes()
off = a.address - 0x08000000
if off < 0 or off + a.length > len(rom) or a.length <= 0:
    p.error("range is outside the ROM")
md = Cs(CS_ARCH_ARM, CS_MODE_ARM if a.arm else CS_MODE_THUMB)
pos = off
while pos < off + a.length:
    # ARM7TDMI has Thumb-1, not Thumb-2. A literal looking like a Thumb-2
    # prefix must not consume the following real 16-bit instruction.
    width = 4 if a.arm else 2
    if not a.arm and pos + 4 <= len(rom):
        first, second = struct.unpack_from("<HH", rom, pos)
        if first & 0xF800 == 0xF000 and second & 0xF800 == 0xF800:
            width = 4
    instruction = list(md.disasm(rom[pos:pos+width], 0x08000000+pos, count=1))
    if not instruction:
        print(f"{0x08000000+pos:08X}: .bytes   {rom[pos:pos+width].hex()}")
        pos += width
        continue
    i = instruction[0]
    literal = ""
    if not a.arm and i.size == 2:
        opcode = int.from_bytes(i.bytes, "little")
        if opcode & 0xF800 == 0x4800:
            address = ((i.address + 4) & ~3) + (opcode & 255) * 4
            if 0 <= address - 0x08000000 <= len(rom) - 4:
                value = struct.unpack_from("<I", rom, address - 0x08000000)[0]
                literal = f" ; [{address:08X}] = {value:08X}"
    print(f"{i.address:08X}: {i.mnemonic:8} {i.op_str}{literal}")
    pos += i.size
