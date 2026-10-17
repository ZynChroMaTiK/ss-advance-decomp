"""Read-only PSG instrument/frequency inventory from the owned ROM."""
from pathlib import Path
import struct
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
rom=(Path(__file__).resolve().parents[1]/'orig/SSA.gba').read_bytes()
u32=lambda p:struct.unpack_from('<I',rom,p)[0]
banks=sorted({u32(u32(0x3e99d0+s*8)-0x08000000+4)-0x08000000 for s in range(136) if rom[u32(0x3e99d0+s*8)-0x08000000]})
for bank in banks:
 if bank!=0x3e8fdc:continue # The music bank has128 entries; SFX uses a separate shorter bank.
 print('bank',hex(bank))
 for n in range(128):
  p=bank+n*12;kind=rom[p]
  if not kind&0xc0 and kind&7==3:
   print(n,rom[p:p+12].hex(' '))
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom);uc.mem_map(0x03000000,0x10000)
for kind in (1,2,3):
 for key in (36,40,48,60,72):
  uc.reg_write(UC_ARM_REG_R0,kind);uc.reg_write(UC_ARM_REG_R1,key);uc.reg_write(UC_ARM_REG_R2,0)
  uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,0x087ff001)
  uc.emu_start(0x080c4881,0x087ff000,count=100)
  assert uc.reg_read(UC_ARM_REG_PC)==0x087ff000
  reg=uc.reg_read(UC_ARM_REG_R0);print('pitch',kind,key,reg,(65536 if kind==3 else 131072)/(2048-reg))
