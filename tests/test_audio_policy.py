"""Compare music queue/fade frames with original08C04,08D30,08E74."""
from pathlib import Path
import ctypes as C
import hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];rom=(r/'orig/SSA.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='4ff48d8c18b70a5184758f1917343ae85a70858e'
b=r/'build/native-tests';b.mkdir(exist_ok=True,parents=True)
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(r/'tests/audio_policy_oracle.c'),
 '-o',str(b/'libssa_audio_policy.so')],check=True)
lib=C.CDLL(str(b/'libssa_audio_policy.so'));native=lib.ssa_test_audio_policy
native.argtypes=[C.POINTER(C.c_uint),C.c_uint,C.c_uint,C.POINTER(C.c_int),C.POINTER(C.c_uint)];native.restype=None
automatic=lib.ssa_test_audio_return;automatic.argtypes=[C.c_void_p,C.c_uint,C.c_uint,C.c_uint,C.c_uint];automatic.restype=C.c_uint
raw=C.create_string_buffer(rom)
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,len(rom));uc.mem_write(0x08000000,rom)
uc.mem_map(0x02000000,0x40000);uc.mem_map(0x03000000,0x10000)
actions=[];gains=[];stop=0x087ff000;counts=[0,0,0];returns=[]
def hook(u,pc,size,data):
 if pc not in (0x080c37cc,0x080c46c4,0x080c3ed8,0x080c4e54,0x08008eb0,0x08008d5c):return
 if pc==0x08008eb0:returns.append(u.reg_read(UC_ARM_REG_R0))
 if pc==0x080c46c4:actions.append(65535)
 if pc==0x080c3ed8:actions.append(u.reg_read(UC_ARM_REG_R0)&65535)
 if pc==0x080c4e54:gains.append((u.reg_read(UC_ARM_REG_R0),u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2)))
 u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook)
def compare(state,op=0,arg=0):
 block=bytearray(0x20);block[0:2]=bytes(state[0:2]);struct.pack_into('<HH',block,2,*state[2:4]);struct.pack_into('<H',block,12,state[4])
 uc.mem_write(0x03004660,bytes(block));uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,stop|1)
 uc.reg_write(UC_ARM_REG_R0,arg);actions.clear();gains.clear()
 uc.emu_start((0x08008c04,0x08008d30,0x08008e74)[op]|1,stop,count=500)
 assert uc.reg_read(UC_ARM_REG_PC)==stop
 result=bytes(uc.mem_read(0x03004660,0x20));expected=[result[0],result[1],*struct.unpack_from('<HH',result,2),struct.unpack_from('<H',result,12)[0]]
 # All unmodelled fields must stay untouched.
 for i in range(len(block)):
  if i not in (0,1,2,3,4,5,12,13):assert result[i]==block[i]
 values=(C.c_uint*5)(*state);action=C.c_int();gain=C.c_uint()
 native(values,op,arg,C.byref(action),C.byref(gain));assert list(values)==expected,(state,op,arg,list(values),expected)
 assert actions==([] if action.value<0 else [action.value]),(state,op,actions,action.value)
 if gain.value==0xffffffff:assert not gains
 else:assert gains==[(p,65535,gain.value) for p in (0x03006990,0x030069d0,0x03006a10,0x03006a60)]
 counts[op]+=1
 return expected,action.value
rng=random.Random(0x08c04)
for _ in range(6000):compare([rng.randrange(256),rng.randrange(256),rng.randrange(65536),rng.randrange(65536),rng.choice([0,0x73,0x78,0x81,65535])])
for interval in range(256):
 for flags in (0,2,4,8,12,255):compare([flags,0,0,0xffdc,0xffff],2,interval)
for volume in (0,1,255,256,0xffdc,65535):
 for song in (0,115,120,129,65535):
  for flags in (0,2,4,8,255):compare([flags,0x77,0,volume,0xffff],1,song)
# Full default fades at every interval: pending music must wait, then start
# exactly once at restored volume. This also checks the one-frame boundary.
for interval in range(16):
 state,_=compare([0,0,0,0xffdc,0xffff],2,interval)
 state,_=compare(state,1,0x81);started=[]
 for frame in range(17*(interval+1)+3):
  state,action=compare(state)
  if action>=0:started.append((frame,action))
 assert started==[(17*(interval+1),0x81)],(interval,started)
return_cases=0
for command in range(24):
 for level in range(14):
  for enabled in (0,1,2,255):
   for active in (0,1):
    uc.mem_write(0x03004660,bytes(0x20));uc.mem_write(0x03004668,struct.pack('<H',command))
    uc.mem_write(0x03006994,struct.pack('<I',1 if active else 0xffffffff))
    uc.mem_write(0x0300572c,struct.pack('<I',0x02000000));uc.mem_write(0x0200002c,bytes([enabled]))
    uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,stop|1);returns.clear()
    uc.emu_start(0x08009121,stop,count=100);assert uc.reg_read(UC_ARM_REG_PC)==stop
    expected=2 if returns else command
    assert returns in ([],[2]) and automatic(raw,command,level,enabled,active)==expected,(command,level,enabled,active,returns)
    return_cases+=1
report={'result':'pass','fade_frame_rom_comparisons':counts[0],'request_rom_comparisons':counts[1],
 'fade_start_rom_comparisons':counts[2],'automatic_return_rom_comparisons':return_cases,'test_only_arm_execution':True,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
 (r/'src/audio.c',r/'include/ssa/audio.h',r/'tests/audio_policy_oracle.c',Path(__file__))}}
(r/'analysis/audio-policy-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
