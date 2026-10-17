"""Read-only original map-view/level-start evidence; no ROM changes."""
from pathlib import Path
import struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

r = Path(__file__).resolve().parents[1]
rom = (r / 'orig/SSA.gba').read_bytes()
dis = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
dis.detail = True
dis.skipdata = True
sections = [(0x1aaa0, 0x1ad34), (0x18774, 0x189f4), (0x15614, 0x15740),
            (0x15b48, 0x15c2c), (0x15888, 0x15a78), (0x168e4, 0x169ac),
            (0x1a780, 0x1aaa0), (0x1f44, 0x2120), (0x165c0, 0x168e4),
            (0x15c2c, 0x15f2c), (0x15f2c, 0x15ff8), (0x16060, 0x165c0),
            (0x1892c, 0x189f4), (0x8eb0, 0x9120), (0x91e4, 0x9250)]
for off in range(0, 0xc3000, 2):
    a, b = struct.unpack_from('<HH', rom, off)
    if a & 0xf800 != 0xf000 or b & 0xf800 != 0xf800:
        continue
    delta = ((a & 0x7ff) << 12) | ((b & 0x7ff) << 1)
    if delta & 0x400000:
        delta -= 0x800000
    if 0x08000000 + off + 4 + delta == 0x0801ac80:
        sections.append((max(0, off - 160), off + 80))
for off in range(0, len(rom)-3, 2):
    if struct.unpack_from('<I', rom, off)[0] in (0x0801ac80, 0x0801ac81, 0x0801aaa0, 0x0801aaa1):
        print('map view reference', hex(off))
        if off < 0xc3000:
            sections.append((max(0, off - 256), off + 32))
lines = []
for start, end in sections:
    lines.append(f'\nsection {start:06x}..{end:06x}')
    for ins in dis.disasm(rom[start:end], 0x08000000 + start):
        extra = ''
        if ins.mnemonic == 'ldr' and '[pc,' in ins.op_str:
            delta = ins.operands[1].mem.disp
            at = ((ins.address + 4) & ~3) + delta - 0x08000000
            extra = f' ; {struct.unpack_from("<I", rom, at)[0]:08x}'
        lines.append(f'{ins.address:08x}: {ins.mnemonic} {ins.op_str}{extra}')
out = r / 'build/native-tests/map-view-disassembly.txt'
out.write_text('\n'.join(lines) + '\n')
print(out)
for off in (0x44a630, 0x44a650, 0x44a670, 0x44cf34, 0x44d1c4):
    print(hex(off), [hex(v) for v in struct.unpack_from('<16I', rom, off)])
for off in (0xdfcec, 0xdfcf0, 0x44c264, 0xb7dac, 0xb7ea8):
    print('bytes', hex(off), rom[off:off+64])
for off in (0xab39c,0xafe4c,0xb4eec,0xb697c,0xb7a60):
    print('lz header',hex(off),hex(struct.unpack_from('<I',rom,off)[0]))
for language in range(3):
    strings = struct.unpack_from('<I', rom, 0x44cf34+language*4)[0]-0x08000000
    for index in (4, 48, 49, 64):
        off = struct.unpack_from('<I', rom, strings+index*4)[0]-0x08000000
        print('text', language, index, rom[off:rom.index(0, off)])
for level in range(14):
    off = struct.unpack_from('<I', rom, 0x44c1d4+level*4)[0]-0x08000000
    route = struct.unpack_from('<I', rom, 0x44d1c4+level*4)[0]-0x08000000
    print('level', level, rom[off:rom.index(0, off)], 'route', rom[route:route+22].hex(' '))
