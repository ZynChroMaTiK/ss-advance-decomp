"""Compare music requests against original08EB0. ARM runs only in this oracle."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes();b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'src/audio.c'),'-o',str(b/'libssa_audio_music.so')],check=True)
lib=C.CDLL(str(b/'libssa_audio_music.so'));select=lib.ssa_audio_music_select
select.argtypes=[C.c_void_p,C.c_size_t,C.c_uint,C.c_uint,C.c_uint,C.POINTER(C.c_uint)];select.restype=C.c_int
raw=C.create_string_buffer(rom);uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
stop=0x087ff000;requests=[];cases=0
def hook(u,pc,size,data):
 if pc not in (0x08008d30,0x08008d5c,0x08008dd0,0x08008dac):return
 if pc==0x08008d30:requests.append(u.reg_read(UC_ARM_REG_R0)&65535)
 u.reg_write(UC_ARM_REG_R0,1);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
for command in [*range(24),65535]:
 for level in range(14):
  for enabled in (0,1,2,255):
   uc.mem_write(0x03004660,bytes(0x30));uc.mem_write(0x0300572c,struct.pack('<I',0x02000000))
   uc.mem_write(0x02000010,struct.pack('<H',level));uc.mem_write(0x0200002c,bytes([enabled]));requests.clear()
   uc.reg_write(UC_ARM_REG_R0,command);uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,stop|1)
   uc.emu_start(0x08008eb1,stop,count=2000);assert uc.reg_read(UC_ARM_REG_PC)==stop
   out=C.c_uint(0xdeadbeef);result=select(raw,len(rom),command,level,enabled,C.byref(out))
   assert result==bool(requests) and (out.value==requests[0] if requests else out.value==0xdeadbeef),(command,level,enabled,requests,result,out.value)
   cases+=1
out=C.c_uint(0xdeadbeef)
for level in (14,15):assert select(raw,len(rom),2,level,1,C.byref(out))==0 and out.value==0xdeadbeef
for args in ((None,len(rom),2,0,1),(raw,1,2,0,1),(raw,len(rom),65536,0,1),(raw,len(rom),2,16,1),(raw,len(rom),2,0,256)):
 assert select(*args,C.byref(out))==-1 and out.value==0xdeadbeef
assert select(raw,len(rom),2,0,1,None)==-1
report={'result':'pass','rom_comparisons':cases,'diagnostic_map_guards':2,'invalid_argument_guards':6,'test_only_arm_execution':True,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (r/'src/audio.c',r/'include/ssa/audio.h',Path(__file__))}}
(r/'analysis/music-selection-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
