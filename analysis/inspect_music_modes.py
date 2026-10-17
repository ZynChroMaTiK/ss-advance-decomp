"""Read-only callers of the ROM's08EB0 sound/menu command dispatcher."""
from pathlib import Path
import struct
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();dis=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
for off in range(0,0xc3000,2):
 a,b=struct.unpack_from('<HH',rom,off)
 if a&0xf800!=0xf000 or b&0xf800!=0xf800:continue
 delta=((a&0x7ff)<<12)|((b&0x7ff)<<1)
 if delta&0x400000:delta-=0x800000
 if 0x08000000+off+4+delta!=0x08008eb0:continue
 print(f'caller{0x08000000+off:08x}')
 for ins in dis.disasm(rom[max(0,off-12):off+4],0x08000000+max(0,off-12)):
  print(f' {ins.address:08x}: {ins.mnemonic} {ins.op_str}')
