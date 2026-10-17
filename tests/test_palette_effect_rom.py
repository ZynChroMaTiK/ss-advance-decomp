"""Whole0750C display call: native palette phase versus original ROM instructions.
Only blend-register setup15698 and512-byte DMA copying are controlled services.
"""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/palette_effect.c'),
 '-o',str(b/'libssa_palette_effect.so')],check=True)
class Palette(C.Structure):
 _fields_=[('rom',C.c_void_p),('size',C.c_size_t),('colors',C.c_uint16*256),('level',C.c_uint),('counter',C.c_uint8)]
lib=C.CDLL(str(b/'libssa_palette_effect.so'));init=lib.ssa_palette_effect_init;step=lib.ssa_palette_effect_step
init.argtypes=[C.POINTER(Palette),C.c_void_p,C.c_size_t,C.c_uint]
step.argtypes=[C.POINTER(Palette),C.POINTER(C.c_uint32),C.c_uint16]
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for addr,size in [(0x08000000,len(rom)),(0x02000000,0x40000),(0x03000000,0x10000),
 (0x04000000,0x1000),(0x05000000,0x1000)]:uc.mem_map(addr,size)
uc.mem_write(0x08000000,rom)
G,P,E,MAP,STOP=0x02000000,0x02001000,0x02002000,0x02003000,0x087ff000
def put(a,fmt,*args):uc.mem_write(a,struct.pack('<'+fmt,*args))
def get(a,fmt):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
def hook(u,a,size,data):
 if a==STOP:u.emu_stop()
 elif a==0x08015698:u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 elif a==0x080c5320:
  src,dst,count=(u.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2))
  assert dst==0x05000000 and count==0x04000080
  u.mem_write(dst,bytes(u.mem_read(src,512)));u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
for a,v in [(0x03004010,G),(0x03004648,P),(0x0300572c,MAP),(P+0xac,E)]:put(a,'I',v)
put(0x0300464e,'H',0);put(E+0x32,'h',100);put(G+4,'H',0)
calls=0;compared=0
def compare(p,request,flags):
 global calls,compared
 seed=bytes(p.colors)
 uc.mem_write(0x05000000,seed);put(0x030000e0,'I',request);put(0x030000c5,'B',p.counter)
 put(P+0xbc,'H',flags);put(MAP+0x10,'H',p.level)
 uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,STOP|1)
 uc.emu_start(0x0800750d,STOP,count=10000)
 q=C.c_uint32(request);assert step(C.byref(p),C.byref(q),flags)==1
 assert q.value==get(0x030000e0,'I'),(hex(request),flags,p.level,p.counter,hex(q.value),hex(get(0x030000e0,'I')))
 assert p.counter==get(0x030000c5,'B')
 assert bytes(p.colors)==bytes(uc.mem_read(0x05000000,512)),(hex(request),flags,p.level)
 calls+=1;compared+=256
 return q.value
requests=[0,1,2,9,0x80000002,0x80000009,0x40000002,0x40000009,0x20000002,0x2000001f]
for level in range(16):
 p=Palette();assert init(C.byref(p),rom,len(rom),level)==1
 for req in requests:
  for counter in (0,1,2,3,127,128,254,255):
   for flags in (0,8):p.counter=counter;compare(p,req,flags)
for level in (0,6,12,14):
 p=Palette();assert init(C.byref(p),rom,len(rom),level)==1
 for req in (9,0x80000009,0x40000009,0x2000001f):
  for counter in range(256):p.counter=counter;compare(p,req,0)
 for req in (9,0x80000009,0x40000009,0x2000001f):
  for tick in range(80):req=compare(p,req,0)
guards=0;p=Palette();assert init(C.byref(p),rom,len(rom),0)==1
for req in (10,0x8000000a,0x4000000a,0x20000020):
 before=bytes(p);q=C.c_uint32(req);assert step(C.byref(p),C.byref(q),0)==-1
 assert bytes(p)==before and q.value==req;guards+=1
bad=bytearray(rom);struct.pack_into('<I',bad,0x44a82c,0xffffffff)
buf=C.create_string_buffer(bytes(bad));p.rom=C.addressof(buf);q=C.c_uint32(2);before=bytes(p)
assert step(C.byref(p),C.byref(q),0)==-1 and bytes(p)==before and q.value==2;guards+=1
before=bytes(p);assert init(C.byref(p),rom,16,0)==-1 and bytes(p)==before;guards+=1
report=dict(result='pass',rom_calls=calls,palette_entries_compared=compared,maps=16,guards=guards,
 scope='Whole original0750C call; compare background palette/request/counter. Blend-register callback and hardware DMA are controlled. No native ROM execution.',
 source_sha256={str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [r/'src/palette_effect.c',r/'include/ssa/palette_effect.h',Path(__file__)]})
(r/'analysis/palette-effect-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
