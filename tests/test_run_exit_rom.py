"""Original02564 death/quit exit fragment, ARM execution as a test oracle only."""
from pathlib import Path
import ctypes as C,hashlib,json,random,struct,subprocess
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_PC
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
inputs=[r/'src/run_exit.c',r/'include/ssa/run_exit.h',r/'include/ssa/player_lifecycle.h',Path(__file__)]
subprocess.run(['gcc','-std=c11','-O1','-g','-shared','-fPIC','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(r/'include'),str(inputs[0]),'-o',str(b/'librun_exit.so')],check=True)
class Life(C.Structure):
 _fields_=[('message_ticks',C.c_uint16),('input_state',C.c_uint16)]+[(n,C.c_uint8) for n in
 ('ticks','player_count','local_player','continue_flag','exit_flag')]
lib=C.CDLL(str(b/'librun_exit.so'));step=lib.ssa_run_exit
step.argtypes=[C.POINTER(Life),C.POINTER(C.c_uint16)]
rom=(r/'orig/SSA.gba').read_bytes();uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for at,size in ((0x08000000,len(rom)),(0x02000000,0x10000),(0x03000000,0x10000)):
 uc.mem_map(at,size)
uc.mem_write(0x08000000,rom)
GAME,SETTINGS,INPUT=0x02000000,0x02001000,0x030042f0
def put(at,fmt,value):uc.mem_write(at,struct.pack('<'+fmt,value))
def get(at,fmt):return struct.unpack('<'+fmt,uc.mem_read(at,struct.calcsize(fmt)))[0]
def hook(u,at,size,ctx):
 assert at==0x0800e648
 u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook,begin=0x0800e648,end=0x0800e648)
rng=random.Random(0x2a9a);cases=0
for mode in (4,5):
 for lives in range(256):
  for players in range(1,5):
   flags=rng.getrandbits(16);timer=rng.getrandbits(16)
   life=Life(timer,mode,1,players,0,lives,rng.randrange(2));native_flags=C.c_uint16(flags)
   uc.mem_write(GAME,bytes(0x1000));uc.mem_write(SETTINGS,bytes(0x100))
   put(0x03004010,'I',GAME);put(0x0300572c,'I',SETTINGS)
   put(GAME+4,'H',flags);put(GAME+0x76,'H',timer);put(GAME+0x10,'B',players)
   put(GAME+0x7c,'B',lives);put(INPUT+0x14,'H',mode)
   uc.reg_write(UC_ARM_REG_SP,0x03007e00);uc.reg_write(UC_ARM_REG_LR,0x087ff001)
   uc.emu_start(0x08002a7b,0x08002b54,count=1000)
   assert uc.reg_read(UC_ARM_REG_PC)==0x08002b54
   result=step(C.byref(life),C.byref(native_flags))
   assert (life.input_state,life.continue_flag,life.message_ticks,native_flags.value)==(
    get(INPUT+0x14,'H'),get(GAME+0x7c,'B'),get(GAME+0x76,'H'),get(GAME+4,'H'))
   assert result==(1 if mode==4 and lives else 2 if mode==4 else 3)
   if result!=1:
    assert get(SETTINGS+2,'H')==1 and get(SETTINGS+4,'H')==(14 if result==2 else 0)
    assert get(SETTINGS+8,'H')==65535 and not get(SETTINGS+6,'H') and not get(SETTINGS+0x19,'B')
   cases+=1
guards=0
for mode in (0,1,2,3,6,7,65535):
 life=Life(600,mode,1,1,0,3,0);flags=C.c_uint16(0xffff);before=bytes(life)
 assert step(C.byref(life),C.byref(flags))==-1 and bytes(life)==before and flags.value==0xffff;guards+=1
assert step(None,C.byref(flags))==-1 and step(C.byref(life),None)==-1;guards+=2
report={'result':'pass','original_exit_cases':cases,'guards':guards,'sanitizers':'UBSan',
 'rom_sha1':hashlib.sha1(rom).hexdigest(),'scope':__doc__,
 'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(r/'analysis/run-exit-rom-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='source_sha256'}))
