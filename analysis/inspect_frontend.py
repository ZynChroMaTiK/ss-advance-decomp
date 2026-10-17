"""Read-only original title/menu evidence from the user's ROM."""
from pathlib import Path
import struct
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
dis=Cs(CS_ARCH_ARM,CS_MODE_THUMB);dis.detail=True;dis.skipdata=True
sections=[(0x154c8,0x15614),(0x169dc,0x16fc0),(0x19268,0x19dc8),(0x1b2d0,0x1b7e0),
 (0x16328,0x164e8),(0x18574,0x18774),(0x17b80,0x17dd0),(0x17f7c,0x1833c),
 (0x18400,0x18574),(0x164e8,0x166f8),(0x17dB8,0x17f7c)]
for at in range(0x44c13c,0x44c144,4):
 print('dispatch table',hex(at),hex(struct.unpack_from('<I',rom,at)[0]))
for index in range(15):
 ptr=struct.unpack_from('<I',rom,0x18594+index*4)[0]
 print('menu page',index,hex(ptr),[(i.mnemonic,i.op_str) for i in dis.disasm(rom[ptr-0x08000000:ptr-0x08000000+4],ptr)])
lines=[]
for start,end in sections:
 lines.append(f'\nsection{start:06x}..{end:06x}')
 for ins in dis.disasm(rom[start:end],0x08000000+start):
  extra=''
  if ins.mnemonic=='ldr' and '[pc,' in ins.op_str:
   at=((ins.address+4)&~3)+ins.operands[1].mem.disp-0x08000000
   extra=f' ; {struct.unpack_from("<I",rom,at)[0]:08x}'
  lines.append(f'{ins.address:08x}: {ins.mnemonic} {ins.op_str}{extra}')
out=r/'build/native-tests/frontend-disassembly.txt';out.write_text('\n'.join(lines)+'\n');print(out)
for language in range(3):
 table=struct.unpack_from('<I',rom,0x44cf34+language*4)[0]-0x08000000
 for index in range(80):
  off=struct.unpack_from('<I',rom,table+index*4)[0]-0x08000000
  if 0<=off<len(rom):print(language,index,rom[off:rom.index(0,off)])
