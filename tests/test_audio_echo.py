"""Compare PCM envelope steps and echo commands with the original ROM driver.

ARM execution is a test oracle only. The game uses native C.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R4,UC_ARM_REG_R6,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'tests/audio_echo_oracle.c'),
 '-o',str(b/'libssa_audio_echo.so')],check=True)
lib=C.CDLL(str(b/'libssa_audio_echo.so'));step=lib.ssa_test_pcm_envelope
step.argtypes=[C.POINTER(C.c_uint)];step.restype=None
command=lib.ssa_test_echo_command;command.argtypes=[C.c_uint,C.c_uint,C.POINTER(C.c_uint)];command.restype=C.c_int
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
uc.hook_add(UC_HOOK_CODE,lambda u,pc,size,data:u.emu_stop(),begin=0x080c357c,end=0x080c357c)
flags=(3,2,1,0x40,4);rng=random.Random(0x535341);cases=0
def compare(state):
 global cases
 stage,level,attack,decay,sustain,release,volume,length=state
 channel=bytearray(0x40);channel[0]=flags[stage]
 channel[4:8]=bytes((attack,decay,sustain,release));channel[9]=level;channel[12:14]=bytes((volume,length))
 uc.mem_write(0x02000000,bytes(channel));uc.reg_write(UC_ARM_REG_R4,0x02000000);uc.reg_write(UC_ARM_REG_R6,flags[stage])
 uc.emu_start(0x080c32ef,0x080c3356,count=100)
 assert uc.reg_read(UC_ARM_REG_PC) in (0x080c3356,0x080c357c)
 actual=bytes(uc.mem_read(0x02000000,0x40));sf=actual[0];active=bool(sf)
 phase=4 if sf&4 else 3 if sf&0x40 else {3:0,2:1,1:2}.get(sf&3,2)
 native=(C.c_uint*9)(*state,1);step(native)
 assert native[8]==active and native[7]==actual[13] and (not active or (native[0],native[1])==(phase,actual[9])),(state,list(native),actual.hex())
 cases+=1
 return list(native[:8]) if native[8] else None
# Exhaustive8-bit levels against phase/parameter boundaries, then random steps.
for stage in range(5):
 for level in range(256):
  for edge in (0,1,127,128,254,255):compare((stage,level,edge,edge,edge,edge,edge,edge))
for _ in range(5000):compare((rng.randrange(5),*(rng.randrange(256) for _ in range(7))))
# Whole lifetimes test release thresholds, sustain-zero entry and exact tail expiry.
for volume in (0,1,64,127,255):
 for length in (0,1,2,8,128,255):
  for stage in (1,3,4):
   state=[stage,255,255,128,0,192,volume,length]
   for _ in range(300):
    state=compare(state)
    if state is None:break
commands=0;stop=0x087ff000
for extension in (8,9):
 for value in range(256):
  track=bytearray(0x50);struct.pack_into('<I',track,0x40,0x02000100)
  uc.mem_write(0x02000000,bytes(track));uc.mem_write(0x02000100,bytes((extension,value)))
  uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_R1,0x02000000)
  uc.reg_write(UC_ARM_REG_SP,0x03007f00);uc.reg_write(UC_ARM_REG_LR,stop|1)
  uc.emu_start(0x080c51f9,stop,count=100);assert uc.reg_read(UC_ARM_REG_PC)==stop
  result=bytes(uc.mem_read(0x02000000,0x50));expected=bytearray(track)
  expected[0x1e if extension==8 else 0x1f]=value;struct.pack_into('<I',expected,0x40,0x02000102)
  assert result==expected
  out=(C.c_uint*3)();assert command(extension,value,out)==1
  assert list(out)==[result[0x1e],result[0x1f],4]
  commands+=1
report={'result':'pass','pcm_envelope_rom_comparisons':cases,'echo_command_rom_comparisons':commands,
 'test_only_arm_execution':True,'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in (r/'src/audio.c',r/'include/ssa/audio.h',r/'tests/audio_echo_oracle.c',Path(__file__))}}
(r/'analysis/audio-echo-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
