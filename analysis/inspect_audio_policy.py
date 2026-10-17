"""Read-only music/fade/reverb policy locations in the user's ROM."""
from pathlib import Path
import struct
rom=(Path(__file__).resolve().parents[1]/'orig/SSA.gba').read_bytes()
def word(p):return struct.unpack_from('<I',rom,p)[0]
print('init sound mode',hex(word(0xc3e90)))
print('song reverb values',sorted({rom[word(0x3e99d0+s*8)-0x08000000+3] for s in range(136)}))
# Every direct call of the global mode setter, including undiscovered functions.
for p in range(0,len(rom)-4,2):
 a,b=struct.unpack_from('<HH',rom,p)
 if a&0xf800!=0xf000 or b&0xf800!=0xf800:continue
 displacement=((a&0x7ff)<<12)|((b&0x7ff)<<1)
 if displacement&0x400000:displacement-=0x800000
 target=0x08000000+p+4+displacement
 if target in (0x080c43c4,0x08008e74,0x08008cfc):print('audio policy caller',hex(target),hex(0x08000000+p))
