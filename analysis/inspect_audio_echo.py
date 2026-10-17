"""Read-only locations for MP2K pseudo-echo recovery."""
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct
r = (Path(__file__).resolve().parents[1] / 'orig/SSA.gba').read_bytes()
print('xcmd', [hex(x) for x in struct.unpack_from('<12I', r, 0x3e8fac)])
d = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for p in range(0xc3000, 0xc3eac, 2):
    ins = next(d.disasm(r[p:p+2], 0x08000000+p), None)
    if ins and ins.mnemonic == 'ldrb' and ins.op_str in ('r0, [r4, #0xc]', 'r0, [r4, #0xd]', 'r5, [r4, #0xc]'):
        print(hex(ins.address), ins.mnemonic, ins.op_str)
print('header reverb', sorted(set(r[struct.unpack_from('<I', r, 0x3e99d0+s*8)[0]-0x08000000+3] for s in range(136))))
